#include "pch.hpp"
#include "VulkanRenderer.hpp"

#include <algorithm>
#include <cstring>
#include <set>
#include <string>

namespace
{
constexpr std::array<const char*, 1> kValidationLayers{ "VK_LAYER_KHRONOS_validation" };
constexpr std::array<const char*, 1> kDeviceExtensions{ VK_KHR_SWAPCHAIN_EXTENSION_NAME };
constexpr bool kDebugToolsEnabled = TREXLAP2_DEBUG_TOOLS != 0;

/** Surfaceが対応する合成アルファ方式から、優先順位順に一つ選択する。 */
VkCompositeAlphaFlagBitsKHR ChooseCompositeAlpha(const VkSurfaceCapabilitiesKHR& capabilities)
{
	constexpr std::array<VkCompositeAlphaFlagBitsKHR, 4> choices{
		VK_COMPOSITE_ALPHA_OPAQUE_BIT_KHR, VK_COMPOSITE_ALPHA_PRE_MULTIPLIED_BIT_KHR,
		VK_COMPOSITE_ALPHA_POST_MULTIPLIED_BIT_KHR, VK_COMPOSITE_ALPHA_INHERIT_BIT_KHR };
	for (const auto choice : choices)
		if ((capabilities.supportedCompositeAlpha & choice) != 0) return choice;
	throw std::runtime_error("No supported swapchain composite alpha mode.");
}

/** 実行ファイルと同じディレクトリにあるSPIR-Vバイナリをuint32_t列として読み込む。 */
std::vector<std::uint32_t> ReadShaderFile(const wchar_t* fileName)
{
	std::array<wchar_t, MAX_PATH> executablePath{};
	const DWORD length = GetModuleFileNameW(nullptr, executablePath.data(), static_cast<DWORD>(executablePath.size()));
	if (length == 0 || length == executablePath.size()) throw std::runtime_error("Could not determine the executable directory.");

	const std::filesystem::path shaderPath = std::filesystem::path(executablePath.data()).parent_path() / fileName;
	std::ifstream file(shaderPath, std::ios::binary | std::ios::ate);
	if (!file) throw std::runtime_error("Could not open a compiled SPIR-V shader file.");
	const std::streamsize byteCount = file.tellg();
	if (byteCount <= 0 || (byteCount % static_cast<std::streamsize>(sizeof(std::uint32_t))) != 0)
		throw std::runtime_error("The SPIR-V shader file has an invalid size.");

	std::vector<std::uint32_t> code(static_cast<std::size_t>(byteCount) / sizeof(std::uint32_t));
	file.seekg(0);
	if (!file.read(reinterpret_cast<char*>(code.data()), byteCount)) throw std::runtime_error("Could not read a compiled SPIR-V shader file.");
	return code;
}
}

/** Vulkan Instanceから描画同期まで、最初のフレームに必要な資源を生成する。 */
VulkanRenderer::VulkanRenderer(HWND hwnd, std::uint32_t width, std::uint32_t height)
	: windowHandle_(hwnd), windowWidth_(width), windowHeight_(height)
{
	CreateInstance(); SetupDebugMessenger(); CreateSurface(hwnd); PickPhysicalDevice();
	CreateLogicalDevice(); CreateSwapchain(); CreateSwapchainImageViews();
	CreateGraphicsPipeline(); CreateCommandPool(); AllocateCommandBuffers(); CreateSyncObjects();
}

/** GPU完了後、生成順の逆順でVulkan資源を安全に破棄する。 */
VulkanRenderer::~VulkanRenderer()
{
	WaitUntilIdle(); DestroySyncObjects(); DestroySwapchainResources();
	if (graphicsPipelineLayout_ != VK_NULL_HANDLE) vkDestroyPipelineLayout(device_, graphicsPipelineLayout_, nullptr);
	if (commandPool_ != VK_NULL_HANDLE) vkDestroyCommandPool(device_, commandPool_, nullptr);
	if (device_ != VK_NULL_HANDLE) vkDestroyDevice(device_, nullptr);
	DestroyDebugUtilsMessenger();
	if (surface_ != VK_NULL_HANDLE) vkDestroySurfaceKHR(instance_, surface_, nullptr);
	if (instance_ != VK_NULL_HANDLE) vkDestroyInstance(instance_, nullptr);
}

/** 描画キューとPresentキューの両方が見つかっているか判定する。 */
bool VulkanRenderer::QueueFamilyIndices::IsComplete() const noexcept
{
	return graphicsFamily.has_value() && presentFamily.has_value();
}

/** Vulkan 1.3と、Win32表示・Debug診断に必要なInstance拡張を有効化する。 */
void VulkanRenderer::CreateInstance()
{
	if constexpr (kDebugToolsEnabled)
	{
		if (!CheckValidationLayerSupport()) throw std::runtime_error("VK_LAYER_KHRONOS_validation is unavailable.");
	}
	VkApplicationInfo app{ VK_STRUCTURE_TYPE_APPLICATION_INFO };
	app.pApplicationName = "TRExLap2-on-SRPG";
	app.applicationVersion = VK_MAKE_API_VERSION(0, 0, 1, 0);
	app.pEngineName = "TRExLap2";
	app.engineVersion = VK_MAKE_API_VERSION(0, 0, 1, 0);
	app.apiVersion = VK_API_VERSION_1_3;
	const auto extensions = GetRequiredInstanceExtensions();
	VkInstanceCreateInfo info{ VK_STRUCTURE_TYPE_INSTANCE_CREATE_INFO };
	info.pApplicationInfo = &app;
	info.enabledExtensionCount = static_cast<std::uint32_t>(extensions.size());
	info.ppEnabledExtensionNames = extensions.data();
	const auto debugInfo = MakeDebugMessengerCreateInfo();
	if constexpr (kDebugToolsEnabled)
	{
		info.enabledLayerCount = static_cast<std::uint32_t>(kValidationLayers.size());
		info.ppEnabledLayerNames = kValidationLayers.data();
		info.pNext = &debugInfo;
	}
	VK_CHECK(vkCreateInstance(&info, nullptr, &instance_));
}

/** Debug構成でのみ、Validation Layerのメッセージ受信先を生成する。 */
void VulkanRenderer::SetupDebugMessenger()
{
	if constexpr (kDebugToolsEnabled)
	{
		const auto info = MakeDebugMessengerCreateInfo();
		VK_CHECK(CreateDebugUtilsMessenger(&info));
	}
}

/** 指定したWin32ウィンドウをVulkanの表示Surfaceへ変換する。 */
void VulkanRenderer::CreateSurface(HWND hwnd)
{
	VkWin32SurfaceCreateInfoKHR info{ VK_STRUCTURE_TYPE_WIN32_SURFACE_CREATE_INFO_KHR };
	info.hinstance = GetModuleHandleW(nullptr);
	info.hwnd = hwnd;
	VK_CHECK(vkCreateWin32SurfaceKHR(instance_, &info, nullptr, &surface_));
}

/** 描画・表示・Swapchain・Vulkan 1.3機能を満たす最初のGPUを採用する。 */
void VulkanRenderer::PickPhysicalDevice()
{
	std::uint32_t count = 0;
	VK_CHECK(vkEnumeratePhysicalDevices(instance_, &count, nullptr));
	if (count == 0) throw std::runtime_error("No Vulkan-compatible GPU was found.");
	std::vector<VkPhysicalDevice> devices(count);
	VK_CHECK(vkEnumeratePhysicalDevices(instance_, &count, devices.data()));
	for (const auto device : devices)
	{
		if (IsDeviceSuitable(device)) { physicalDevice_ = device; return; }
	}
	throw std::runtime_error("No GPU supports Vulkan 1.3 dynamic rendering and presentation.");
}

/** 描画・表示・Dynamic Rendering・Swapchainを満たす物理GPUか検証する。 */
bool VulkanRenderer::IsDeviceSuitable(VkPhysicalDevice device) const
{
	if (!FindQueueFamilies(device).IsComplete() || !CheckDeviceExtensionSupport(device)) return false;
	const auto support = QuerySwapchainSupport(device);
	if (support.formats.empty() || support.presentModes.empty()) return false;
	VkPhysicalDeviceVulkan13Features features13{ VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_VULKAN_1_3_FEATURES };
	VkPhysicalDeviceFeatures2 features2{ VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_FEATURES_2 };
	features2.pNext = &features13;
	vkGetPhysicalDeviceFeatures2(device, &features2);
	return features13.dynamicRendering == VK_TRUE && features13.synchronization2 == VK_TRUE;
}

/** 必須のDevice拡張が物理GPUで利用可能か検証する。 */
bool VulkanRenderer::CheckDeviceExtensionSupport(VkPhysicalDevice device) const
{
	std::uint32_t count = 0;
	VK_CHECK(vkEnumerateDeviceExtensionProperties(device, nullptr, &count, nullptr));
	std::vector<VkExtensionProperties> available(count);
	VK_CHECK(vkEnumerateDeviceExtensionProperties(device, nullptr, &count, available.data()));
	std::set<std::string> required(kDeviceExtensions.begin(), kDeviceExtensions.end());
	for (const auto& extension : available) required.erase(extension.extensionName);
	return required.empty();
}

/** 描画可能かつSurface提示可能なキューファミリの番号を探索する。 */
VulkanRenderer::QueueFamilyIndices VulkanRenderer::FindQueueFamilies(VkPhysicalDevice device) const
{
	QueueFamilyIndices result{};
	std::uint32_t count = 0;
	vkGetPhysicalDeviceQueueFamilyProperties(device, &count, nullptr);
	std::vector<VkQueueFamilyProperties> families(count);
	vkGetPhysicalDeviceQueueFamilyProperties(device, &count, families.data());
	for (std::uint32_t index = 0; index < count; ++index)
	{
		if ((families[index].queueFlags & VK_QUEUE_GRAPHICS_BIT) != 0) result.graphicsFamily = index;
		VkBool32 present = VK_FALSE;
		VK_CHECK(vkGetPhysicalDeviceSurfaceSupportKHR(device, index, surface_, &present));
		if (present == VK_TRUE) result.presentFamily = index;
		if (result.IsComplete()) break;
	}
	return result;
}

/** 物理GPUとSurfaceの組み合わせで利用可能なSwapchain条件を問い合わせる。 */
VulkanRenderer::SwapchainSupportDetails VulkanRenderer::QuerySwapchainSupport(VkPhysicalDevice device) const
{
	SwapchainSupportDetails result{};
	VK_CHECK(vkGetPhysicalDeviceSurfaceCapabilitiesKHR(device, surface_, &result.capabilities));
	std::uint32_t count = 0;
	VK_CHECK(vkGetPhysicalDeviceSurfaceFormatsKHR(device, surface_, &count, nullptr));
	result.formats.resize(count);
	if (count > 0) VK_CHECK(vkGetPhysicalDeviceSurfaceFormatsKHR(device, surface_, &count, result.formats.data()));
	count = 0;
	VK_CHECK(vkGetPhysicalDeviceSurfacePresentModesKHR(device, surface_, &count, nullptr));
	result.presentModes.resize(count);
	if (count > 0) VK_CHECK(vkGetPhysicalDeviceSurfacePresentModesKHR(device, surface_, &count, result.presentModes.data()));
	return result;
}

/** Dynamic RenderingとSynchronization 2を必須のDevice機能として有効化する。 */
void VulkanRenderer::CreateLogicalDevice()
{
	const auto families = FindQueueFamilies(physicalDevice_);
	graphicsQueueFamily_ = *families.graphicsFamily;
	presentQueueFamily_ = *families.presentFamily;
	const float priority = 1.0f;
	std::set<std::uint32_t> familySet{ graphicsQueueFamily_, presentQueueFamily_ };
	std::vector<VkDeviceQueueCreateInfo> queueInfos;
	for (const auto family : familySet)
	{
		VkDeviceQueueCreateInfo info{ VK_STRUCTURE_TYPE_DEVICE_QUEUE_CREATE_INFO };
		info.queueFamilyIndex = family; info.queueCount = 1; info.pQueuePriorities = &priority;
		queueInfos.push_back(info);
	}
	VkPhysicalDeviceVulkan13Features features13{ VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_VULKAN_1_3_FEATURES };
	features13.dynamicRendering = VK_TRUE;
	features13.synchronization2 = VK_TRUE;
	VkDeviceCreateInfo info{ VK_STRUCTURE_TYPE_DEVICE_CREATE_INFO };
	info.pNext = &features13;
	info.queueCreateInfoCount = static_cast<std::uint32_t>(queueInfos.size());
	info.pQueueCreateInfos = queueInfos.data();
	info.enabledExtensionCount = static_cast<std::uint32_t>(kDeviceExtensions.size());
	info.ppEnabledExtensionNames = kDeviceExtensions.data();
	VK_CHECK(vkCreateDevice(physicalDevice_, &info, nullptr, &device_));
	vkGetDeviceQueue(device_, graphicsQueueFamily_, 0, &graphicsQueue_);
	vkGetDeviceQueue(device_, presentQueueFamily_, 0, &presentQueue_);
}

/** Swapchain画像はAcquire後からPresent前だけ、アプリ側が描画に使用できる。 */
void VulkanRenderer::CreateSwapchain()
{
	const auto support = QuerySwapchainSupport(physicalDevice_);
	const auto format = ChooseSwapSurfaceFormat(support.formats);
	const auto presentMode = ChooseSwapchainPresentMode(support.presentModes);
	const auto extent = ChooseSwapchainExtent(support.capabilities);
	std::uint32_t imageCount = support.capabilities.minImageCount + 1;
	if (support.capabilities.maxImageCount > 0 && imageCount > support.capabilities.maxImageCount) imageCount = support.capabilities.maxImageCount;
	VkSwapchainCreateInfoKHR info{ VK_STRUCTURE_TYPE_SWAPCHAIN_CREATE_INFO_KHR };
	info.surface = surface_; info.minImageCount = imageCount; info.imageFormat = format.format;
	info.imageColorSpace = format.colorSpace; info.imageExtent = extent; info.imageArrayLayers = 1;
	info.imageUsage = VK_IMAGE_USAGE_COLOR_ATTACHMENT_BIT;
	const std::uint32_t families[]{ graphicsQueueFamily_, presentQueueFamily_ };
	if (graphicsQueueFamily_ != presentQueueFamily_)
	{
		info.imageSharingMode = VK_SHARING_MODE_CONCURRENT; info.queueFamilyIndexCount = 2; info.pQueueFamilyIndices = families;
	}
	else info.imageSharingMode = VK_SHARING_MODE_EXCLUSIVE;
	info.preTransform = support.capabilities.currentTransform;
	info.compositeAlpha = ChooseCompositeAlpha(support.capabilities);
	info.presentMode = presentMode; info.clipped = VK_TRUE;
	VK_CHECK(vkCreateSwapchainKHR(device_, &info, nullptr, &swapchain_));
	VK_CHECK(vkGetSwapchainImagesKHR(device_, swapchain_, &imageCount, nullptr));
	swapchainImages_.resize(imageCount);
	VK_CHECK(vkGetSwapchainImagesKHR(device_, swapchain_, &imageCount, swapchainImages_.data()));
	imagesInFlight_.assign(imageCount, VK_NULL_HANDLE);
	swapchainImageFormat_ = format.format; swapchainExtent_ = extent;
}

/** 各Swapchain画像をDynamic RenderingのColor Attachmentとして参照できるImageViewにする。 */
void VulkanRenderer::CreateSwapchainImageViews()
{
	swapchainImageViews_.resize(swapchainImages_.size());
	for (std::size_t index = 0; index < swapchainImages_.size(); ++index)
	{
		VkImageViewCreateInfo info{ VK_STRUCTURE_TYPE_IMAGE_VIEW_CREATE_INFO };
		info.image = swapchainImages_[index]; info.viewType = VK_IMAGE_VIEW_TYPE_2D; info.format = swapchainImageFormat_;
		info.components = { VK_COMPONENT_SWIZZLE_IDENTITY, VK_COMPONENT_SWIZZLE_IDENTITY, VK_COMPONENT_SWIZZLE_IDENTITY, VK_COMPONENT_SWIZZLE_IDENTITY };
		info.subresourceRange.aspectMask = VK_IMAGE_ASPECT_COLOR_BIT; info.subresourceRange.levelCount = 1; info.subresourceRange.layerCount = 1;
		VK_CHECK(vkCreateImageView(device_, &info, nullptr, &swapchainImageViews_[index]));
	}
}

/** 三角形のSPIR-VはVisual Studioのカスタムビルドでexeと同じ場所へ出力される。 */
void VulkanRenderer::CreateGraphicsPipeline()
{
	const VkShaderModule vertexShader = CreateShaderModule(ReadShaderFile(L"Triangle.vert.spv"));
	const VkShaderModule fragmentShader = CreateShaderModule(ReadShaderFile(L"Triangle.frag.spv"));

	VkPipelineShaderStageCreateInfo vertexStage{ VK_STRUCTURE_TYPE_PIPELINE_SHADER_STAGE_CREATE_INFO };
	vertexStage.stage = VK_SHADER_STAGE_VERTEX_BIT; vertexStage.module = vertexShader; vertexStage.pName = "main";
	VkPipelineShaderStageCreateInfo fragmentStage{ VK_STRUCTURE_TYPE_PIPELINE_SHADER_STAGE_CREATE_INFO };
	fragmentStage.stage = VK_SHADER_STAGE_FRAGMENT_BIT; fragmentStage.module = fragmentShader; fragmentStage.pName = "main";
	const VkPipelineShaderStageCreateInfo stages[]{ vertexStage, fragmentStage };

	VkPipelineVertexInputStateCreateInfo vertexInput{ VK_STRUCTURE_TYPE_PIPELINE_VERTEX_INPUT_STATE_CREATE_INFO };
	/* 頂点属性は未使用。頂点シェーダがgl_VertexIndexから座標と色を直接決定する。 */
	VkPipelineInputAssemblyStateCreateInfo inputAssembly{ VK_STRUCTURE_TYPE_PIPELINE_INPUT_ASSEMBLY_STATE_CREATE_INFO };
	inputAssembly.topology = VK_PRIMITIVE_TOPOLOGY_TRIANGLE_LIST;
	VkPipelineViewportStateCreateInfo viewportState{ VK_STRUCTURE_TYPE_PIPELINE_VIEWPORT_STATE_CREATE_INFO };
	viewportState.viewportCount = 1; viewportState.scissorCount = 1;
	VkPipelineRasterizationStateCreateInfo rasterizer{ VK_STRUCTURE_TYPE_PIPELINE_RASTERIZATION_STATE_CREATE_INFO };
	rasterizer.polygonMode = VK_POLYGON_MODE_FILL; rasterizer.cullMode = VK_CULL_MODE_NONE;
	rasterizer.frontFace = VK_FRONT_FACE_COUNTER_CLOCKWISE; rasterizer.lineWidth = 1.0f;
	VkPipelineMultisampleStateCreateInfo multisampling{ VK_STRUCTURE_TYPE_PIPELINE_MULTISAMPLE_STATE_CREATE_INFO };
	multisampling.rasterizationSamples = VK_SAMPLE_COUNT_1_BIT;
	VkPipelineColorBlendAttachmentState colorBlendAttachment{};
	colorBlendAttachment.colorWriteMask = VK_COLOR_COMPONENT_R_BIT | VK_COLOR_COMPONENT_G_BIT | VK_COLOR_COMPONENT_B_BIT | VK_COLOR_COMPONENT_A_BIT;
	VkPipelineColorBlendStateCreateInfo colorBlending{ VK_STRUCTURE_TYPE_PIPELINE_COLOR_BLEND_STATE_CREATE_INFO };
	colorBlending.attachmentCount = 1; colorBlending.pAttachments = &colorBlendAttachment;
	const VkDynamicState dynamicStates[]{ VK_DYNAMIC_STATE_VIEWPORT, VK_DYNAMIC_STATE_SCISSOR };
	VkPipelineDynamicStateCreateInfo dynamicState{ VK_STRUCTURE_TYPE_PIPELINE_DYNAMIC_STATE_CREATE_INFO };
	dynamicState.dynamicStateCount = static_cast<std::uint32_t>(std::size(dynamicStates)); dynamicState.pDynamicStates = dynamicStates;
	VkPipelineRenderingCreateInfo renderingInfo{ VK_STRUCTURE_TYPE_PIPELINE_RENDERING_CREATE_INFO };
	renderingInfo.colorAttachmentCount = 1; renderingInfo.pColorAttachmentFormats = &swapchainImageFormat_;

	if (graphicsPipelineLayout_ == VK_NULL_HANDLE)
	{
		VkPipelineLayoutCreateInfo layoutInfo{ VK_STRUCTURE_TYPE_PIPELINE_LAYOUT_CREATE_INFO };
		VK_CHECK(vkCreatePipelineLayout(device_, &layoutInfo, nullptr, &graphicsPipelineLayout_));
	}
	VkGraphicsPipelineCreateInfo pipelineInfo{ VK_STRUCTURE_TYPE_GRAPHICS_PIPELINE_CREATE_INFO };
	pipelineInfo.pNext = &renderingInfo; pipelineInfo.stageCount = static_cast<std::uint32_t>(std::size(stages)); pipelineInfo.pStages = stages;
	pipelineInfo.pVertexInputState = &vertexInput; pipelineInfo.pInputAssemblyState = &inputAssembly;
	pipelineInfo.pViewportState = &viewportState; pipelineInfo.pRasterizationState = &rasterizer;
	pipelineInfo.pMultisampleState = &multisampling; pipelineInfo.pColorBlendState = &colorBlending;
	pipelineInfo.pDynamicState = &dynamicState; pipelineInfo.layout = graphicsPipelineLayout_;
	const VkResult result = vkCreateGraphicsPipelines(device_, VK_NULL_HANDLE, 1, &pipelineInfo, nullptr, &graphicsPipeline_);
	vkDestroyShaderModule(device_, fragmentShader, nullptr);
	vkDestroyShaderModule(device_, vertexShader, nullptr);
	VK_CHECK(result);
}

/** 読み込んだSPIR-Vコードから一時的なVulkan Shader Moduleを生成する。 */
VkShaderModule VulkanRenderer::CreateShaderModule(const std::vector<std::uint32_t>& code) const
{
	VkShaderModuleCreateInfo info{ VK_STRUCTURE_TYPE_SHADER_MODULE_CREATE_INFO };
	info.codeSize = code.size() * sizeof(std::uint32_t); info.pCode = code.data();
	VkShaderModule shader = VK_NULL_HANDLE;
	VK_CHECK(vkCreateShaderModule(device_, &info, nullptr, &shader));
	return shader;
}

/** Graphics Queue用コマンドバッファを個別リセット可能なCommand Poolを生成する。 */
void VulkanRenderer::CreateCommandPool()
{
	VkCommandPoolCreateInfo info{ VK_STRUCTURE_TYPE_COMMAND_POOL_CREATE_INFO };
	info.flags = VK_COMMAND_POOL_CREATE_RESET_COMMAND_BUFFER_BIT; info.queueFamilyIndex = graphicsQueueFamily_;
	VK_CHECK(vkCreateCommandPool(device_, &info, nullptr, &commandPool_));
}

/** フレーム並列数と同数のPrimary Command Bufferを確保する。 */
void VulkanRenderer::AllocateCommandBuffers()
{
	commandBuffers_.resize(kMaxFramesInFlight);
	VkCommandBufferAllocateInfo info{ VK_STRUCTURE_TYPE_COMMAND_BUFFER_ALLOCATE_INFO };
	info.commandPool = commandPool_; info.level = VK_COMMAND_BUFFER_LEVEL_PRIMARY; info.commandBufferCount = kMaxFramesInFlight;
	VK_CHECK(vkAllocateCommandBuffers(device_, &info, commandBuffers_.data()));
}

/** Acquire・描画完了・CPU待機を同期するSemaphoreとFenceをフレームごとに生成する。 */
void VulkanRenderer::CreateSyncObjects()
{
	VkSemaphoreCreateInfo semaphoreInfo{ VK_STRUCTURE_TYPE_SEMAPHORE_CREATE_INFO };
	VkFenceCreateInfo fenceInfo{ VK_STRUCTURE_TYPE_FENCE_CREATE_INFO };
	fenceInfo.flags = VK_FENCE_CREATE_SIGNALED_BIT;
	for (auto& frame : frameSyncs_)
	{
		VK_CHECK(vkCreateSemaphore(device_, &semaphoreInfo, nullptr, &frame.imageAvailable));
		VK_CHECK(vkCreateSemaphore(device_, &semaphoreInfo, nullptr, &frame.renderFinished));
		VK_CHECK(vkCreateFence(device_, &fenceInfo, nullptr, &frame.inFlight));
	}
}

/** 同じフレーム用同期オブジェクトをGPU完了前に再利用しないためFenceを待機する。 */
void VulkanRenderer::DrawFrame()
{
	FrameSync& frame = frameSyncs_[currentFrame_];
	VK_CHECK(vkWaitForFences(device_, 1, &frame.inFlight, VK_TRUE, UINT64_MAX));
	std::uint32_t imageIndex = 0;
	const VkResult acquired = vkAcquireNextImageKHR(device_, swapchain_, UINT64_MAX, frame.imageAvailable, VK_NULL_HANDLE, &imageIndex);
	if (acquired == VK_ERROR_OUT_OF_DATE_KHR) return;
	if (acquired != VK_SUCCESS && acquired != VK_SUBOPTIMAL_KHR) VK_CHECK(acquired);
	/* 取得画像が過去の未完了フレームに属する場合も、そのFenceを待機する。 */
	if (imagesInFlight_[imageIndex] != VK_NULL_HANDLE) VK_CHECK(vkWaitForFences(device_, 1, &imagesInFlight_[imageIndex], VK_TRUE, UINT64_MAX));
	imagesInFlight_[imageIndex] = frame.inFlight;
	VK_CHECK(vkResetFences(device_, 1, &frame.inFlight));
	VK_CHECK(vkResetCommandBuffer(commandBuffers_[currentFrame_], 0));
	RecordCommandBuffer(commandBuffers_[currentFrame_], imageIndex);

	VkSemaphoreSubmitInfo wait{ VK_STRUCTURE_TYPE_SEMAPHORE_SUBMIT_INFO };
	wait.semaphore = frame.imageAvailable; wait.stageMask = VK_PIPELINE_STAGE_2_COLOR_ATTACHMENT_OUTPUT_BIT;
	VkCommandBufferSubmitInfo command{ VK_STRUCTURE_TYPE_COMMAND_BUFFER_SUBMIT_INFO };
	command.commandBuffer = commandBuffers_[currentFrame_];
	VkSemaphoreSubmitInfo signal{ VK_STRUCTURE_TYPE_SEMAPHORE_SUBMIT_INFO };
	signal.semaphore = frame.renderFinished; signal.stageMask = VK_PIPELINE_STAGE_2_ALL_GRAPHICS_BIT;
	VkSubmitInfo2 submit{ VK_STRUCTURE_TYPE_SUBMIT_INFO_2 };
	submit.waitSemaphoreInfoCount = 1; submit.pWaitSemaphoreInfos = &wait;
	submit.commandBufferInfoCount = 1; submit.pCommandBufferInfos = &command;
	submit.signalSemaphoreInfoCount = 1; submit.pSignalSemaphoreInfos = &signal;
	VK_CHECK(vkQueueSubmit2(graphicsQueue_, 1, &submit, frame.inFlight));

	VkPresentInfoKHR present{ VK_STRUCTURE_TYPE_PRESENT_INFO_KHR };
	present.waitSemaphoreCount = 1; present.pWaitSemaphores = &frame.renderFinished;
	present.swapchainCount = 1; present.pSwapchains = &swapchain_; present.pImageIndices = &imageIndex;
	const VkResult result = vkQueuePresentKHR(presentQueue_, &present);
	if (result != VK_SUCCESS && result != VK_SUBOPTIMAL_KHR && result != VK_ERROR_OUT_OF_DATE_KHR) VK_CHECK(result);
	currentFrame_ = (currentFrame_ + 1) % kMaxFramesInFlight;
}

/**
 * @brief スワップチェーン画像を濃紺でクリアし、三角形を記録する。
 *
 * 画面内容は毎回破棄するため、古い内容を読まないUNDEFINEDから遷移する。
 */
void VulkanRenderer::RecordCommandBuffer(VkCommandBuffer commandBuffer, std::uint32_t imageIndex)
{
	VkCommandBufferBeginInfo begin{ VK_STRUCTURE_TYPE_COMMAND_BUFFER_BEGIN_INFO };
	begin.flags = VK_COMMAND_BUFFER_USAGE_ONE_TIME_SUBMIT_BIT;
	VK_CHECK(vkBeginCommandBuffer(commandBuffer, &begin));
	VkImageMemoryBarrier2 toColor{ VK_STRUCTURE_TYPE_IMAGE_MEMORY_BARRIER_2 };
	toColor.dstStageMask = VK_PIPELINE_STAGE_2_COLOR_ATTACHMENT_OUTPUT_BIT;
	toColor.dstAccessMask = VK_ACCESS_2_COLOR_ATTACHMENT_WRITE_BIT;
	toColor.oldLayout = VK_IMAGE_LAYOUT_UNDEFINED; toColor.newLayout = VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL;
	toColor.image = swapchainImages_[imageIndex]; toColor.subresourceRange.aspectMask = VK_IMAGE_ASPECT_COLOR_BIT;
	toColor.subresourceRange.levelCount = 1; toColor.subresourceRange.layerCount = 1;
	VkDependencyInfo dependency{ VK_STRUCTURE_TYPE_DEPENDENCY_INFO };
	dependency.imageMemoryBarrierCount = 1; dependency.pImageMemoryBarriers = &toColor;
	vkCmdPipelineBarrier2(commandBuffer, &dependency);
	VkRenderingAttachmentInfo color{ VK_STRUCTURE_TYPE_RENDERING_ATTACHMENT_INFO };
	color.imageView = swapchainImageViews_[imageIndex]; color.imageLayout = VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL;
	color.loadOp = VK_ATTACHMENT_LOAD_OP_CLEAR; color.storeOp = VK_ATTACHMENT_STORE_OP_STORE;
	color.clearValue.color = { { 0.035f, 0.055f, 0.100f, 1.0f } };
	VkRenderingInfo rendering{ VK_STRUCTURE_TYPE_RENDERING_INFO };
	rendering.renderArea.extent = swapchainExtent_; rendering.layerCount = 1;
	rendering.colorAttachmentCount = 1; rendering.pColorAttachments = &color;
	/* Dynamic RenderingではRenderPass/Framebufferを生成せず、対象ImageViewを直接指定する。 */
	vkCmdBeginRendering(commandBuffer, &rendering);
	VkViewport viewport{};
	viewport.width = static_cast<float>(swapchainExtent_.width); viewport.height = static_cast<float>(swapchainExtent_.height); viewport.maxDepth = 1.0f;
	VkRect2D scissor{};
	scissor.extent = swapchainExtent_;
	vkCmdSetViewport(commandBuffer, 0, 1, &viewport);
	vkCmdSetScissor(commandBuffer, 0, 1, &scissor);
	vkCmdBindPipeline(commandBuffer, VK_PIPELINE_BIND_POINT_GRAPHICS, graphicsPipeline_);
	vkCmdDraw(commandBuffer, 3, 1, 0, 0);
	vkCmdEndRendering(commandBuffer);
	VkImageMemoryBarrier2 toPresent{ VK_STRUCTURE_TYPE_IMAGE_MEMORY_BARRIER_2 };
	toPresent.srcStageMask = VK_PIPELINE_STAGE_2_COLOR_ATTACHMENT_OUTPUT_BIT;
	toPresent.srcAccessMask = VK_ACCESS_2_COLOR_ATTACHMENT_WRITE_BIT;
	toPresent.oldLayout = VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL; toPresent.newLayout = VK_IMAGE_LAYOUT_PRESENT_SRC_KHR;
	toPresent.image = swapchainImages_[imageIndex]; toPresent.subresourceRange.aspectMask = VK_IMAGE_ASPECT_COLOR_BIT;
	toPresent.subresourceRange.levelCount = 1; toPresent.subresourceRange.layerCount = 1;
	dependency.pImageMemoryBarriers = &toPresent;
	vkCmdPipelineBarrier2(commandBuffer, &dependency);
	VK_CHECK(vkEndCommandBuffer(commandBuffer));
}

/** 最小化時の0x0サイズではSwapchainを作れないため、復元時のWM_SIZEまで待機する。 */
void VulkanRenderer::RecreateSwapchain(std::uint32_t width, std::uint32_t height)
{
	if (width == 0 || height == 0) return;
	WaitUntilIdle(); windowWidth_ = width; windowHeight_ = height;
	DestroySwapchainResources(); CreateSwapchain(); CreateSwapchainImageViews(); CreateGraphicsPipeline();
}

/** 全GPU処理の完了を待機し、資源破棄またはSwapchain再生成を可能にする。 */
void VulkanRenderer::WaitUntilIdle() const
{
	if (device_ != VK_NULL_HANDLE) VK_CHECK(vkDeviceWaitIdle(device_));
}

/** Swapchain本体より先に、その画像を参照するImageViewを破棄する。 */
void VulkanRenderer::DestroySwapchainResources()
{
	if (graphicsPipeline_ != VK_NULL_HANDLE) vkDestroyPipeline(device_, graphicsPipeline_, nullptr);
	graphicsPipeline_ = VK_NULL_HANDLE;
	for (const auto view : swapchainImageViews_) vkDestroyImageView(device_, view, nullptr);
	swapchainImageViews_.clear(); swapchainImages_.clear(); imagesInFlight_.clear();
	if (swapchain_ != VK_NULL_HANDLE) vkDestroySwapchainKHR(device_, swapchain_, nullptr);
	swapchain_ = VK_NULL_HANDLE;
}

/** フレームごとのSemaphoreとFenceを破棄し、ハンドルを初期化する。 */
void VulkanRenderer::DestroySyncObjects()
{
	if (device_ == VK_NULL_HANDLE) return;
	for (auto& frame : frameSyncs_)
	{
		if (frame.imageAvailable != VK_NULL_HANDLE) vkDestroySemaphore(device_, frame.imageAvailable, nullptr);
		if (frame.renderFinished != VK_NULL_HANDLE) vkDestroySemaphore(device_, frame.renderFinished, nullptr);
		if (frame.inFlight != VK_NULL_HANDLE) vkDestroyFence(device_, frame.inFlight, nullptr);
		frame = {};
	}
}

/** sRGB BGRAを優先し、利用できなければSurfaceが提示した先頭形式を選ぶ。 */
VkSurfaceFormatKHR VulkanRenderer::ChooseSwapSurfaceFormat(const std::vector<VkSurfaceFormatKHR>& formats) const
{
	for (const auto& format : formats)
		if (format.format == VK_FORMAT_B8G8R8A8_SRGB && format.colorSpace == VK_COLOR_SPACE_SRGB_NONLINEAR_KHR) return format;
	return formats.front();
}

/** 低遅延のMAILBOXを優先し、必須のFIFOへフォールバックする。 */
VkPresentModeKHR VulkanRenderer::ChooseSwapchainPresentMode(const std::vector<VkPresentModeKHR>& modes) const
{
	for (const auto mode : modes) if (mode == VK_PRESENT_MODE_MAILBOX_KHR) return mode;
	return VK_PRESENT_MODE_FIFO_KHR;
}

/** Surface指定サイズまたは現在のクライアントサイズからSwapchain寸法を決定する。 */
VkExtent2D VulkanRenderer::ChooseSwapchainExtent(const VkSurfaceCapabilitiesKHR& capabilities) const
{
	if (capabilities.currentExtent.width != UINT32_MAX) return capabilities.currentExtent;
	return { std::clamp(windowWidth_, capabilities.minImageExtent.width, capabilities.maxImageExtent.width),
		std::clamp(windowHeight_, capabilities.minImageExtent.height, capabilities.maxImageExtent.height) };
}

/** Win32表示とDebug診断に必要なVulkan Instance拡張名を列挙する。 */
std::vector<const char*> VulkanRenderer::GetRequiredInstanceExtensions() const
{
	std::vector<const char*> result{ VK_KHR_SURFACE_EXTENSION_NAME, VK_KHR_WIN32_SURFACE_EXTENSION_NAME };
	if constexpr (kDebugToolsEnabled) result.push_back(VK_EXT_DEBUG_UTILS_EXTENSION_NAME);
	return result;
}

/** Debug構成で要求するKhronos Validation Layerがインストール済みか検証する。 */
bool VulkanRenderer::CheckValidationLayerSupport() const
{
	std::uint32_t count = 0; VK_CHECK(vkEnumerateInstanceLayerProperties(&count, nullptr));
	std::vector<VkLayerProperties> layers(count); VK_CHECK(vkEnumerateInstanceLayerProperties(&count, layers.data()));
	for (const auto* required : kValidationLayers)
	{
		bool found = false;
		for (const auto& layer : layers) if (std::strcmp(required, layer.layerName) == 0) { found = true; break; }
		if (!found) return false;
	}
	return true;
}

/** Instance生成時と生成後で共用するDebug Messenger設定を組み立てる。 */
VkDebugUtilsMessengerCreateInfoEXT VulkanRenderer::MakeDebugMessengerCreateInfo()
{
	VkDebugUtilsMessengerCreateInfoEXT info{ VK_STRUCTURE_TYPE_DEBUG_UTILS_MESSENGER_CREATE_INFO_EXT };
	info.messageSeverity = VK_DEBUG_UTILS_MESSAGE_SEVERITY_WARNING_BIT_EXT | VK_DEBUG_UTILS_MESSAGE_SEVERITY_ERROR_BIT_EXT;
	info.messageType = VK_DEBUG_UTILS_MESSAGE_TYPE_GENERAL_BIT_EXT | VK_DEBUG_UTILS_MESSAGE_TYPE_VALIDATION_BIT_EXT | VK_DEBUG_UTILS_MESSAGE_TYPE_PERFORMANCE_BIT_EXT;
	info.pfnUserCallback = DebugCallback;
	return info;
}

/** Vulkan Validation Layerの警告・エラーをVisual Studio出力へ転送する。 */
VKAPI_ATTR VkBool32 VKAPI_CALL VulkanRenderer::DebugCallback(VkDebugUtilsMessageSeverityFlagBitsEXT, VkDebugUtilsMessageTypeFlagsEXT, const VkDebugUtilsMessengerCallbackDataEXT* data, void*)
{
	OutputDebugStringA("[Vulkan] "); OutputDebugStringA(data->pMessage); OutputDebugStringA("\n");
	return VK_FALSE;
}

/** Instance拡張関数を取得してDebug Messengerを生成する。 */
VkResult VulkanRenderer::CreateDebugUtilsMessenger(const VkDebugUtilsMessengerCreateInfoEXT* info)
{
	auto function = reinterpret_cast<PFN_vkCreateDebugUtilsMessengerEXT>(vkGetInstanceProcAddr(instance_, "vkCreateDebugUtilsMessengerEXT"));
	return function == nullptr ? VK_ERROR_EXTENSION_NOT_PRESENT : function(instance_, info, nullptr, &debugMessenger_);
}

/** 生成済みDebug Messengerを拡張関数経由で破棄する。 */
void VulkanRenderer::DestroyDebugUtilsMessenger()
{
	if (debugMessenger_ == VK_NULL_HANDLE || instance_ == VK_NULL_HANDLE) return;
	auto function = reinterpret_cast<PFN_vkDestroyDebugUtilsMessengerEXT>(vkGetInstanceProcAddr(instance_, "vkDestroyDebugUtilsMessengerEXT"));
	if (function != nullptr) function(instance_, debugMessenger_, nullptr);
	debugMessenger_ = VK_NULL_HANDLE;
}
