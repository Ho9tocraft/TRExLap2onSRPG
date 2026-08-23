# Vulkan初回表示の実装メモ

状態: draft  
対象: x64 Debug、Windowsサブシステム

## 到達点

Win32ウィンドウ上に、Vulkan 1.3 Dynamic Renderingで濃紺のクリア画面を表示する。シェーダ、頂点バッファ、テクスチャ、ImGuiはまだ導入しない。

## 実装内容

- `Main.cpp`で`Win32Window`からVulkan用の`HWND`とクライアントサイズを受け取り、`VulkanRenderer`を生成する。
- リサイズ時は`RecreateSwapchain()`を呼ぶ。
- `VulkanRenderer.cpp`はInstance、Win32 Surface、GPU選択、Logical Device、Swapchain、画像View、Command Buffer、同期、Presentを扱う。
- 各フレームで取得したSwapchain画像を`COLOR_ATTACHMENT_OPTIMAL`へ遷移し、Dynamic Renderingのclearで濃紺に塗り、`PRESENT_SRC_KHR`へ戻す。
- Debugビルドでは`TREXLAP2_DEBUG_TOOLS=1`によりValidation Layerを有効化し、メッセージをVisual Studioの出力ウィンドウへ送る。

## 動作確認

1. Visual Studioで`Debug|x64`を選択する。
2. F5で実行する。
3. 1920x1080のウィンドウが濃紺で表示されることを確認する。
4. リサイズ、最小化、復元、終了でクラッシュしないことを確認する。
5. Visual Studioの出力に`Validation Error`が出ないことを確認する。

## 前提

- Vulkan SDK 1.3以降。
- GPUドライバがVulkan 1.3のDynamic RenderingとSynchronization 2に対応していること。
- x64のみをサポート対象とする。

## 次の描画段階

単色クリアの後は、GLSLをSPIR-Vへコンパイルして三角形を描く。その段階で初めてシェーダ、`VkPipelineLayout`、`VkPipeline`を追加する。
