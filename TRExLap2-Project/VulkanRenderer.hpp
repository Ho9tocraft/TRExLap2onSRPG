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
 * Dynamic Renderingでスワップチェーン画像をクリアし、検証用の三角形を描画する。
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
	void CreateTexture();
	void DestroyTexture();
	void CreateDescriptorSetLayout();
	void CreateDescriptorPoolAndSet();
	void CreateGraphicsPipeline();
	VkShaderModule CreateShaderModule(const std::vector<std::uint32_t>& code) const;

	void CreateCommandPool();
	void AllocateCommandBuffers();
	void CreateSyncObjects();

	/**
	 * @brief スワップチェーン画像を濃紺でクリアし、三角形を描画するコマンドを記録する。
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

	/** Dynamic Rendering用の空のDescriptor Set Layoutを持つパイプラインレイアウト。 */
	VkPipelineLayout graphicsPipelineLayout_ = VK_NULL_HANDLE;
	/** 頂点バッファを使わず、頂点シェーダがgl_VertexIndexから三頂点を生成するパイプライン。 */
	VkPipeline graphicsPipeline_ = VK_NULL_HANDLE;
	VkImage textureImage_ = VK_NULL_HANDLE;
	VkDeviceMemory textureMemory_ = VK_NULL_HANDLE;
	VkImageView textureImageView_ = VK_NULL_HANDLE;
	VkSampler textureSampler_ = VK_NULL_HANDLE;
	VkDescriptorSetLayout textureDescriptorSetLayout_ = VK_NULL_HANDLE;
	VkDescriptorPool textureDescriptorPool_ = VK_NULL_HANDLE;
	VkDescriptorSet textureDescriptorSet_ = VK_NULL_HANDLE;

	std::array<FrameSync, kMaxFramesInFlight> frameSyncs_{};
	std::uint32_t currentFrame_ = 0;
};
