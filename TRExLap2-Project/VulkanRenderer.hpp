#pragma once

#include "VulkanCheck.hpp"

#include <array>
#include <cstdint>
#include <optional>
#include <vector>

#include <vulkan/vulkan.h>

#ifndef TREXLAP2_DEBUG_TOOLS
#define TREXLAP2_DEBUG_TOOLS 0
#endif // !TREXLAP2_DEBUG_TOOLS

/**
 * @brief Win32ウィンドウにVulkan 1.3の描画結果を提示する最小レンダラ。
 *
 * 現段階ではDynamic Renderingでスワップチェーン画像をクリアするだけ。
 * シェーダ、パイプライン、頂点バッファは次の描画段階で追加する。
 */
class VulkanRenderer final
{
public:
	VulkanRenderer(HWND hwnd, std::uint32_t width, std::uint32_t height);
	~VulkanRenderer();

	VulkanRenderer(const VulkanRenderer&) = delete;
	VulkanRenderer& operator=(const VulkanRenderer&) = delete;
	VulkanRenderer(VulkanRenderer&&) = delete;
	VulkanRenderer& operator=(VulkanRenderer&&) = delete;

	/** @brief 1フレームを取得、クリア、GPU送信、画面提示する。 */
	void DrawFrame();

	/** @brief ウィンドウリサイズ後にスワップチェーン依存資源を作り直す。 */
	void RecreateSwapchain(std::uint32_t width, std::uint32_t height);

	/** @brief GPUの完了を待つ。破棄やスワップチェーン再生成の直前に使う。 */
	void WaitUntilIdle() const;

private:
	static constexpr std::uint32_t kMaxFramesInFlight = 2;

	struct QueueFamilyIndices
	{
		std::optional<std::uint32_t> graphicsFamily;
		std::optional<std::uint32_t> presentFamily;

		bool IsComplete() const noexcept;
	};

	struct SwapchainSupportDetails
	{
		VkSurfaceCapabilitiesKHR capabilities{};
		std::vector<VkSurfaceFormatKHR> formats;
		std::vector<VkPresentModeKHR> presentModes;
	};

	struct FrameSync
	{
		/** スワップチェーン画像を利用可能になった時に通知される。 */
		VkSemaphore imageAvailable = VK_NULL_HANDLE;
		/** GPUによる描画完了をPresent Queueへ通知する。 */
		VkSemaphore renderFinished = VK_NULL_HANDLE;
		/** CPUがこのフレーム用コマンドバッファを再利用するための待機対象。 */
		VkFence inFlight = VK_NULL_HANDLE;
	};

	void CreateInstance();
	void SetupDebugMessenger();
	void CreateSurface(HWND windowHandle);

	void PickPhysicalDevice();
	bool IsDeviceSuitable(VkPhysicalDevice device) const;
	bool CheckDeviceExtensionSupport(VkPhysicalDevice device) const;

	QueueFamilyIndices FindQueueFamilies(VkPhysicalDevice device) const;
	SwapchainSupportDetails QuerySwapchainSupport(VkPhysicalDevice device) const;

	void CreateLogicalDevice();
	void CreateSwapchain();
	void CreateSwapchainImageViews();

	void CreateCommandPool();
	void AllocateCommandBuffers();
	void CreateSyncObjects();

	/**
	 * @brief スワップチェーン画像を濃紺でクリアするコマンドを記録する。
	 *
	 * PRESENT/UNDEFINED -> COLOR_ATTACHMENT_OPTIMAL -> PRESENT_SRC_KHR の順で
	 * 画像レイアウトを遷移させる。
	 */
	void RecordCommandBuffer(VkCommandBuffer commandBuffer, std::uint32_t imageIndex);

	void DestroySwapchainResources();
	void DestroySyncObjects();

	VkSurfaceFormatKHR ChooseSwapSurfaceFormat(const std::vector<VkSurfaceFormatKHR>& availableFormats) const;
	VkPresentModeKHR ChooseSwapchainPresentMode(const std::vector<VkPresentModeKHR>& availablePresentModes) const;
	VkExtent2D ChooseSwapchainExtent(const VkSurfaceCapabilitiesKHR& capabilities) const;

	std::vector<const char*> GetRequiredInstanceExtensions() const;
	bool CheckValidationLayerSupport() const;
	static VkDebugUtilsMessengerCreateInfoEXT MakeDebugMessengerCreateInfo();

	static VKAPI_ATTR VkBool32 VKAPI_CALL DebugCallback(
		VkDebugUtilsMessageSeverityFlagBitsEXT messageSeverity,
		VkDebugUtilsMessageTypeFlagsEXT messageType,
		const VkDebugUtilsMessengerCallbackDataEXT* pCallbackData,
		void* pUserData);

	VkResult CreateDebugUtilsMessenger(const VkDebugUtilsMessengerCreateInfoEXT* pCreateInfo);

	void DestroyDebugUtilsMessenger();

	HWND windowHandle_ = nullptr;
	std::uint32_t windowWidth_ = 0;
	std::uint32_t windowHeight_ = 0;

	VkInstance instance_ = VK_NULL_HANDLE;
	VkDebugUtilsMessengerEXT debugMessenger_ = VK_NULL_HANDLE;
	VkSurfaceKHR surface_ = VK_NULL_HANDLE;

	VkPhysicalDevice physicalDevice_ = VK_NULL_HANDLE;
	VkDevice device_ = VK_NULL_HANDLE;

	std::uint32_t graphicsQueueFamily_ = 0;
	std::uint32_t presentQueueFamily_ = 0;

	VkQueue graphicsQueue_ = VK_NULL_HANDLE;
	VkQueue presentQueue_ = VK_NULL_HANDLE;

	VkSwapchainKHR swapchain_ = VK_NULL_HANDLE;
	VkFormat swapchainImageFormat_ = VK_FORMAT_UNDEFINED;
	VkExtent2D swapchainExtent_{};

	std::vector<VkImage> swapchainImages_;
	std::vector<VkImageView> swapchainImageViews_;
	/** 各スワップチェーン画像を最後に使用したフレームFence。画像の早期再利用を防ぐ。 */
	std::vector<VkFence> imagesInFlight_;

	VkCommandPool commandPool_ = VK_NULL_HANDLE;
	std::vector<VkCommandBuffer> commandBuffers_;

	std::array<FrameSync, kMaxFramesInFlight> frameSyncs_{};
	std::uint32_t currentFrame_ = 0;
};
