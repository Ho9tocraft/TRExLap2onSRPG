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
constexpr char kDlssProjectId[] = "c3dfa68e-460d-4c4a-bc9c-95fc87545f7a";
constexpr char kDlssEngineVersion[] = "TRExLap2-0.1";

/// <summary>
/// 実行ファイルの配置ディレクトリを取得し、実行時資産とログの基準にする。
/// </summary>
std::filesystem::path GetExecutableDirectory()
{
	std::vector<wchar_t> pathBuffer(MAX_PATH);
	while (true)
	{
		const DWORD copiedLength = GetModuleFileNameW(nullptr, pathBuffer.data(), static_cast<DWORD>(pathBuffer.size()));
		if (copiedLength == 0) throw std::runtime_error("GetModuleFileNameW failed.");
		if (copiedLength < pathBuffer.size() - 1) return std::filesystem::path(pathBuffer.data()).parent_path();
		pathBuffer.resize(pathBuffer.size() * 2);
	}
}

/// <summary>
/// VulkanとDLSSの実行診断を単体exe横に固定して記録するパスを返す。
/// </summary>
std::filesystem::path GetValidationLogPath()
{
	return GetExecutableDirectory() / L"VulkanValidation.log";
}

/// <summary>
/// DLSS Super Samplingの必要拡張問い合わせに使うNGX Feature Discovery情報を組み立てる。
/// </summary>
NVSDK_NGX_FeatureDiscoveryInfo MakeDlssFeatureDiscoveryInfo()
{
	static const std::wstring applicationDataPath = GetExecutableDirectory().wstring();
	NVSDK_NGX_FeatureDiscoveryInfo info{};
	info.SDKVersion = NVSDK_NGX_Version_API;
	info.FeatureID = NVSDK_NGX_Feature_SuperSampling;
	info.Identifier.IdentifierType = NVSDK_NGX_Application_Identifier_Type_Project_Id;
	info.Identifier.v.ProjectDesc = { kDlssProjectId, NVSDK_NGX_ENGINE_TYPE_CUSTOM, kDlssEngineVersion };
	info.ApplicationDataPath = applicationDataPath.c_str();
	return info;
}

/// <summary>
/// Debug実行開始時に前回のVulkan Validationログを空にする。
/// </summary>
void ResetValidationLog()
{
	if constexpr (kDebugToolsEnabled) std::ofstream(GetValidationLogPath(), std::ios::trunc);
}

/// <summary>
/// Vulkan Validationの警告またはエラーを検証用ログへ追記する。
/// </summary>
void AppendValidationLog(const char* message)
{
	if constexpr (kDebugToolsEnabled)
	{
		std::ofstream log(GetValidationLogPath(), std::ios::app);
		if (log) log << message << '\n';
	}
}

/// <summary>
/// Surfaceが対応する合成アルファ方式から、優先順位順に一つ選択する。
/// </summary>
VkCompositeAlphaFlagBitsKHR ChooseCompositeAlpha(const VkSurfaceCapabilitiesKHR& capabilities)
{
	constexpr std::array<VkCompositeAlphaFlagBitsKHR, 4> choices{
		VK_COMPOSITE_ALPHA_OPAQUE_BIT_KHR, VK_COMPOSITE_ALPHA_PRE_MULTIPLIED_BIT_KHR,
		VK_COMPOSITE_ALPHA_POST_MULTIPLIED_BIT_KHR, VK_COMPOSITE_ALPHA_INHERIT_BIT_KHR };
	for (const auto choice : choices)
		if ((capabilities.supportedCompositeAlpha & choice) != 0) return choice;
	throw std::runtime_error("No supported swapchain composite alpha mode.");
}

/// <summary>
/// 実行ファイルと同じディレクトリにあるSPIR-Vバイナリをuint32_t列として読み込む。
/// </summary>
std::vector<std::uint32_t> ReadShaderFile(const wchar_t* fileName)
{
	const std::filesystem::path shaderPath = GetExecutableDirectory() / fileName;
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

/// <summary>
/// Vulkan Instanceからテクスチャと描画同期まで、最初のフレームに必要な資源を生成する。
/// </summary>
VulkanRenderer::VulkanRenderer(HWND hwnd, std::uint32_t width, std::uint32_t height, const ImageRgba8& texture)
	: windowHandle_(hwnd), windowWidth_(width), windowHeight_(height)
{
	ResetValidationLog();
	CreateInstance(); SetupDebugMessenger(); CreateSurface(hwnd); PickPhysicalDevice();
	QueryDlssDeviceExtensions(); CreateLogicalDevice(); CreateSwapchain(); CreateSwapchainImageViews();
	CreateCommandPool(); CreateDescriptorSetLayout(); CreateTexture(texture);
	InitializeDlss(); CreateAntiAliasingResources(); CreateDescriptorPoolAndSet(); CreateTaaDescriptorResources();
	CreateGraphicsPipeline(); AllocateCommandBuffers(); CreateSyncObjects();
}

/// <summary>
/// GPU完了後、生成順の逆順でVulkan資源を安全に破棄する。
/// </summary>
VulkanRenderer::~VulkanRenderer()
{
	WaitUntilIdle(); DestroySyncObjects(); DestroySwapchainResources();
	ShutdownDlss();
	if (texturePipelineLayout_ != VK_NULL_HANDLE) vkDestroyPipelineLayout(device_, texturePipelineLayout_, nullptr);
	if (taaPipelineLayout_ != VK_NULL_HANDLE) vkDestroyPipelineLayout(device_, taaPipelineLayout_, nullptr);
	DestroyTexture();
	if (textureDescriptorSetLayout_ != VK_NULL_HANDLE) vkDestroyDescriptorSetLayout(device_, textureDescriptorSetLayout_, nullptr);
	if (taaDescriptorSetLayout_ != VK_NULL_HANDLE) vkDestroyDescriptorSetLayout(device_, taaDescriptorSetLayout_, nullptr);
	if (commandPool_ != VK_NULL_HANDLE) vkDestroyCommandPool(device_, commandPool_, nullptr);
	if (device_ != VK_NULL_HANDLE) vkDestroyDevice(device_, nullptr);
	DestroyDebugUtilsMessenger();
	if (surface_ != VK_NULL_HANDLE) vkDestroySurfaceKHR(instance_, surface_, nullptr);
	if (instance_ != VK_NULL_HANDLE) vkDestroyInstance(instance_, nullptr);
}

/// <summary>
/// 描画キューとPresentキューの両方が見つかっているか判定する。
/// </summary>
bool VulkanRenderer::QueueFamilyIndices::IsComplete() const noexcept
{
	return graphicsFamily.has_value() && presentFamily.has_value();
}

/// <summary>
/// Vulkan 1.3と、Win32表示・Debug診断に必要なInstance拡張を有効化する。
/// </summary>
void VulkanRenderer::CreateInstance()
{
	QueryDlssInstanceExtensions();
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

/// <summary>
/// DLSSが要求するVulkan Instance拡張をSDKから取得し、後続のInstance生成へ加える。
/// </summary>
void VulkanRenderer::QueryDlssInstanceExtensions()
{
	const NVSDK_NGX_FeatureDiscoveryInfo discovery = MakeDlssFeatureDiscoveryInfo();
	std::uint32_t extensionCount = 0;
	VkExtensionProperties* extensionProperties = nullptr;
	const NVSDK_NGX_Result result = NVSDK_NGX_VULKAN_GetFeatureInstanceExtensionRequirements(&discovery, &extensionCount, &extensionProperties);
	if (NVSDK_NGX_FAILED(result)) return;
	dlssInstanceExtensions_.clear();
	for (std::uint32_t index = 0; index < extensionCount; ++index) dlssInstanceExtensions_.emplace_back(extensionProperties[index].extensionName);
	dlssExtensionRequirementsAvailable_ = true;
}

/// <summary>
/// Debug構成でのみ、Validation Layerのメッセージ受信先を生成する。
/// </summary>
void VulkanRenderer::SetupDebugMessenger()
{
	if constexpr (kDebugToolsEnabled)
	{
		const auto info = MakeDebugMessengerCreateInfo();
		VK_CHECK(CreateDebugUtilsMessenger(&info));
	}
}

/// <summary>
/// 指定したWin32ウィンドウをVulkanの表示Surfaceへ変換する。
/// </summary>
void VulkanRenderer::CreateSurface(HWND hwnd)
{
	VkWin32SurfaceCreateInfoKHR info{ VK_STRUCTURE_TYPE_WIN32_SURFACE_CREATE_INFO_KHR };
	info.hinstance = GetModuleHandleW(nullptr);
	info.hwnd = hwnd;
	VK_CHECK(vkCreateWin32SurfaceKHR(instance_, &info, nullptr, &surface_));
}

/// <summary>
/// 描画・表示・Swapchain・Vulkan 1.3機能を満たす最初のGPUを採用する。
/// </summary>
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

/// <summary>
/// 選定済みGPUに対してDLSSが要求するVulkan Device拡張をSDKから取得する。
/// </summary>
void VulkanRenderer::QueryDlssDeviceExtensions()
{
	if (!dlssExtensionRequirementsAvailable_) return;
	const NVSDK_NGX_FeatureDiscoveryInfo discovery = MakeDlssFeatureDiscoveryInfo();
	std::uint32_t extensionCount = 0;
	VkExtensionProperties* extensionProperties = nullptr;
	const NVSDK_NGX_Result result = NVSDK_NGX_VULKAN_GetFeatureDeviceExtensionRequirements(instance_, physicalDevice_, &discovery, &extensionCount, &extensionProperties);
	if (NVSDK_NGX_FAILED(result))
	{
		dlssExtensionRequirementsAvailable_ = false;
		dlssDeviceExtensions_.clear();
		return;
	}
	dlssDeviceExtensions_.clear();
	for (std::uint32_t index = 0; index < extensionCount; ++index) dlssDeviceExtensions_.emplace_back(extensionProperties[index].extensionName);
}

/// <summary>
/// 描画・表示・Dynamic Rendering・Swapchainを満たす物理GPUか検証する。
/// </summary>
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

/// <summary>
/// 必須のDevice拡張が物理GPUで利用可能か検証する。
/// </summary>
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

/// <summary>
/// 描画可能かつSurface提示可能なキューファミリの番号を探索する。
/// </summary>
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

/// <summary>
/// 物理GPUとSurfaceの組み合わせで利用可能なSwapchain条件を問い合わせる。
/// </summary>
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

/// <summary>
/// Dynamic Rendering、Synchronization 2、およびNGXが要求するBuffer Device Addressを有効化する。
/// </summary>
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
	VkPhysicalDeviceVulkan12Features features12{ VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_VULKAN_1_2_FEATURES };
	features12.bufferDeviceAddress = VK_TRUE;
	features12.pNext = &features13;
	VkDeviceCreateInfo info{ VK_STRUCTURE_TYPE_DEVICE_CREATE_INFO };
	info.pNext = &features12;
	info.queueCreateInfoCount = static_cast<std::uint32_t>(queueInfos.size());
	info.pQueueCreateInfos = queueInfos.data();
	const auto extensions = GetRequiredDeviceExtensions();
	info.enabledExtensionCount = static_cast<std::uint32_t>(extensions.size());
	info.ppEnabledExtensionNames = extensions.data();
	VK_CHECK(vkCreateDevice(physicalDevice_, &info, nullptr, &device_));
	vkGetDeviceQueue(device_, graphicsQueueFamily_, 0, &graphicsQueue_);
	vkGetDeviceQueue(device_, presentQueueFamily_, 0, &presentQueue_);
}

/// <summary>
/// Swapchain画像はAcquire後からPresent前だけ、アプリ側が描画に使用できる。
/// </summary>
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
	swapchainSupportsStorage_ = (support.capabilities.supportedUsageFlags & VK_IMAGE_USAGE_STORAGE_BIT) != 0;
	info.imageUsage = VK_IMAGE_USAGE_COLOR_ATTACHMENT_BIT;
	if ((support.capabilities.supportedUsageFlags & VK_IMAGE_USAGE_TRANSFER_DST_BIT) != 0) info.imageUsage |= VK_IMAGE_USAGE_TRANSFER_DST_BIT;
	if (swapchainSupportsStorage_) info.imageUsage |= VK_IMAGE_USAGE_STORAGE_BIT;
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

/// <summary>
/// 各Swapchain画像をDynamic RenderingのColor Attachmentとして参照できるImageViewにする。
/// </summary>
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

/// <summary>
/// 指定条件をすべて満たす物理デバイスメモリ種別を探索する。
/// </summary>
std::uint32_t VulkanRenderer::FindMemoryType(std::uint32_t typeFilter, VkMemoryPropertyFlags properties) const
{
	VkPhysicalDeviceMemoryProperties memoryProperties{};
	vkGetPhysicalDeviceMemoryProperties(physicalDevice_, &memoryProperties);
	for (std::uint32_t index = 0; index < memoryProperties.memoryTypeCount; ++index)
	{
		const bool acceptedByResource = (typeFilter & (1u << index)) != 0;
		const bool hasProperties = (memoryProperties.memoryTypes[index].propertyFlags & properties) == properties;
		if (acceptedByResource && hasProperties) return index;
	}
	throw std::runtime_error("No compatible Vulkan memory type was found.");
}

/// <summary>
/// Bufferを生成し、要求された特性のDevice Memoryを割り当てて結合する。
/// </summary>
void VulkanRenderer::CreateBuffer(VkDeviceSize size, VkBufferUsageFlags usage, VkMemoryPropertyFlags properties, VkBuffer& buffer, VkDeviceMemory& memory) const
{
	VkBufferCreateInfo bufferInfo{ VK_STRUCTURE_TYPE_BUFFER_CREATE_INFO };
	bufferInfo.size = size; bufferInfo.usage = usage; bufferInfo.sharingMode = VK_SHARING_MODE_EXCLUSIVE;
	VK_CHECK(vkCreateBuffer(device_, &bufferInfo, nullptr, &buffer));
	VkMemoryRequirements requirements{};
	vkGetBufferMemoryRequirements(device_, buffer, &requirements);
	VkMemoryAllocateInfo allocateInfo{ VK_STRUCTURE_TYPE_MEMORY_ALLOCATE_INFO };
	allocateInfo.allocationSize = requirements.size;
	allocateInfo.memoryTypeIndex = FindMemoryType(requirements.memoryTypeBits, properties);
	VK_CHECK(vkAllocateMemory(device_, &allocateInfo, nullptr, &memory));
	VK_CHECK(vkBindBufferMemory(device_, buffer, memory, 0));
}

/// <summary>
/// 画像転送専用の一回限りPrimary Command Bufferを確保して記録開始する。
/// </summary>
VkCommandBuffer VulkanRenderer::BeginSingleTimeCommands() const
{
	VkCommandBufferAllocateInfo allocateInfo{ VK_STRUCTURE_TYPE_COMMAND_BUFFER_ALLOCATE_INFO };
	allocateInfo.commandPool = commandPool_; allocateInfo.level = VK_COMMAND_BUFFER_LEVEL_PRIMARY; allocateInfo.commandBufferCount = 1;
	VkCommandBuffer commandBuffer = VK_NULL_HANDLE;
	VK_CHECK(vkAllocateCommandBuffers(device_, &allocateInfo, &commandBuffer));
	VkCommandBufferBeginInfo beginInfo{ VK_STRUCTURE_TYPE_COMMAND_BUFFER_BEGIN_INFO };
	beginInfo.flags = VK_COMMAND_BUFFER_USAGE_ONE_TIME_SUBMIT_BIT;
	VK_CHECK(vkBeginCommandBuffer(commandBuffer, &beginInfo));
	return commandBuffer;
}

/// <summary>
/// 一回限りCommand Bufferを送信し、完了を待って解放する。
/// </summary>
void VulkanRenderer::EndSingleTimeCommands(VkCommandBuffer commandBuffer) const
{
	VK_CHECK(vkEndCommandBuffer(commandBuffer));
	VkCommandBufferSubmitInfo commandInfo{ VK_STRUCTURE_TYPE_COMMAND_BUFFER_SUBMIT_INFO };
	commandInfo.commandBuffer = commandBuffer;
	VkSubmitInfo2 submitInfo{ VK_STRUCTURE_TYPE_SUBMIT_INFO_2 };
	submitInfo.commandBufferInfoCount = 1; submitInfo.pCommandBufferInfos = &commandInfo;
	VK_CHECK(vkQueueSubmit2(graphicsQueue_, 1, &submitInfo, VK_NULL_HANDLE));
	VK_CHECK(vkQueueWaitIdle(graphicsQueue_));
	vkFreeCommandBuffers(device_, commandPool_, 1, &commandBuffer);
}

/// <summary>
/// テクスチャ画像を転送先またはFragment Shader読取用レイアウトへ遷移する。
/// </summary>
void VulkanRenderer::TransitionTextureLayout(VkImageLayout oldLayout, VkImageLayout newLayout) const
{
	VkImageMemoryBarrier2 barrier{ VK_STRUCTURE_TYPE_IMAGE_MEMORY_BARRIER_2 };
	barrier.oldLayout = oldLayout; barrier.newLayout = newLayout; barrier.image = textureImage_;
	barrier.subresourceRange.aspectMask = VK_IMAGE_ASPECT_COLOR_BIT;
	barrier.subresourceRange.levelCount = 1; barrier.subresourceRange.layerCount = 1;
	if (oldLayout == VK_IMAGE_LAYOUT_UNDEFINED && newLayout == VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL)
	{
		barrier.srcStageMask = VK_PIPELINE_STAGE_2_NONE;
		barrier.dstStageMask = VK_PIPELINE_STAGE_2_COPY_BIT;
		barrier.dstAccessMask = VK_ACCESS_2_TRANSFER_WRITE_BIT;
	}
	else if (oldLayout == VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL && newLayout == VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL)
	{
		barrier.srcStageMask = VK_PIPELINE_STAGE_2_COPY_BIT;
		barrier.srcAccessMask = VK_ACCESS_2_TRANSFER_WRITE_BIT;
		barrier.dstStageMask = VK_PIPELINE_STAGE_2_FRAGMENT_SHADER_BIT;
		barrier.dstAccessMask = VK_ACCESS_2_SHADER_SAMPLED_READ_BIT;
	}
	else throw std::runtime_error("Unsupported texture image layout transition.");
	VkDependencyInfo dependency{ VK_STRUCTURE_TYPE_DEPENDENCY_INFO };
	dependency.imageMemoryBarrierCount = 1; dependency.pImageMemoryBarriers = &barrier;
	const VkCommandBuffer commandBuffer = BeginSingleTimeCommands();
	vkCmdPipelineBarrier2(commandBuffer, &dependency);
	EndSingleTimeCommands(commandBuffer);
}

/// <summary>
/// Staging Buffer内のRGBA8をテクスチャ画像のMip 0へコピーする。
/// </summary>
void VulkanRenderer::CopyBufferToTexture(VkBuffer buffer, std::uint32_t width, std::uint32_t height) const
{
	VkBufferImageCopy region{};
	region.imageSubresource.aspectMask = VK_IMAGE_ASPECT_COLOR_BIT;
	region.imageSubresource.layerCount = 1;
	region.imageExtent = { width, height, 1 };
	const VkCommandBuffer commandBuffer = BeginSingleTimeCommands();
	vkCmdCopyBufferToImage(commandBuffer, buffer, textureImage_, VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL, 1, &region);
	EndSingleTimeCommands(commandBuffer);
}

/// <summary>
/// WICのRGBA8をStaging Buffer経由でDevice Local画像へ転送し、ViewとSamplerを生成する。
/// </summary>
void VulkanRenderer::CreateTexture(const ImageRgba8& texture)
{
	if (texture.width == 0 || texture.height == 0 || texture.pixels.empty()) throw std::runtime_error("Texture data is empty.");
	textureWidth_ = texture.width;
	textureHeight_ = texture.height;
	const VkDeviceSize byteSize = static_cast<VkDeviceSize>(texture.pixels.size());
	VkBuffer stagingBuffer = VK_NULL_HANDLE;
	VkDeviceMemory stagingMemory = VK_NULL_HANDLE;
	CreateBuffer(byteSize, VK_BUFFER_USAGE_TRANSFER_SRC_BIT, VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT | VK_MEMORY_PROPERTY_HOST_COHERENT_BIT, stagingBuffer, stagingMemory);
	void* mapped = nullptr;
	VK_CHECK(vkMapMemory(device_, stagingMemory, 0, byteSize, 0, &mapped));
	std::memcpy(mapped, texture.pixels.data(), texture.pixels.size());
	vkUnmapMemory(device_, stagingMemory);

	VkImageCreateInfo imageInfo{ VK_STRUCTURE_TYPE_IMAGE_CREATE_INFO };
	imageInfo.imageType = VK_IMAGE_TYPE_2D; imageInfo.extent = { texture.width, texture.height, 1 };
	imageInfo.mipLevels = 1; imageInfo.arrayLayers = 1; imageInfo.format = VK_FORMAT_R8G8B8A8_SRGB;
	imageInfo.tiling = VK_IMAGE_TILING_OPTIMAL; imageInfo.initialLayout = VK_IMAGE_LAYOUT_UNDEFINED;
	imageInfo.usage = VK_IMAGE_USAGE_TRANSFER_DST_BIT | VK_IMAGE_USAGE_SAMPLED_BIT;
	imageInfo.samples = VK_SAMPLE_COUNT_1_BIT; imageInfo.sharingMode = VK_SHARING_MODE_EXCLUSIVE;
	VK_CHECK(vkCreateImage(device_, &imageInfo, nullptr, &textureImage_));
	VkMemoryRequirements requirements{};
	vkGetImageMemoryRequirements(device_, textureImage_, &requirements);
	VkMemoryAllocateInfo allocateInfo{ VK_STRUCTURE_TYPE_MEMORY_ALLOCATE_INFO };
	allocateInfo.allocationSize = requirements.size;
	allocateInfo.memoryTypeIndex = FindMemoryType(requirements.memoryTypeBits, VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT);
	VK_CHECK(vkAllocateMemory(device_, &allocateInfo, nullptr, &textureMemory_));
	VK_CHECK(vkBindImageMemory(device_, textureImage_, textureMemory_, 0));
	TransitionTextureLayout(VK_IMAGE_LAYOUT_UNDEFINED, VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL);
	CopyBufferToTexture(stagingBuffer, texture.width, texture.height);
	TransitionTextureLayout(VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL, VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL);
	vkDestroyBuffer(device_, stagingBuffer, nullptr);
	vkFreeMemory(device_, stagingMemory, nullptr);

	VkImageViewCreateInfo viewInfo{ VK_STRUCTURE_TYPE_IMAGE_VIEW_CREATE_INFO };
	viewInfo.image = textureImage_; viewInfo.viewType = VK_IMAGE_VIEW_TYPE_2D; viewInfo.format = VK_FORMAT_R8G8B8A8_SRGB;
	viewInfo.subresourceRange.aspectMask = VK_IMAGE_ASPECT_COLOR_BIT;
	viewInfo.subresourceRange.levelCount = 1; viewInfo.subresourceRange.layerCount = 1;
	VK_CHECK(vkCreateImageView(device_, &viewInfo, nullptr, &textureImageView_));
	VkSamplerCreateInfo samplerInfo{ VK_STRUCTURE_TYPE_SAMPLER_CREATE_INFO };
	samplerInfo.magFilter = VK_FILTER_LINEAR; samplerInfo.minFilter = VK_FILTER_LINEAR;
	samplerInfo.addressModeU = VK_SAMPLER_ADDRESS_MODE_CLAMP_TO_EDGE;
	samplerInfo.addressModeV = VK_SAMPLER_ADDRESS_MODE_CLAMP_TO_EDGE;
	samplerInfo.addressModeW = VK_SAMPLER_ADDRESS_MODE_CLAMP_TO_EDGE;
	samplerInfo.mipmapMode = VK_SAMPLER_MIPMAP_MODE_LINEAR; samplerInfo.maxLod = 0.0f;
	VK_CHECK(vkCreateSampler(device_, &samplerInfo, nullptr, &textureSampler_));
}

/// <summary>
/// テクスチャのSampler、View、Image、Device Memoryを依存順に破棄する。
/// </summary>
void VulkanRenderer::DestroyTexture()
{
	if (device_ == VK_NULL_HANDLE) return;
	if (textureSampler_ != VK_NULL_HANDLE) vkDestroySampler(device_, textureSampler_, nullptr);
	if (textureImageView_ != VK_NULL_HANDLE) vkDestroyImageView(device_, textureImageView_, nullptr);
	if (textureImage_ != VK_NULL_HANDLE) vkDestroyImage(device_, textureImage_, nullptr);
	if (textureMemory_ != VK_NULL_HANDLE) vkFreeMemory(device_, textureMemory_, nullptr);
	textureSampler_ = VK_NULL_HANDLE; textureImageView_ = VK_NULL_HANDLE;
	textureImage_ = VK_NULL_HANDLE; textureMemory_ = VK_NULL_HANDLE;
	textureWidth_ = 0; textureHeight_ = 0;
}

/// <summary>
/// Swapchain解像度のPost Process画像を生成し、Device LocalメモリとViewを結合する。
/// </summary>
void VulkanRenderer::CreatePostProcessImage(VkFormat format, VkImageUsageFlags usage, VkImageAspectFlags aspectMask, PostProcessImage& image)
{
	VkImageCreateInfo imageInfo{ VK_STRUCTURE_TYPE_IMAGE_CREATE_INFO };
	imageInfo.imageType = VK_IMAGE_TYPE_2D;
	imageInfo.extent = { swapchainExtent_.width, swapchainExtent_.height, 1 };
	imageInfo.mipLevels = 1; imageInfo.arrayLayers = 1; imageInfo.format = format;
	imageInfo.tiling = VK_IMAGE_TILING_OPTIMAL; imageInfo.initialLayout = VK_IMAGE_LAYOUT_UNDEFINED;
	imageInfo.usage = usage; imageInfo.samples = VK_SAMPLE_COUNT_1_BIT; imageInfo.sharingMode = VK_SHARING_MODE_EXCLUSIVE;
	VK_CHECK(vkCreateImage(device_, &imageInfo, nullptr, &image.image));
	VkMemoryRequirements requirements{};
	vkGetImageMemoryRequirements(device_, image.image, &requirements);
	VkMemoryAllocateInfo allocateInfo{ VK_STRUCTURE_TYPE_MEMORY_ALLOCATE_INFO };
	allocateInfo.allocationSize = requirements.size;
	allocateInfo.memoryTypeIndex = FindMemoryType(requirements.memoryTypeBits, VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT);
	VK_CHECK(vkAllocateMemory(device_, &allocateInfo, nullptr, &image.memory));
	VK_CHECK(vkBindImageMemory(device_, image.image, image.memory, 0));
	VkImageViewCreateInfo viewInfo{ VK_STRUCTURE_TYPE_IMAGE_VIEW_CREATE_INFO };
	viewInfo.image = image.image; viewInfo.viewType = VK_IMAGE_VIEW_TYPE_2D; viewInfo.format = format;
	viewInfo.subresourceRange.aspectMask = aspectMask; viewInfo.subresourceRange.levelCount = 1; viewInfo.subresourceRange.layerCount = 1;
	VK_CHECK(vkCreateImageView(device_, &viewInfo, nullptr, &image.view));
	image.format = format; image.layout = VK_IMAGE_LAYOUT_UNDEFINED; image.aspectMask = aspectMask;
}

/// <summary>
/// Post Process画像のView、Image、Device Memoryを依存順に破棄する。
/// </summary>
void VulkanRenderer::DestroyPostProcessImage(PostProcessImage& image)
{
	if (device_ == VK_NULL_HANDLE) return;
	if (image.view != VK_NULL_HANDLE) vkDestroyImageView(device_, image.view, nullptr);
	if (image.image != VK_NULL_HANDLE) vkDestroyImage(device_, image.image, nullptr);
	if (image.memory != VK_NULL_HANDLE) vkFreeMemory(device_, image.memory, nullptr);
	image = {};
}

/// <summary>
/// scene、TAA履歴、およびDLSSで必要な入力画像を現在のSwapchain解像度で生成する。
/// </summary>
void VulkanRenderer::CreateAntiAliasingResources()
{
	const VkImageUsageFlags colorUsage = VK_IMAGE_USAGE_COLOR_ATTACHMENT_BIT | VK_IMAGE_USAGE_SAMPLED_BIT | VK_IMAGE_USAGE_STORAGE_BIT | VK_IMAGE_USAGE_TRANSFER_DST_BIT;
	CreatePostProcessImage(VK_FORMAT_R8G8B8A8_UNORM, colorUsage, VK_IMAGE_ASPECT_COLOR_BIT, sceneColor_);
	for (auto& history : taaHistory_) CreatePostProcessImage(VK_FORMAT_R8G8B8A8_UNORM, colorUsage, VK_IMAGE_ASPECT_COLOR_BIT, history);
	taaHistoryIndex_ = 0; taaHistoryValid_ = false;
	if (dlssInitialized_ && swapchainSupportsStorage_)
	{
		CreatePostProcessImage(VK_FORMAT_D32_SFLOAT, VK_IMAGE_USAGE_SAMPLED_BIT | VK_IMAGE_USAGE_TRANSFER_DST_BIT, VK_IMAGE_ASPECT_DEPTH_BIT, dlssDepth_);
		CreatePostProcessImage(VK_FORMAT_R16G16_SFLOAT, VK_IMAGE_USAGE_SAMPLED_BIT | VK_IMAGE_USAGE_TRANSFER_DST_BIT, VK_IMAGE_ASPECT_COLOR_BIT, dlssMotionVectors_);
		CreateDlssFeature();
	}
}

/// <summary>
/// DLSS Featureを先に解放し、Swapchain依存の内部画像をすべて破棄する。
/// </summary>
void VulkanRenderer::DestroyAntiAliasingResources()
{
	ReleaseDlssFeature();
	DestroyPostProcessImage(dlssMotionVectors_); DestroyPostProcessImage(dlssDepth_);
	for (auto& history : taaHistory_) DestroyPostProcessImage(history);
	DestroyPostProcessImage(sceneColor_);
	taaHistoryValid_ = false;
}

/// <summary>
/// 画像レイアウトを用途に合わせて遷移し、全コマンド間の読書き依存を明示する。
/// </summary>
void VulkanRenderer::TransitionImage(VkCommandBuffer commandBuffer, PostProcessImage& image, VkImageLayout newLayout) const
{
	if (image.layout == newLayout) return;
	VkImageMemoryBarrier2 barrier{ VK_STRUCTURE_TYPE_IMAGE_MEMORY_BARRIER_2 };
	barrier.srcStageMask = image.layout == VK_IMAGE_LAYOUT_UNDEFINED ? VK_PIPELINE_STAGE_2_NONE : VK_PIPELINE_STAGE_2_ALL_COMMANDS_BIT;
	barrier.srcAccessMask = image.layout == VK_IMAGE_LAYOUT_UNDEFINED ? VK_ACCESS_2_NONE : VK_ACCESS_2_MEMORY_WRITE_BIT;
	barrier.dstStageMask = VK_PIPELINE_STAGE_2_ALL_COMMANDS_BIT;
	barrier.dstAccessMask = VK_ACCESS_2_MEMORY_READ_BIT | VK_ACCESS_2_MEMORY_WRITE_BIT;
	barrier.oldLayout = image.layout; barrier.newLayout = newLayout; barrier.image = image.image;
	barrier.subresourceRange.aspectMask = image.aspectMask; barrier.subresourceRange.levelCount = 1; barrier.subresourceRange.layerCount = 1;
	VkDependencyInfo dependency{ VK_STRUCTURE_TYPE_DEPENDENCY_INFO };
	dependency.imageMemoryBarrierCount = 1; dependency.pImageMemoryBarriers = &barrier;
	vkCmdPipelineBarrier2(commandBuffer, &dependency);
	image.layout = newLayout;
}

/// <summary>
/// Fragment Shaderの単一テクスチャと、TAAの二入力を公開するDescriptor Set Layoutを生成する。
/// </summary>
void VulkanRenderer::CreateDescriptorSetLayout()
{
	VkDescriptorSetLayoutBinding binding{};
	binding.binding = 0; binding.descriptorType = VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER;
	binding.descriptorCount = 1; binding.stageFlags = VK_SHADER_STAGE_FRAGMENT_BIT;
	VkDescriptorSetLayoutCreateInfo info{ VK_STRUCTURE_TYPE_DESCRIPTOR_SET_LAYOUT_CREATE_INFO };
	info.bindingCount = 1; info.pBindings = &binding;
	VK_CHECK(vkCreateDescriptorSetLayout(device_, &info, nullptr, &textureDescriptorSetLayout_));
	const std::array<VkDescriptorSetLayoutBinding, 2> taaBindings{ binding, VkDescriptorSetLayoutBinding{ 1, VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER, 1, VK_SHADER_STAGE_FRAGMENT_BIT, nullptr } };
	VkDescriptorSetLayoutCreateInfo taaInfo{ VK_STRUCTURE_TYPE_DESCRIPTOR_SET_LAYOUT_CREATE_INFO };
	taaInfo.flags = 0; taaInfo.bindingCount = static_cast<std::uint32_t>(taaBindings.size()); taaInfo.pBindings = taaBindings.data();
	VK_CHECK(vkCreateDescriptorSetLayout(device_, &taaInfo, nullptr, &taaDescriptorSetLayout_));
}

/// <summary>
/// 元画像とTAA履歴を読む単一Sampler Descriptor Setを生成する。
/// </summary>
void VulkanRenderer::CreateDescriptorPoolAndSet()
{
	VkDescriptorPoolSize poolSize{};
	poolSize.type = VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER; poolSize.descriptorCount = 3;
	VkDescriptorPoolCreateInfo poolInfo{ VK_STRUCTURE_TYPE_DESCRIPTOR_POOL_CREATE_INFO };
	poolInfo.maxSets = 3; poolInfo.poolSizeCount = 1; poolInfo.pPoolSizes = &poolSize;
	VK_CHECK(vkCreateDescriptorPool(device_, &poolInfo, nullptr, &textureDescriptorPool_));
	const std::array<VkDescriptorSetLayout, 3> layouts{ textureDescriptorSetLayout_, textureDescriptorSetLayout_, textureDescriptorSetLayout_ };
	std::array<VkDescriptorSet, 3> sets{};
	VkDescriptorSetAllocateInfo allocateInfo{ VK_STRUCTURE_TYPE_DESCRIPTOR_SET_ALLOCATE_INFO };
	allocateInfo.descriptorPool = textureDescriptorPool_; allocateInfo.descriptorSetCount = static_cast<std::uint32_t>(layouts.size());
	allocateInfo.pSetLayouts = layouts.data();
	VK_CHECK(vkAllocateDescriptorSets(device_, &allocateInfo, sets.data()));
	textureDescriptorSet_ = sets[0]; historyDescriptorSets_[0] = sets[1]; historyDescriptorSets_[1] = sets[2];
	const std::array<VkDescriptorImageInfo, 3> images{
		VkDescriptorImageInfo{ textureSampler_, textureImageView_, VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL },
		VkDescriptorImageInfo{ textureSampler_, taaHistory_[0].view, VK_IMAGE_LAYOUT_GENERAL },
		VkDescriptorImageInfo{ textureSampler_, taaHistory_[1].view, VK_IMAGE_LAYOUT_GENERAL } };
	std::array<VkWriteDescriptorSet, 3> writes{};
	for (std::size_t index = 0; index < writes.size(); ++index)
	{
		writes[index] = { VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET };
		writes[index].dstSet = sets[index]; writes[index].dstBinding = 0; writes[index].descriptorCount = 1;
		writes[index].descriptorType = VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER; writes[index].pImageInfo = &images[index];
	}
	vkUpdateDescriptorSets(device_, static_cast<std::uint32_t>(writes.size()), writes.data(), 0, nullptr);
}

/// <summary>
/// sceneと各TAA履歴を二入力として結び付けるDescriptor PoolとSetを生成する。
/// </summary>
void VulkanRenderer::CreateTaaDescriptorResources()
{
	VkDescriptorPoolSize poolSize{ VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER, 4 };
	VkDescriptorPoolCreateInfo poolInfo{ VK_STRUCTURE_TYPE_DESCRIPTOR_POOL_CREATE_INFO };
	poolInfo.maxSets = 2; poolInfo.poolSizeCount = 1; poolInfo.pPoolSizes = &poolSize;
	VK_CHECK(vkCreateDescriptorPool(device_, &poolInfo, nullptr, &taaDescriptorPool_));
	const std::array<VkDescriptorSetLayout, 2> layouts{ taaDescriptorSetLayout_, taaDescriptorSetLayout_ };
	VkDescriptorSetAllocateInfo allocateInfo{ VK_STRUCTURE_TYPE_DESCRIPTOR_SET_ALLOCATE_INFO };
	allocateInfo.descriptorPool = taaDescriptorPool_; allocateInfo.descriptorSetCount = static_cast<std::uint32_t>(layouts.size()); allocateInfo.pSetLayouts = layouts.data();
	VK_CHECK(vkAllocateDescriptorSets(device_, &allocateInfo, taaDescriptorSets_.data()));
	for (std::uint32_t index = 0; index < taaDescriptorSets_.size(); ++index)
	{
		const std::array<VkDescriptorImageInfo, 2> images{
			VkDescriptorImageInfo{ textureSampler_, sceneColor_.view, VK_IMAGE_LAYOUT_GENERAL },
			VkDescriptorImageInfo{ textureSampler_, taaHistory_[index].view, VK_IMAGE_LAYOUT_GENERAL } };
		std::array<VkWriteDescriptorSet, 2> writes{};
		for (std::uint32_t binding = 0; binding < writes.size(); ++binding)
		{
			writes[binding] = { VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET };
			writes[binding].dstSet = taaDescriptorSets_[index]; writes[binding].dstBinding = binding; writes[binding].descriptorCount = 1;
			writes[binding].descriptorType = VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER; writes[binding].pImageInfo = &images[binding];
		}
		vkUpdateDescriptorSets(device_, static_cast<std::uint32_t>(writes.size()), writes.data(), 0, nullptr);
	}
}

/// <summary>
/// Swapchain解像度に依存するすべてのDescriptor PoolとSetを破棄する。
/// </summary>
void VulkanRenderer::DestroyDescriptorResources()
{
	if (device_ == VK_NULL_HANDLE) return;
	if (taaDescriptorPool_ != VK_NULL_HANDLE) vkDestroyDescriptorPool(device_, taaDescriptorPool_, nullptr);
	if (textureDescriptorPool_ != VK_NULL_HANDLE) vkDestroyDescriptorPool(device_, textureDescriptorPool_, nullptr);
	taaDescriptorPool_ = VK_NULL_HANDLE; textureDescriptorPool_ = VK_NULL_HANDLE;
	taaDescriptorSets_ = {}; historyDescriptorSets_ = {}; textureDescriptorSet_ = VK_NULL_HANDLE;
}

/// <summary>
/// scene、TAA、presentで共有するDynamic Rendering用Graphics Pipelineを生成する。
/// </summary>
void VulkanRenderer::CreateGraphicsPipelineForTarget(VkShaderModule vertexShader, VkShaderModule fragmentShader, VkPipelineLayout pipelineLayout, VkFormat targetFormat, bool alphaBlend, VkPipeline& pipeline) const
{
	VkPipelineShaderStageCreateInfo vertexStage{ VK_STRUCTURE_TYPE_PIPELINE_SHADER_STAGE_CREATE_INFO };
	vertexStage.stage = VK_SHADER_STAGE_VERTEX_BIT; vertexStage.module = vertexShader; vertexStage.pName = "main";
	VkPipelineShaderStageCreateInfo fragmentStage{ VK_STRUCTURE_TYPE_PIPELINE_SHADER_STAGE_CREATE_INFO };
	fragmentStage.stage = VK_SHADER_STAGE_FRAGMENT_BIT; fragmentStage.module = fragmentShader; fragmentStage.pName = "main";
	const VkPipelineShaderStageCreateInfo stages[]{ vertexStage, fragmentStage };
	VkPipelineVertexInputStateCreateInfo vertexInput{ VK_STRUCTURE_TYPE_PIPELINE_VERTEX_INPUT_STATE_CREATE_INFO };
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
	colorBlendAttachment.blendEnable = alphaBlend ? VK_TRUE : VK_FALSE;
	colorBlendAttachment.srcColorBlendFactor = VK_BLEND_FACTOR_SRC_ALPHA;
	colorBlendAttachment.dstColorBlendFactor = VK_BLEND_FACTOR_ONE_MINUS_SRC_ALPHA;
	colorBlendAttachment.colorBlendOp = VK_BLEND_OP_ADD;
	colorBlendAttachment.srcAlphaBlendFactor = VK_BLEND_FACTOR_ONE;
	colorBlendAttachment.dstAlphaBlendFactor = VK_BLEND_FACTOR_ONE_MINUS_SRC_ALPHA;
	colorBlendAttachment.alphaBlendOp = VK_BLEND_OP_ADD;
	VkPipelineColorBlendStateCreateInfo colorBlending{ VK_STRUCTURE_TYPE_PIPELINE_COLOR_BLEND_STATE_CREATE_INFO };
	colorBlending.attachmentCount = 1; colorBlending.pAttachments = &colorBlendAttachment;
	const VkDynamicState dynamicStates[]{ VK_DYNAMIC_STATE_VIEWPORT, VK_DYNAMIC_STATE_SCISSOR };
	VkPipelineDynamicStateCreateInfo dynamicState{ VK_STRUCTURE_TYPE_PIPELINE_DYNAMIC_STATE_CREATE_INFO };
	dynamicState.dynamicStateCount = static_cast<std::uint32_t>(std::size(dynamicStates)); dynamicState.pDynamicStates = dynamicStates;
	VkPipelineRenderingCreateInfo renderingInfo{ VK_STRUCTURE_TYPE_PIPELINE_RENDERING_CREATE_INFO };
	renderingInfo.colorAttachmentCount = 1; renderingInfo.pColorAttachmentFormats = &targetFormat;
	VkGraphicsPipelineCreateInfo pipelineInfo{ VK_STRUCTURE_TYPE_GRAPHICS_PIPELINE_CREATE_INFO };
	pipelineInfo.pNext = &renderingInfo; pipelineInfo.stageCount = static_cast<std::uint32_t>(std::size(stages)); pipelineInfo.pStages = stages;
	pipelineInfo.pVertexInputState = &vertexInput; pipelineInfo.pInputAssemblyState = &inputAssembly;
	pipelineInfo.pViewportState = &viewportState; pipelineInfo.pRasterizationState = &rasterizer;
	pipelineInfo.pMultisampleState = &multisampling; pipelineInfo.pColorBlendState = &colorBlending;
	pipelineInfo.pDynamicState = &dynamicState; pipelineInfo.layout = pipelineLayout;
	VK_CHECK(vkCreateGraphicsPipelines(device_, VK_NULL_HANDLE, 1, &pipelineInfo, nullptr, &pipeline));
}

/// <summary>
/// scene描画・TAA履歴合成・Swapchain提示の三種類のGraphics Pipelineを生成する。
/// </summary>
void VulkanRenderer::CreateGraphicsPipeline()
{
	const VkShaderModule vertexShader = CreateShaderModule(ReadShaderFile(L"TRExLap2Shader.vert.spv"));
	const VkShaderModule textureFragmentShader = CreateShaderModule(ReadShaderFile(L"TRExLap2Shader.frag.spv"));
	const VkShaderModule taaFragmentShader = CreateShaderModule(ReadShaderFile(L"TRExLap2Taa.frag.spv"));
	VkPushConstantRange drawRange{};
	drawRange.stageFlags = VK_SHADER_STAGE_VERTEX_BIT | VK_SHADER_STAGE_FRAGMENT_BIT;
	drawRange.offset = 0; drawRange.size = sizeof(TextureDrawConstants);
	if (texturePipelineLayout_ == VK_NULL_HANDLE)
	{
		VkPipelineLayoutCreateInfo info{ VK_STRUCTURE_TYPE_PIPELINE_LAYOUT_CREATE_INFO };
		info.setLayoutCount = 1; info.pSetLayouts = &textureDescriptorSetLayout_;
		info.pushConstantRangeCount = 1; info.pPushConstantRanges = &drawRange;
		VK_CHECK(vkCreatePipelineLayout(device_, &info, nullptr, &texturePipelineLayout_));
	}
	if (taaPipelineLayout_ == VK_NULL_HANDLE)
	{
		VkPipelineLayoutCreateInfo info{ VK_STRUCTURE_TYPE_PIPELINE_LAYOUT_CREATE_INFO };
		info.setLayoutCount = 1; info.pSetLayouts = &taaDescriptorSetLayout_;
		info.pushConstantRangeCount = 1; info.pPushConstantRanges = &drawRange;
		VK_CHECK(vkCreatePipelineLayout(device_, &info, nullptr, &taaPipelineLayout_));
	}
	CreateGraphicsPipelineForTarget(vertexShader, textureFragmentShader, texturePipelineLayout_, sceneColor_.format, true, scenePipeline_);
	CreateGraphicsPipelineForTarget(vertexShader, taaFragmentShader, taaPipelineLayout_, taaHistory_[0].format, false, taaPipeline_);
	CreateGraphicsPipelineForTarget(vertexShader, textureFragmentShader, texturePipelineLayout_, swapchainImageFormat_, false, presentPipeline_);
	vkDestroyShaderModule(device_, taaFragmentShader, nullptr);
	vkDestroyShaderModule(device_, textureFragmentShader, nullptr);
	vkDestroyShaderModule(device_, vertexShader, nullptr);
}

/// <summary>
/// 読み込んだSPIR-Vコードから一時的なVulkan Shader Moduleを生成する。
/// </summary>
VkShaderModule VulkanRenderer::CreateShaderModule(const std::vector<std::uint32_t>& code) const
{
	VkShaderModuleCreateInfo info{ VK_STRUCTURE_TYPE_SHADER_MODULE_CREATE_INFO };
	info.codeSize = code.size() * sizeof(std::uint32_t); info.pCode = code.data();
	VkShaderModule shader = VK_NULL_HANDLE;
	VK_CHECK(vkCreateShaderModule(device_, &info, nullptr, &shader));
	return shader;
}

/// <summary>
/// NVNGXをプロジェクト識別子付きで初期化し、DLSS Super Samplingの可用性を問い合わせる。
/// </summary>
void VulkanRenderer::InitializeDlss()
{
	if (!dlssExtensionRequirementsAvailable_)
	{
		AppendValidationLog("[DLSS] NGX extension requirements are unavailable. TAA fallback selected.");
		return;
	}
	const std::wstring applicationDataPath = GetExecutableDirectory().wstring();
	const NVSDK_NGX_Result initialized = NVSDK_NGX_VULKAN_Init_with_ProjectID(
		kDlssProjectId, NVSDK_NGX_ENGINE_TYPE_CUSTOM, kDlssEngineVersion, applicationDataPath.c_str(),
		instance_, physicalDevice_, device_, vkGetInstanceProcAddr, vkGetDeviceProcAddr, nullptr);
	if (NVSDK_NGX_FAILED(initialized))
	{
		std::ostringstream message; message << "[DLSS] NGX initialization failed (0x" << std::hex << static_cast<unsigned int>(initialized) << "). TAA fallback selected.\n";
		OutputDebugStringA(message.str().c_str());
		AppendValidationLog(message.str().c_str());
		return;
	}
	dlssInitialized_ = true;
	const NVSDK_NGX_Result capabilityResult = NVSDK_NGX_VULKAN_GetCapabilityParameters(&dlssParameters_);
	int available = 0;
	const NVSDK_NGX_Result availabilityResult = NVSDK_NGX_SUCCEED(capabilityResult)
		? NVSDK_NGX_Parameter_GetI(dlssParameters_, NVSDK_NGX_EParameter_SuperSampling_Available, &available)
		: NVSDK_NGX_Result_Fail;
	if (NVSDK_NGX_FAILED(capabilityResult) || NVSDK_NGX_FAILED(availabilityResult) || available == 0)
	{
		std::ostringstream message;
		message << "[DLSS] DLSS Super Sampling is unavailable (capability=0x" << std::hex << static_cast<unsigned int>(capabilityResult)
			<< ", availability=0x" << static_cast<unsigned int>(availabilityResult) << std::dec << ", value=" << available << "). TAA fallback selected.\n";
		OutputDebugStringA(message.str().c_str());
		AppendValidationLog(message.str().c_str());
		if (dlssParameters_ != nullptr) NVSDK_NGX_VULKAN_DestroyParameters(dlssParameters_);
		dlssParameters_ = nullptr;
		NVSDK_NGX_VULKAN_Shutdown1(device_);
		dlssInitialized_ = false;
	}
}

/// <summary>
/// 現在のSwapchain解像度を入出力同一にして、DLAA Featureを生成する。
/// </summary>
void VulkanRenderer::CreateDlssFeature()
{
	if (!dlssInitialized_ || dlssParameters_ == nullptr || !swapchainSupportsStorage_) return;
	NVSDK_NGX_DLSS_Create_Params createParams{};
	createParams.Feature.InWidth = swapchainExtent_.width; createParams.Feature.InHeight = swapchainExtent_.height;
	createParams.Feature.InTargetWidth = swapchainExtent_.width; createParams.Feature.InTargetHeight = swapchainExtent_.height;
	createParams.Feature.InPerfQualityValue = NVSDK_NGX_PerfQuality_Value_DLAA;
	createParams.InFeatureCreateFlags = NVSDK_NGX_DLSS_Feature_Flags_None;
	const VkCommandBuffer commandBuffer = BeginSingleTimeCommands();
	const NVSDK_NGX_Result result = NGX_VULKAN_CREATE_DLSS_EXT1(device_, commandBuffer, 0, 0, &dlssFeature_, dlssParameters_, &createParams);
	EndSingleTimeCommands(commandBuffer);
	if (NVSDK_NGX_FAILED(result))
	{
		std::ostringstream message; message << "[DLSS] DLAA feature creation failed (0x" << std::hex << static_cast<unsigned int>(result) << "). TAA fallback selected.\n";
		OutputDebugStringA(message.str().c_str());
		AppendValidationLog(message.str().c_str());
		dlssFeature_ = nullptr;
		return;
	}
	dlssEnabled_ = true;
	OutputDebugStringA("[DLSS] DLAA feature enabled.\n");
	AppendValidationLog("[DLSS] DLAA feature enabled.");
}

/// <summary>
/// 解像度変更または終了前に、生成済みのDLSS Featureを解放する。
/// </summary>
void VulkanRenderer::ReleaseDlssFeature()
{
	if (dlssFeature_ != nullptr) NVSDK_NGX_VULKAN_ReleaseFeature(dlssFeature_);
	dlssFeature_ = nullptr; dlssEnabled_ = false;
}

/// <summary>
/// NGX Parameterを破棄し、Vulkan Deviceより先にNGXを停止する。
/// </summary>
void VulkanRenderer::ShutdownDlss()
{
	ReleaseDlssFeature();
	if (dlssParameters_ != nullptr) NVSDK_NGX_VULKAN_DestroyParameters(dlssParameters_);
	dlssParameters_ = nullptr;
	if (dlssInitialized_) NVSDK_NGX_VULKAN_Shutdown1(device_);
	dlssInitialized_ = false;
}

/// <summary>
/// scene・深度・MVをDLAAへ渡し、取得済みSwapchain画像へ結果を書き込む。
/// </summary>
bool VulkanRenderer::EvaluateDlss(VkCommandBuffer commandBuffer, std::uint32_t imageIndex)
{
	if (!dlssEnabled_ || dlssFeature_ == nullptr || dlssParameters_ == nullptr) return false;
	const VkImageSubresourceRange colorRange{ VK_IMAGE_ASPECT_COLOR_BIT, 0, 1, 0, 1 };
	const VkImageSubresourceRange depthRange{ VK_IMAGE_ASPECT_DEPTH_BIT, 0, 1, 0, 1 };
	NVSDK_NGX_Resource_VK scene = NVSDK_NGX_Create_ImageView_Resource_VK(sceneColor_.view, sceneColor_.image, colorRange, sceneColor_.format, swapchainExtent_.width, swapchainExtent_.height, false);
	NVSDK_NGX_Resource_VK depth = NVSDK_NGX_Create_ImageView_Resource_VK(dlssDepth_.view, dlssDepth_.image, depthRange, dlssDepth_.format, swapchainExtent_.width, swapchainExtent_.height, false);
	NVSDK_NGX_Resource_VK motion = NVSDK_NGX_Create_ImageView_Resource_VK(dlssMotionVectors_.view, dlssMotionVectors_.image, colorRange, dlssMotionVectors_.format, swapchainExtent_.width, swapchainExtent_.height, false);
	NVSDK_NGX_Resource_VK output = NVSDK_NGX_Create_ImageView_Resource_VK(swapchainImageViews_[imageIndex], swapchainImages_[imageIndex], colorRange, swapchainImageFormat_, swapchainExtent_.width, swapchainExtent_.height, true);
	NVSDK_NGX_VK_DLSS_Eval_Params evaluateParams{};
	evaluateParams.Feature.pInColor = &scene; evaluateParams.Feature.pInOutput = &output;
	evaluateParams.pInDepth = &depth; evaluateParams.pInMotionVectors = &motion;
	evaluateParams.InRenderSubrectDimensions = { swapchainExtent_.width, swapchainExtent_.height };
	evaluateParams.InReset = taaHistoryValid_ ? 0 : 1;
	const NVSDK_NGX_Result result = NGX_VULKAN_EVALUATE_DLSS_EXT(commandBuffer, dlssFeature_, dlssParameters_, &evaluateParams);
	if (NVSDK_NGX_SUCCEED(result)) return true;
	std::ostringstream message; message << "[DLSS] DLAA evaluation failed (0x" << std::hex << static_cast<unsigned int>(result) << "). TAA fallback selected.\n";
	OutputDebugStringA(message.str().c_str());
	AppendValidationLog(message.str().c_str());
	dlssEnabled_ = false;
	return false;
}

/// <summary>
/// Graphics Queue用コマンドバッファを個別リセット可能なCommand Poolを生成する。
/// </summary>
void VulkanRenderer::CreateCommandPool()
{
	VkCommandPoolCreateInfo info{ VK_STRUCTURE_TYPE_COMMAND_POOL_CREATE_INFO };
	info.flags = VK_COMMAND_POOL_CREATE_RESET_COMMAND_BUFFER_BIT; info.queueFamilyIndex = graphicsQueueFamily_;
	VK_CHECK(vkCreateCommandPool(device_, &info, nullptr, &commandPool_));
}

/// <summary>
/// フレーム並列数と同数のPrimary Command Bufferを確保する。
/// </summary>
void VulkanRenderer::AllocateCommandBuffers()
{
	commandBuffers_.resize(kMaxFramesInFlight);
	VkCommandBufferAllocateInfo info{ VK_STRUCTURE_TYPE_COMMAND_BUFFER_ALLOCATE_INFO };
	info.commandPool = commandPool_; info.level = VK_COMMAND_BUFFER_LEVEL_PRIMARY; info.commandBufferCount = kMaxFramesInFlight;
	VK_CHECK(vkAllocateCommandBuffers(device_, &info, commandBuffers_.data()));
}

/// <summary>
/// Acquire・描画完了・CPU待機を同期するSemaphoreとFenceをフレームごとに生成する。
/// </summary>
void VulkanRenderer::CreateSyncObjects()
{
	VkSemaphoreCreateInfo semaphoreInfo{ VK_STRUCTURE_TYPE_SEMAPHORE_CREATE_INFO };
	VkFenceCreateInfo fenceInfo{ VK_STRUCTURE_TYPE_FENCE_CREATE_INFO };
	fenceInfo.flags = VK_FENCE_CREATE_SIGNALED_BIT;
	for (auto& frame : frameSyncs_)
	{
		VK_CHECK(vkCreateSemaphore(device_, &semaphoreInfo, nullptr, &frame.imageAvailable));
		VK_CHECK(vkCreateFence(device_, &fenceInfo, nullptr, &frame.inFlight));
	}
	CreateRenderFinishedSemaphores();
}

/// <summary>
/// Present待機の寿命をSwapchain画像へ結び付ける描画完了Semaphoreを生成する。
/// </summary>
void VulkanRenderer::CreateRenderFinishedSemaphores()
{
	VkSemaphoreCreateInfo info{ VK_STRUCTURE_TYPE_SEMAPHORE_CREATE_INFO };
	renderFinishedSemaphores_.resize(swapchainImages_.size(), VK_NULL_HANDLE);
	for (auto& semaphore : renderFinishedSemaphores_) VK_CHECK(vkCreateSemaphore(device_, &info, nullptr, &semaphore));
}

/// <summary>
/// Swapchain再生成または終了時に画像単位の描画完了Semaphoreを破棄する。
/// </summary>
void VulkanRenderer::DestroyRenderFinishedSemaphores()
{
	if (device_ == VK_NULL_HANDLE) return;
	for (const auto semaphore : renderFinishedSemaphores_)
		if (semaphore != VK_NULL_HANDLE) vkDestroySemaphore(device_, semaphore, nullptr);
	renderFinishedSemaphores_.clear();
}

/// <summary>
/// 同じフレーム用同期オブジェクトをGPU完了前に再利用しないためFenceを待機する。
/// </summary>
void VulkanRenderer::DrawFrame()
{
	FrameSync& frame = frameSyncs_[currentFrame_];
	VK_CHECK(vkWaitForFences(device_, 1, &frame.inFlight, VK_TRUE, UINT64_MAX));
	std::uint32_t imageIndex = 0;
	const VkResult acquired = vkAcquireNextImageKHR(device_, swapchain_, UINT64_MAX, frame.imageAvailable, VK_NULL_HANDLE, &imageIndex);
	if (acquired == VK_ERROR_OUT_OF_DATE_KHR) return;
	if (acquired != VK_SUCCESS && acquired != VK_SUBOPTIMAL_KHR) VK_CHECK(acquired);
	const VkSemaphore renderFinished = renderFinishedSemaphores_[imageIndex];
	/* 取得画像が過去の未完了フレームに属する場合も、そのFenceを待機する。 */
	if (imagesInFlight_[imageIndex] != VK_NULL_HANDLE) VK_CHECK(vkWaitForFences(device_, 1, &imagesInFlight_[imageIndex], VK_TRUE, UINT64_MAX));
	imagesInFlight_[imageIndex] = frame.inFlight;
	VK_CHECK(vkResetFences(device_, 1, &frame.inFlight));
	VK_CHECK(vkResetCommandBuffer(commandBuffers_[currentFrame_], 0));
	RecordCommandBuffer(commandBuffers_[currentFrame_], imageIndex);

	VkSemaphoreSubmitInfo wait{ VK_STRUCTURE_TYPE_SEMAPHORE_SUBMIT_INFO };
	wait.semaphore = frame.imageAvailable; wait.stageMask = VK_PIPELINE_STAGE_2_ALL_COMMANDS_BIT;
	VkCommandBufferSubmitInfo command{ VK_STRUCTURE_TYPE_COMMAND_BUFFER_SUBMIT_INFO };
	command.commandBuffer = commandBuffers_[currentFrame_];
	VkSemaphoreSubmitInfo signal{ VK_STRUCTURE_TYPE_SEMAPHORE_SUBMIT_INFO };
	signal.semaphore = renderFinished; signal.stageMask = VK_PIPELINE_STAGE_2_ALL_GRAPHICS_BIT;
	VkSubmitInfo2 submit{ VK_STRUCTURE_TYPE_SUBMIT_INFO_2 };
	submit.waitSemaphoreInfoCount = 1; submit.pWaitSemaphoreInfos = &wait;
	submit.commandBufferInfoCount = 1; submit.pCommandBufferInfos = &command;
	submit.signalSemaphoreInfoCount = 1; submit.pSignalSemaphoreInfos = &signal;
	VK_CHECK(vkQueueSubmit2(graphicsQueue_, 1, &submit, frame.inFlight));

	VkPresentInfoKHR present{ VK_STRUCTURE_TYPE_PRESENT_INFO_KHR };
	present.waitSemaphoreCount = 1; present.pWaitSemaphores = &renderFinished;
	present.swapchainCount = 1; present.pSwapchains = &swapchain_; present.pImageIndices = &imageIndex;
	const VkResult result = vkQueuePresentKHR(presentQueue_, &present);
	if (result != VK_SUCCESS && result != VK_SUBOPTIMAL_KHR && result != VK_ERROR_OUT_OF_DATE_KHR) VK_CHECK(result);
	currentFrame_ = (currentFrame_ + 1) % kMaxFramesInFlight;
}

/// <summary>
/// scene描画後にDLAAまたはTAAを適用し、最終結果をSwapchainへ提示するコマンドを記録する。
/// </summary>
void VulkanRenderer::RecordCommandBuffer(VkCommandBuffer commandBuffer, std::uint32_t imageIndex)
{
	VkCommandBufferBeginInfo begin{ VK_STRUCTURE_TYPE_COMMAND_BUFFER_BEGIN_INFO };
	begin.flags = VK_COMMAND_BUFFER_USAGE_ONE_TIME_SUBMIT_BIT;
	VK_CHECK(vkBeginCommandBuffer(commandBuffer, &begin));
	const auto setViewportAndScissor = [&]()
	{
		VkViewport viewport{};
		viewport.width = static_cast<float>(swapchainExtent_.width); viewport.height = static_cast<float>(swapchainExtent_.height); viewport.maxDepth = 1.0f;
		VkRect2D scissor{}; scissor.extent = swapchainExtent_;
		vkCmdSetViewport(commandBuffer, 0, 1, &viewport); vkCmdSetScissor(commandBuffer, 0, 1, &scissor);
	};
	const auto beginColorRendering = [&](VkImageView view, VkImageLayout layout, VkClearColorValue clearColor)
	{
		VkRenderingAttachmentInfo color{ VK_STRUCTURE_TYPE_RENDERING_ATTACHMENT_INFO };
		color.imageView = view; color.imageLayout = layout; color.loadOp = VK_ATTACHMENT_LOAD_OP_CLEAR; color.storeOp = VK_ATTACHMENT_STORE_OP_STORE;
		color.clearValue.color = clearColor;
		VkRenderingInfo rendering{ VK_STRUCTURE_TYPE_RENDERING_INFO };
		rendering.renderArea.extent = swapchainExtent_; rendering.layerCount = 1;
		rendering.colorAttachmentCount = 1; rendering.pColorAttachments = &color;
		vkCmdBeginRendering(commandBuffer, &rendering);
	};
	VkImageLayout swapchainLayout = VK_IMAGE_LAYOUT_UNDEFINED;
	const auto transitionSwapchain = [&](VkImageLayout newLayout)
	{
		VkImageMemoryBarrier2 barrier{ VK_STRUCTURE_TYPE_IMAGE_MEMORY_BARRIER_2 };
		barrier.srcStageMask = swapchainLayout == VK_IMAGE_LAYOUT_UNDEFINED ? VK_PIPELINE_STAGE_2_NONE : VK_PIPELINE_STAGE_2_ALL_COMMANDS_BIT;
		barrier.srcAccessMask = swapchainLayout == VK_IMAGE_LAYOUT_UNDEFINED ? VK_ACCESS_2_NONE : VK_ACCESS_2_MEMORY_WRITE_BIT;
		barrier.dstStageMask = VK_PIPELINE_STAGE_2_ALL_COMMANDS_BIT;
		barrier.dstAccessMask = VK_ACCESS_2_MEMORY_READ_BIT | VK_ACCESS_2_MEMORY_WRITE_BIT;
		barrier.oldLayout = swapchainLayout; barrier.newLayout = newLayout; barrier.image = swapchainImages_[imageIndex];
		barrier.subresourceRange.aspectMask = VK_IMAGE_ASPECT_COLOR_BIT; barrier.subresourceRange.levelCount = 1; barrier.subresourceRange.layerCount = 1;
		VkDependencyInfo dependency{ VK_STRUCTURE_TYPE_DEPENDENCY_INFO };
		dependency.imageMemoryBarrierCount = 1; dependency.pImageMemoryBarriers = &barrier;
		vkCmdPipelineBarrier2(commandBuffer, &dependency);
		swapchainLayout = newLayout;
	};

	TransitionImage(commandBuffer, sceneColor_, VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL);
	beginColorRendering(sceneColor_.view, VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL, { { 0.035f, 0.055f, 0.100f, 1.0f } });
	setViewportAndScissor();
	vkCmdBindPipeline(commandBuffer, VK_PIPELINE_BIND_POINT_GRAPHICS, scenePipeline_);
	vkCmdBindDescriptorSets(commandBuffer, VK_PIPELINE_BIND_POINT_GRAPHICS, texturePipelineLayout_, 0, 1, &textureDescriptorSet_, 0, nullptr);
	const TextureDrawConstants portraitConstants{
		static_cast<float>(textureWidth_) / static_cast<float>(swapchainExtent_.width),
		static_cast<float>(textureHeight_) / static_cast<float>(swapchainExtent_.height), 0.0f, 0.0f };
	vkCmdPushConstants(commandBuffer, texturePipelineLayout_, VK_SHADER_STAGE_VERTEX_BIT | VK_SHADER_STAGE_FRAGMENT_BIT, 0, sizeof(portraitConstants), &portraitConstants);
	vkCmdDraw(commandBuffer, 6, 1, 0, 0);
	vkCmdEndRendering(commandBuffer);
	TransitionImage(commandBuffer, sceneColor_, VK_IMAGE_LAYOUT_GENERAL);

	if (dlssEnabled_)
	{
		TransitionImage(commandBuffer, dlssDepth_, VK_IMAGE_LAYOUT_GENERAL);
		TransitionImage(commandBuffer, dlssMotionVectors_, VK_IMAGE_LAYOUT_GENERAL);
		const VkImageSubresourceRange depthRange{ VK_IMAGE_ASPECT_DEPTH_BIT, 0, 1, 0, 1 };
		const VkImageSubresourceRange colorRange{ VK_IMAGE_ASPECT_COLOR_BIT, 0, 1, 0, 1 };
		VkClearDepthStencilValue clearDepth{ 1.0f, 0 };
		VkClearColorValue clearMotion{};
		vkCmdClearDepthStencilImage(commandBuffer, dlssDepth_.image, VK_IMAGE_LAYOUT_GENERAL, &clearDepth, 1, &depthRange);
		vkCmdClearColorImage(commandBuffer, dlssMotionVectors_.image, VK_IMAGE_LAYOUT_GENERAL, &clearMotion, 1, &colorRange);
		transitionSwapchain(VK_IMAGE_LAYOUT_GENERAL);
		if (EvaluateDlss(commandBuffer, imageIndex))
		{
			transitionSwapchain(VK_IMAGE_LAYOUT_PRESENT_SRC_KHR);
			taaHistoryValid_ = true;
			VK_CHECK(vkEndCommandBuffer(commandBuffer));
			return;
		}
	}

	const std::uint32_t historyReadIndex = taaHistoryIndex_;
	const std::uint32_t historyWriteIndex = 1 - historyReadIndex;
	TransitionImage(commandBuffer, taaHistory_[historyReadIndex], VK_IMAGE_LAYOUT_GENERAL);
	TransitionImage(commandBuffer, taaHistory_[historyWriteIndex], VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL);
	beginColorRendering(taaHistory_[historyWriteIndex].view, VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL, { { 0.0f, 0.0f, 0.0f, 1.0f } });
	setViewportAndScissor();
	vkCmdBindPipeline(commandBuffer, VK_PIPELINE_BIND_POINT_GRAPHICS, taaPipeline_);
	vkCmdBindDescriptorSets(commandBuffer, VK_PIPELINE_BIND_POINT_GRAPHICS, taaPipelineLayout_, 0, 1, &taaDescriptorSets_[historyReadIndex], 0, nullptr);
	const TextureDrawConstants taaConstants{ 1.0f, 1.0f, 0.0f, taaHistoryValid_ ? 0.90f : 0.0f };
	vkCmdPushConstants(commandBuffer, taaPipelineLayout_, VK_SHADER_STAGE_VERTEX_BIT | VK_SHADER_STAGE_FRAGMENT_BIT, 0, sizeof(taaConstants), &taaConstants);
	vkCmdDraw(commandBuffer, 6, 1, 0, 0);
	vkCmdEndRendering(commandBuffer);
	TransitionImage(commandBuffer, taaHistory_[historyWriteIndex], VK_IMAGE_LAYOUT_GENERAL);

	transitionSwapchain(VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL);
	beginColorRendering(swapchainImageViews_[imageIndex], VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL, { { 0.035f, 0.055f, 0.100f, 1.0f } });
	setViewportAndScissor();
	vkCmdBindPipeline(commandBuffer, VK_PIPELINE_BIND_POINT_GRAPHICS, presentPipeline_);
	vkCmdBindDescriptorSets(commandBuffer, VK_PIPELINE_BIND_POINT_GRAPHICS, texturePipelineLayout_, 0, 1, &historyDescriptorSets_[historyWriteIndex], 0, nullptr);
	const TextureDrawConstants presentConstants{ 1.0f, 1.0f, 1.0f, 0.0f };
	vkCmdPushConstants(commandBuffer, texturePipelineLayout_, VK_SHADER_STAGE_VERTEX_BIT | VK_SHADER_STAGE_FRAGMENT_BIT, 0, sizeof(presentConstants), &presentConstants);
	vkCmdDraw(commandBuffer, 6, 1, 0, 0);
	vkCmdEndRendering(commandBuffer);
	transitionSwapchain(VK_IMAGE_LAYOUT_PRESENT_SRC_KHR);
	taaHistoryIndex_ = historyWriteIndex; taaHistoryValid_ = true;
	VK_CHECK(vkEndCommandBuffer(commandBuffer));
}

/// <summary>
/// 最小化時の0x0サイズではSwapchainを作れないため、復元時のWM_SIZEまで待機する。
/// </summary>
void VulkanRenderer::RecreateSwapchain(std::uint32_t width, std::uint32_t height)
{
	if (width == 0 || height == 0) return;
	WaitUntilIdle(); windowWidth_ = width; windowHeight_ = height;
	DestroyRenderFinishedSemaphores(); DestroySwapchainResources();
	CreateSwapchain(); CreateSwapchainImageViews(); CreateAntiAliasingResources(); CreateDescriptorPoolAndSet(); CreateTaaDescriptorResources();
	CreateGraphicsPipeline(); CreateRenderFinishedSemaphores();
}

/// <summary>
/// 全GPU処理の完了を待機し、資源破棄またはSwapchain再生成を可能にする。
/// </summary>
void VulkanRenderer::WaitUntilIdle() const
{
	if (device_ != VK_NULL_HANDLE) VK_CHECK(vkDeviceWaitIdle(device_));
}

/// <summary>
/// Swapchain本体より先に、その画像を参照するImageViewを破棄する。
/// </summary>
void VulkanRenderer::DestroySwapchainResources()
{
	if (presentPipeline_ != VK_NULL_HANDLE) vkDestroyPipeline(device_, presentPipeline_, nullptr);
	if (taaPipeline_ != VK_NULL_HANDLE) vkDestroyPipeline(device_, taaPipeline_, nullptr);
	if (scenePipeline_ != VK_NULL_HANDLE) vkDestroyPipeline(device_, scenePipeline_, nullptr);
	presentPipeline_ = VK_NULL_HANDLE; taaPipeline_ = VK_NULL_HANDLE; scenePipeline_ = VK_NULL_HANDLE;
	DestroyDescriptorResources(); DestroyAntiAliasingResources();
	for (const auto view : swapchainImageViews_) vkDestroyImageView(device_, view, nullptr);
	swapchainImageViews_.clear(); swapchainImages_.clear(); imagesInFlight_.clear();
	if (swapchain_ != VK_NULL_HANDLE) vkDestroySwapchainKHR(device_, swapchain_, nullptr);
	swapchain_ = VK_NULL_HANDLE; swapchainSupportsStorage_ = false;
}

/// <summary>
/// フレームごとのSemaphoreとFenceを破棄し、ハンドルを初期化する。
/// </summary>
void VulkanRenderer::DestroySyncObjects()
{
	if (device_ == VK_NULL_HANDLE) return;
	DestroyRenderFinishedSemaphores();
	for (auto& frame : frameSyncs_)
	{
		if (frame.imageAvailable != VK_NULL_HANDLE) vkDestroySemaphore(device_, frame.imageAvailable, nullptr);
		if (frame.inFlight != VK_NULL_HANDLE) vkDestroyFence(device_, frame.inFlight, nullptr);
		frame = {};
	}
}

/// <summary>
/// Overlayや将来のDLSSがStorage用途を追加できるBGRA8 UNORMを優先する。
/// </summary>
VkSurfaceFormatKHR VulkanRenderer::ChooseSwapSurfaceFormat(const std::vector<VkSurfaceFormatKHR>& formats) const
{
	for (const auto& format : formats)
		if (format.format == VK_FORMAT_B8G8R8A8_UNORM && format.colorSpace == VK_COLOR_SPACE_SRGB_NONLINEAR_KHR) return format;
	return formats.front();
}

/// <summary>
/// 低遅延のMAILBOXを優先し、必須のFIFOへフォールバックする。
/// </summary>
VkPresentModeKHR VulkanRenderer::ChooseSwapchainPresentMode(const std::vector<VkPresentModeKHR>& modes) const
{
	for (const auto mode : modes) if (mode == VK_PRESENT_MODE_MAILBOX_KHR) return mode;
	return VK_PRESENT_MODE_FIFO_KHR;
}

/// <summary>
/// Surface指定サイズまたは現在のクライアントサイズからSwapchain寸法を決定する。
/// </summary>
VkExtent2D VulkanRenderer::ChooseSwapchainExtent(const VkSurfaceCapabilitiesKHR& capabilities) const
{
	if (capabilities.currentExtent.width != UINT32_MAX) return capabilities.currentExtent;
	return { std::clamp(windowWidth_, capabilities.minImageExtent.width, capabilities.maxImageExtent.width),
		std::clamp(windowHeight_, capabilities.minImageExtent.height, capabilities.maxImageExtent.height) };
}

/// <summary>
/// Win32表示とDebug診断に必要なVulkan Instance拡張名を列挙する。
/// </summary>
std::vector<const char*> VulkanRenderer::GetRequiredInstanceExtensions() const
{
	std::vector<const char*> result{ VK_KHR_SURFACE_EXTENSION_NAME, VK_KHR_WIN32_SURFACE_EXTENSION_NAME };
	if constexpr (kDebugToolsEnabled) result.push_back(VK_EXT_DEBUG_UTILS_EXTENSION_NAME);
	for (const auto& extension : dlssInstanceExtensions_)
		if (std::none_of(result.begin(), result.end(), [&](const char* current) { return std::strcmp(current, extension.c_str()) == 0; })) result.push_back(extension.c_str());
	return result;
}

/// <summary>
/// Swapchainに加え、NGXが問い合わせたDLSS用Device拡張を重複なく列挙する。
/// </summary>
std::vector<const char*> VulkanRenderer::GetRequiredDeviceExtensions() const
{
	std::vector<const char*> result(kDeviceExtensions.begin(), kDeviceExtensions.end());
	for (const auto& extension : dlssDeviceExtensions_)
		if (std::none_of(result.begin(), result.end(), [&](const char* current) { return std::strcmp(current, extension.c_str()) == 0; })) result.push_back(extension.c_str());
	return result;
}

/// <summary>
/// Debug構成で要求するKhronos Validation Layerがインストール済みか検証する。
/// </summary>
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

/// <summary>
/// Instance生成時と生成後で共用するDebug Messenger設定を組み立てる。
/// </summary>
VkDebugUtilsMessengerCreateInfoEXT VulkanRenderer::MakeDebugMessengerCreateInfo()
{
	VkDebugUtilsMessengerCreateInfoEXT info{ VK_STRUCTURE_TYPE_DEBUG_UTILS_MESSENGER_CREATE_INFO_EXT };
	info.messageSeverity = VK_DEBUG_UTILS_MESSAGE_SEVERITY_WARNING_BIT_EXT | VK_DEBUG_UTILS_MESSAGE_SEVERITY_ERROR_BIT_EXT;
	info.messageType = VK_DEBUG_UTILS_MESSAGE_TYPE_GENERAL_BIT_EXT | VK_DEBUG_UTILS_MESSAGE_TYPE_VALIDATION_BIT_EXT | VK_DEBUG_UTILS_MESSAGE_TYPE_PERFORMANCE_BIT_EXT;
	info.pfnUserCallback = DebugCallback;
	return info;
}

/// <summary>
/// Vulkan Validation Layerの警告・エラーをVisual Studio出力へ転送する。
/// </summary>
VKAPI_ATTR VkBool32 VKAPI_CALL VulkanRenderer::DebugCallback(VkDebugUtilsMessageSeverityFlagBitsEXT, VkDebugUtilsMessageTypeFlagsEXT, const VkDebugUtilsMessengerCallbackDataEXT* data, void*)
{
	OutputDebugStringA("[Vulkan] "); OutputDebugStringA(data->pMessage); OutputDebugStringA("\n");
	AppendValidationLog(data->pMessage);
	return VK_FALSE;
}

/// <summary>
/// Instance拡張関数を取得してDebug Messengerを生成する。
/// </summary>
VkResult VulkanRenderer::CreateDebugUtilsMessenger(const VkDebugUtilsMessengerCreateInfoEXT* info)
{
	auto function = reinterpret_cast<PFN_vkCreateDebugUtilsMessengerEXT>(vkGetInstanceProcAddr(instance_, "vkCreateDebugUtilsMessengerEXT"));
	return function == nullptr ? VK_ERROR_EXTENSION_NOT_PRESENT : function(instance_, info, nullptr, &debugMessenger_);
}

/// <summary>
/// 生成済みDebug Messengerを拡張関数経由で破棄する。
/// </summary>
void VulkanRenderer::DestroyDebugUtilsMessenger()
{
	if (debugMessenger_ == VK_NULL_HANDLE || instance_ == VK_NULL_HANDLE) return;
	auto function = reinterpret_cast<PFN_vkDestroyDebugUtilsMessengerEXT>(vkGetInstanceProcAddr(instance_, "vkDestroyDebugUtilsMessengerEXT"));
	if (function != nullptr) function(instance_, debugMessenger_, nullptr);
	debugMessenger_ = VK_NULL_HANDLE;
}
