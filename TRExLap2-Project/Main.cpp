#include "pch.hpp"
#include "ImageLoader.hpp"
#include "Win32Window.hpp"
#include "VulkanRenderer.hpp"
#include <exception>

namespace {
	constexpr wchar_t kWindowTitle[] = L"TRExLap2 on SRPG";

	/** 実行ファイルの配置先を取得し、作業ディレクトリに依存しないアセット探索に使う。 */
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
}

int WINAPI wWinMain(
	_In_ HINSTANCE hInstance,
	_In_opt_ HINSTANCE hPrevInstance,
	_In_ LPWSTR lpCmdLine,
	_In_ int showCommand)
{
	try
	{
		const std::filesystem::path portraitPath = GetExecutableDirectory() / L"assets/images/playable/exellia_renewal.png";
		const ImageRgba8 exelliaPortrait = ImageLoader::LoadRgba8(portraitPath);
		std::ostringstream imageInfo;
		imageInfo << "[ImageLoader] exellia_renewal.png: " << exelliaPortrait.width << 'x' << exelliaPortrait.height << " RGBA8\n";
		OutputDebugStringA(imageInfo.str().c_str());

		Win32Window window(hInstance, kWindowTitle, 1920, 1080, showCommand);
		VulkanRenderer renderer(
			window.GetHandle(),
			window.GetClientWidth(),
			window.GetClientHeight(),
			exelliaPortrait);

		while (window.ProcessMessages())
		{
			/** WM_SIZEを受けたフレームでだけ、Swapchain依存資源を作り直す。 */
			if (window.ConsumeResize() && !window.IsMinimized())
			{
				renderer.RecreateSwapchain(
					window.GetClientWidth(),
					window.GetClientHeight());
			}

			if (!window.IsMinimized())
			{
				/** 画像付き矩形を描画する。後でSRPGマップとUI描画をここへ追加する。 */
				renderer.DrawFrame();
			}
			else
			{
				WaitMessage();
			}
		}

		renderer.WaitUntilIdle();
	}
	catch (const std::exception& exception)
	{
		OutputDebugStringA(exception.what());
		MessageBoxA(nullptr, exception.what(), "TRExLap2 Vulkan error", MB_OK | MB_ICONERROR);
		return 1;
	}

	return 0;
}
