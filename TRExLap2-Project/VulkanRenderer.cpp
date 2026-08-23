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

VkCompositeAlphaFlagBitsKHR ChooseCompositeAlpha(const VkSurfaceCapabilitiesKHR& capabilities)
{
	constexpr std::array<VkCompositeAlphaFlagBitsKHR, 4> choices{
		VK_COMPOSITE_ALPHA_OPAQUE_BIT_KHR, VK_COMPOSITE_ALPHA_PRE_MULTIPLIED_BIT_KHR,
		VK_COMPOSITE_ALPHA_POST_MULTIPLIED_BIT_KHR, VK_COMPOSITE_ALPHA_INHERIT_BIT_KHR };
	for (const auto choice : choices)
		if ((capabilities.supportedCompositeAlpha & choice) != 0) return choice;
	throw std::runtime_error("No supported swapchain composite alpha mode.");
}
}

VulkanRenderer::VulkanRenderer(HWND hwnd, std::uint32_t width, std::uint32_t height)
	: windowHandle_(hwnd), windowWidth_(width), windowHeight_(height)
{
	CreateInstance(); SetupDebugMessenger(); CreateSurface(hwnd); PickPhysicalDevice();
	CreateLogicalDevice(); CreateSwapchain(); CreateSwapchainImageViews();
	CreateCommandPool(); AllocateCommandBuffers(); CreateSyncObjects();
}

VulkanRenderer::~VulkanRenderer()
{
	WaitUntilIdle(); DestroySyncObjects(); DestroySwapchainResources();
	if (commandPool_ != VK_NULL_HANDLE) vkDestroyCommandPool(device_, commandPool_, nullptr);
	if (device_ != VK_NULL_HANDLE) vkDestroyDevice(device_, nullptr);
	DestroyDebugUtilsMessenger();
	if (surface_ != VK_NULL_HANDLE) vkDestroySurfaceKHR(instance_, surface_, nullptr);
	if (instance_ != VK_NULL_HANDLE) vkDestroyInstance(instance_, nullptr);
}

bool VulkanRenderer::QueueFamilyIndices::IsComplete() const noexcept
{
	return graphicsFamily.has_value() && presentFamily.has_value();
}

void VulkanRenderer::CreateInstance()
{
	/** Vulkan 1.3と、Win32表示・Debug診断に必要なInstance拡張を有効化する。 */
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

void VulkanRenderer::SetupDebugMessenger()
{
	if constexpr (kDebugToolsEnabled)
	{
		const auto info = MakeDebugMessengerCreateInfo();
		VK_CHECK(CreateDebugUtilsMessenger(&info));
	}
}

void VulkanRenderer::CreateSurface(HWND hwnd)
{
	VkWin32SurfaceCreateInfoKHR info{ VK_STRUCTURE_TYPE_WIN32_SURFACE_CREATE_INFO_KHR };
	info.hinstance = GetModuleHandleW(nullptr);
	info.hwnd = hwnd;
	VK_CHECK(vkCreateWin32SurfaceKHR(instance_, &info, nullptr, &surface_));
}

void VulkanRenderer::PickPhysicalDevice()
{
	/** 描画・表示・Swapchain・Vulkan 1.3機能を満たす最初のGPUを採用する。 */
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

void VulkanRenderer::CreateLogicalDevice()
{
	/** Dynamic RenderingとSynchronization 2を必須のDevice機能として有効化する。 */
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

void VulkanRenderer::CreateSwapchain()
{
	/** Swapchain画像はAcquire後からPresent前だけ、アプリ側が描画に使用できる。 */
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

void VulkanRenderer::CreateCommandPool()
{
	VkCommandPoolCreateInfo info{ VK_STRUCTURE_TYPE_COMMAND_POOL_CREATE_INFO };
	info.flags = VK_COMMAND_POOL_CREATE_RESET_COMMAND_BUFFER_BIT; info.queueFamilyIndex = graphicsQueueFamily_;
	VK_CHECK(vkCreateCommandPool(device_, &info, nullptr, &commandPool_));
}

void VulkanRenderer::AllocateCommandBuffers()
{
	commandBuffers_.resize(kMaxFramesInFlight);
	VkCommandBufferAllocateInfo info{ VK_STRUCTURE_TYPE_COMMAND_BUFFER_ALLOCATE_INFO };
	info.commandPool = commandPool_; info.level = VK_COMMAND_BUFFER_LEVEL_PRIMARY; info.commandBufferCount = kMaxFramesInFlight;
	VK_CHECK(vkAllocateCommandBuffers(device_, &info, commandBuffers_.data()));
}

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

void VulkanRenderer::DrawFrame()
{
	/** 同じフレーム用同期オブジェクトをGPU完了前に再利用しないためFenceを待機する。 */
	FrameSync& frame = frameSyncs_[currentFrame_];
	VK_CHECK(vkWaitForFences(device_, 1, &frame.inFlight, VK_TRUE, UINT64_MAX));
	std::uint32_t imageIndex = 0;
	const VkResult acquired = vkAcquireNextImageKHR(device_, swapchain_, UINT64_MAX, frame.imageAvailable, VK_NULL_HANDLE, &imageIndex);
	if (acquired == VK_ERROR_OUT_OF_DATE_KHR) return;
	if (acquired != VK_SUCCESS && acquired != VK_SUBOPTIMAL_KHR) VK_CHECK(acquired);
	/** 取得画像が過去の未完了フレームに属する場合も、そのFenceを待機する。 */
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

void VulkanRenderer::RecordCommandBuffer(VkCommandBuffer commandBuffer, std::uint32_t imageIndex)
{
	/** 画面内容は毎回破棄するため、古い内容を読まないUNDEFINEDから遷移する。 */
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
	/** Dynamic RenderingではRenderPass/Framebufferを生成せず、対象ImageViewを直接指定する。 */
	vkCmdBeginRendering(commandBuffer, &rendering);
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

void VulkanRenderer::RecreateSwapchain(std::uint32_t width, std::uint32_t height)
{
	/** 最小化時の0x0サイズではSwapchainを作れないため、復元時のWM_SIZEまで待機する。 */
	if (width == 0 || height == 0) return;
	WaitUntilIdle(); windowWidth_ = width; windowHeight_ = height;
	DestroySwapchainResources(); CreateSwapchain(); CreateSwapchainImageViews();
}

void VulkanRenderer::WaitUntilIdle() const
{
	if (device_ != VK_NULL_HANDLE) VK_CHECK(vkDeviceWaitIdle(device_));
}

void VulkanRenderer::DestroySwapchainResources()
{
	/** Swapchain本体より先に、その画像を参照するImageViewを破棄する。 */
	for (const auto view : swapchainImageViews_) vkDestroyImageView(device_, view, nullptr);
	swapchainImageViews_.clear(); swapchainImages_.clear(); imagesInFlight_.clear();
	if (swapchain_ != VK_NULL_HANDLE) vkDestroySwapchainKHR(device_, swapchain_, nullptr);
	swapchain_ = VK_NULL_HANDLE;
}

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

VkSurfaceFormatKHR VulkanRenderer::ChooseSwapSurfaceFormat(const std::vector<VkSurfaceFormatKHR>& formats) const
{
	for (const auto& format : formats)
		if (format.format == VK_FORMAT_B8G8R8A8_SRGB && format.colorSpace == VK_COLOR_SPACE_SRGB_NONLINEAR_KHR) return format;
	return formats.front();
}

VkPresentModeKHR VulkanRenderer::ChooseSwapchainPresentMode(const std::vector<VkPresentModeKHR>& modes) const
{
	for (const auto mode : modes) if (mode == VK_PRESENT_MODE_MAILBOX_KHR) return mode;
	return VK_PRESENT_MODE_FIFO_KHR;
}

VkExtent2D VulkanRenderer::ChooseSwapchainExtent(const VkSurfaceCapabilitiesKHR& capabilities) const
{
	if (capabilities.currentExtent.width != UINT32_MAX) return capabilities.currentExtent;
	return { std::clamp(windowWidth_, capabilities.minImageExtent.width, capabilities.maxImageExtent.width),
		std::clamp(windowHeight_, capabilities.minImageExtent.height, capabilities.maxImageExtent.height) };
}

std::vector<const char*> VulkanRenderer::GetRequiredInstanceExtensions() const
{
	std::vector<const char*> result{ VK_KHR_SURFACE_EXTENSION_NAME, VK_KHR_WIN32_SURFACE_EXTENSION_NAME };
	if constexpr (kDebugToolsEnabled) result.push_back(VK_EXT_DEBUG_UTILS_EXTENSION_NAME);
	return result;
}

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

VkDebugUtilsMessengerCreateInfoEXT VulkanRenderer::MakeDebugMessengerCreateInfo()
{
	VkDebugUtilsMessengerCreateInfoEXT info{ VK_STRUCTURE_TYPE_DEBUG_UTILS_MESSENGER_CREATE_INFO_EXT };
	info.messageSeverity = VK_DEBUG_UTILS_MESSAGE_SEVERITY_WARNING_BIT_EXT | VK_DEBUG_UTILS_MESSAGE_SEVERITY_ERROR_BIT_EXT;
	info.messageType = VK_DEBUG_UTILS_MESSAGE_TYPE_GENERAL_BIT_EXT | VK_DEBUG_UTILS_MESSAGE_TYPE_VALIDATION_BIT_EXT | VK_DEBUG_UTILS_MESSAGE_TYPE_PERFORMANCE_BIT_EXT;
	info.pfnUserCallback = DebugCallback;
	return info;
}

VKAPI_ATTR VkBool32 VKAPI_CALL VulkanRenderer::DebugCallback(VkDebugUtilsMessageSeverityFlagBitsEXT, VkDebugUtilsMessageTypeFlagsEXT, const VkDebugUtilsMessengerCallbackDataEXT* data, void*)
{
	OutputDebugStringA("[Vulkan] "); OutputDebugStringA(data->pMessage); OutputDebugStringA("\n");
	return VK_FALSE;
}

VkResult VulkanRenderer::CreateDebugUtilsMessenger(const VkDebugUtilsMessengerCreateInfoEXT* info)
{
	auto function = reinterpret_cast<PFN_vkCreateDebugUtilsMessengerEXT>(vkGetInstanceProcAddr(instance_, "vkCreateDebugUtilsMessengerEXT"));
	return function == nullptr ? VK_ERROR_EXTENSION_NOT_PRESENT : function(instance_, info, nullptr, &debugMessenger_);
}

void VulkanRenderer::DestroyDebugUtilsMessenger()
{
	if (debugMessenger_ == VK_NULL_HANDLE || instance_ == VK_NULL_HANDLE) return;
	auto function = reinterpret_cast<PFN_vkDestroyDebugUtilsMessengerEXT>(vkGetInstanceProcAddr(instance_, "vkDestroyDebugUtilsMessengerEXT"));
	if (function != nullptr) function(instance_, debugMessenger_, nullptr);
	debugMessenger_ = VK_NULL_HANDLE;
}
