#pragma once

#include "ImageLoader.hpp"
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
 * Dynamic Renderingでスワップチェーン画像をクリアし、RGBA8テクスチャを描画する。
 */
class VulkanRenderer final
{
public:
	VulkanRenderer(HWND hwnd, std::uint32_t width, std::uint32_t height, const ImageRgba8& texture);
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
	/** TAA履歴をフレーム間で安全に共有するため、現段階は1フレームずつ完了させる。 */
	static constexpr std::uint32_t kMaxFramesInFlight = 1;

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
		/** CPUがこのフレーム用コマンドバッファを再利用するための待機対象。 */
		VkFence inFlight = VK_NULL_HANDLE;
	};

	/** Vertex/Fragment Shaderへ渡す、表示矩形と出力エンコードの設定。 */
	struct TextureDrawConstants
	{
		float halfWidthNdc = 0.0f;
		float halfHeightNdc = 0.0f;
		float encodeToUnorm = 0.0f;
		float historyWeight = 0.0f;
	};

	/** Swapchainの解像度に従属する、Vulkan画像・メモリ・Viewの所有単位。 */
	struct PostProcessImage
	{
		VkImage image = VK_NULL_HANDLE;
		VkDeviceMemory memory = VK_NULL_HANDLE;
		VkImageView view = VK_NULL_HANDLE;
		VkFormat format = VK_FORMAT_UNDEFINED;
		VkImageLayout layout = VK_IMAGE_LAYOUT_UNDEFINED;
		VkImageAspectFlags aspectMask = VK_IMAGE_ASPECT_COLOR_BIT;
	};

	void CreateInstance();
	void QueryDlssInstanceExtensions();
	void SetupDebugMessenger();
	void CreateSurface(HWND windowHandle);

	void PickPhysicalDevice();
	void QueryDlssDeviceExtensions();
	bool IsDeviceSuitable(VkPhysicalDevice device) const;
	bool CheckDeviceExtensionSupport(VkPhysicalDevice device) const;

	QueueFamilyIndices FindQueueFamilies(VkPhysicalDevice device) const;
	SwapchainSupportDetails QuerySwapchainSupport(VkPhysicalDevice device) const;

	void CreateLogicalDevice();
	void CreateSwapchain();
	void CreateSwapchainImageViews();
	void CreateTexture(const ImageRgba8& texture);
	void DestroyTexture();
	void CreateDescriptorSetLayout();
	void CreateDescriptorPoolAndSet();
	void CreateTaaDescriptorResources();
	void DestroyDescriptorResources();
	std::uint32_t FindMemoryType(std::uint32_t typeFilter, VkMemoryPropertyFlags properties) const;
	void CreateBuffer(VkDeviceSize size, VkBufferUsageFlags usage, VkMemoryPropertyFlags properties, VkBuffer& buffer, VkDeviceMemory& memory) const;
	void CreatePostProcessImage(VkFormat format, VkImageUsageFlags usage, VkImageAspectFlags aspectMask, PostProcessImage& image);
	void DestroyPostProcessImage(PostProcessImage& image);
	void CreateAntiAliasingResources();
	void DestroyAntiAliasingResources();
	void TransitionImage(VkCommandBuffer commandBuffer, PostProcessImage& image, VkImageLayout newLayout) const;
	VkCommandBuffer BeginSingleTimeCommands() const;
	void EndSingleTimeCommands(VkCommandBuffer commandBuffer) const;
	void TransitionTextureLayout(VkImageLayout oldLayout, VkImageLayout newLayout) const;
	void CopyBufferToTexture(VkBuffer buffer, std::uint32_t width, std::uint32_t height) const;
	void CreateGraphicsPipeline();
	void CreateGraphicsPipelineForTarget(VkShaderModule vertexShader, VkShaderModule fragmentShader, VkPipelineLayout pipelineLayout, VkFormat targetFormat, bool alphaBlend, VkPipeline& pipeline) const;
	VkShaderModule CreateShaderModule(const std::vector<std::uint32_t>& code) const;
	void InitializeDlss();
	void CreateDlssFeature();
	void ReleaseDlssFeature();
	void ShutdownDlss();
	bool EvaluateDlss(VkCommandBuffer commandBuffer, std::uint32_t imageIndex);

	void CreateCommandPool();
	void AllocateCommandBuffers();
	void CreateSyncObjects();
	void CreateRenderFinishedSemaphores();
	void DestroyRenderFinishedSemaphores();

	/**
	 * @brief scene描画後にDLAAまたはTAAを適用し、Swapchainへ提示するコマンドを記録する。
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
	std::vector<const char*> GetRequiredDeviceExtensions() const;
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
	std::vector<std::string> dlssInstanceExtensions_;
	std::vector<std::string> dlssDeviceExtensions_;
	bool dlssExtensionRequirementsAvailable_ = false;

	std::uint32_t graphicsQueueFamily_ = 0;
	std::uint32_t presentQueueFamily_ = 0;

	VkQueue graphicsQueue_ = VK_NULL_HANDLE;
	VkQueue presentQueue_ = VK_NULL_HANDLE;

	VkSwapchainKHR swapchain_ = VK_NULL_HANDLE;
	VkFormat swapchainImageFormat_ = VK_FORMAT_UNDEFINED;
	VkExtent2D swapchainExtent_{};
	bool swapchainSupportsStorage_ = false;

	std::vector<VkImage> swapchainImages_;
	std::vector<VkImageView> swapchainImageViews_;
	/** 各スワップチェーン画像を最後に使用したフレームFence。画像の早期再利用を防ぐ。 */
	std::vector<VkFence> imagesInFlight_;

	VkCommandPool commandPool_ = VK_NULL_HANDLE;
	std::vector<VkCommandBuffer> commandBuffers_;

	/** 単一テクスチャのscene/present描画に使うパイプラインレイアウト。 */
	VkPipelineLayout texturePipelineLayout_ = VK_NULL_HANDLE;
	/** sceneへ透過画像を描画するパイプライン。 */
	VkPipeline scenePipeline_ = VK_NULL_HANDLE;
	/** TAA結果をSwapchainへ描画するパイプライン。 */
	VkPipeline presentPipeline_ = VK_NULL_HANDLE;
	/** sceneと履歴を同時に読むTAA用パイプラインレイアウト。 */
	VkPipelineLayout taaPipelineLayout_ = VK_NULL_HANDLE;
	/** TAA履歴を合成して次の履歴画像へ書き込むパイプライン。 */
	VkPipeline taaPipeline_ = VK_NULL_HANDLE;
	/** WICで復号したRGBA8を保持するDevice Local画像。 */
	VkImage textureImage_ = VK_NULL_HANDLE;
	VkDeviceMemory textureMemory_ = VK_NULL_HANDLE;
	VkImageView textureImageView_ = VK_NULL_HANDLE;
	VkSampler textureSampler_ = VK_NULL_HANDLE;
	VkDescriptorSetLayout textureDescriptorSetLayout_ = VK_NULL_HANDLE;
	VkDescriptorPool textureDescriptorPool_ = VK_NULL_HANDLE;
	VkDescriptorSet textureDescriptorSet_ = VK_NULL_HANDLE;
	std::array<VkDescriptorSet, 2> historyDescriptorSets_{};
	VkDescriptorSetLayout taaDescriptorSetLayout_ = VK_NULL_HANDLE;
	VkDescriptorPool taaDescriptorPool_ = VK_NULL_HANDLE;
	std::array<VkDescriptorSet, 2> taaDescriptorSets_{};
	/** 元画像の物理ピクセル幅。Swapchain上の1:1表示に使用する。 */
	std::uint32_t textureWidth_ = 0;
	/** 元画像の物理ピクセル高。Swapchain上の1:1表示に使用する。 */
	std::uint32_t textureHeight_ = 0;
	/** sceneは線形カラー、履歴はTAAの前フレーム出力を保持する。 */
	PostProcessImage sceneColor_{};
	std::array<PostProcessImage, 2> taaHistory_{};
	/** DLSSが有効な環境だけで確保する深度とモーションベクトル。 */
	PostProcessImage dlssDepth_{};
	PostProcessImage dlssMotionVectors_{};
	std::uint32_t taaHistoryIndex_ = 0;
	bool taaHistoryValid_ = false;
	NVSDK_NGX_Parameter* dlssParameters_ = nullptr;
	NVSDK_NGX_Handle* dlssFeature_ = nullptr;
	bool dlssInitialized_ = false;
	bool dlssEnabled_ = false;

	std::array<FrameSync, kMaxFramesInFlight> frameSyncs_{};
	/** Present待機中の再利用を防ぐため、Swapchain画像ごとに所有する描画完了Semaphore。 */
	std::vector<VkSemaphore> renderFinishedSemaphores_;
	std::uint32_t currentFrame_ = 0;
};
