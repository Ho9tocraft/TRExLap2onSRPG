#include "pch.hpp"
#include "Win32Window.hpp"
#include "VulkanRenderer.hpp"
#include <exception>

namespace {
	constexpr wchar_t kWindowTitle[] = L"TRExLap2 on SRPG";
}

int WINAPI wWinMain(
	_In_ HINSTANCE hInstance,
	_In_opt_ HINSTANCE hPrevInstance,
	_In_ LPWSTR lpCmdLine,
	_In_ int showCommand)
{
	try
	{
		Win32Window window(hInstance, kWindowTitle, 1920, 1080, showCommand);
		VulkanRenderer renderer(
			window.GetHandle(),
			window.GetClientWidth(),
			window.GetClientHeight());

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
				/** 現段階では単色クリアのみ。後でSRPGマップとUI描画をここへ追加する。 */
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
