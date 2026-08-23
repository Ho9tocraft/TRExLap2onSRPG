# STATUS

更新日時: 2026-08-23

## 現在のスライス

第2段階: 完了。DLAAを優先し、利用できない環境ではTAAへ自動フォールバックする。

## 実装済み

- Win32ウィンドウ、Vulkan 1.3 Dynamic Rendering、Swapchain、同期、濃紺クリア。
- SPIR-Vシェーダのカスタムビルド。
- WICによるPNGのRGBA8復号 (`ImageLoader`)。
- `exellia_renewal.png` (144 x 144、32bpp RGBA) の読み込みと画像寸法のデバッグ出力。
- 追補1: `ImageRgba8::width` / `height` を保持し、Swapchainの物理ピクセル寸法へ換算して中央に1:1表示するPush Constant。
- 追補1: 実行ファイル基準のアセット探索と、ビルド出力先への `assets` 自動コピー。単体exe実行時も作業ディレクトリに依存しない。
- Staging BufferによるRGBA8のDevice Local Imageへの転送。
- Image View、Sampler、Combined Image Sampler Descriptor Setの生成と破棄。
- UV付き矩形シェーダ、透過アルファブレンド、画像描画。
- Swapchain画像単位のPresent完了Semaphoreによる安全な再利用。
- Debug x64ビルド成功（0 warnings / 0 errors）。
- Debug x64実行で画像表示とプロセス生存を確認。
- ValidationログのVUIDは0件。
- 追補1検証: 144 x 144のエクセリア画像を1:1表示し、臨時の320 x 180 RGBA PNGも320 x 180で表示することをComputer Useで確認。テスト後に元PNGをSHA-256一致で復帰済み。
- `Triangle.*` を `TRExLap2Shader.*` へ改名し、プロジェクト設定・SPIR-V出力名・読み込み先をすべて更新。
- NVIDIA NGXの必要なInstance/Device拡張を問い合わせて有効化し、Buffer Device Addressを含むDLAA必須機能をDevice作成時に有効化。
- フル解像度入力・出力のDLAA Featureを初期化し、Scene Color、Depth、Motion Vector、Swapchain出力をNGXへ渡す経路を実装。
- DLAAが利用不可または評価失敗の場合に、線形Scene Colorと二重TAA履歴を用いたTAAフォールバックで描画を継続。
- DLAAの内部クリアに必要なTransfer Destination usageをSwapchainおよび中間カラー画像へ付与。
- Debug x64でDLAA Feature有効化、画像表示、Validationログのアプリ側VUID 0件を再確認（0 warnings / 0 errorsでビルド）。

## 実行環境由来の既知警告

- Overwolf/OBSの暗黙Vulkan LayerがAPI 1.2であるため、Vulkan 1.3アプリ起動時にLoader警告が出る。
- Overwolf DLLが `vkGetInstanceProcAddr` を公開していないためLoaderがそのLayerをスキップする。アプリ側のVUIDや実行停止は発生しない。

## 未着手

- `Endwalker - Footfalls` の音声再生。

## 次のスライス

- 第3段階として音声再生を実装する。
