#include "pch.hpp"
#include "Win32Window.hpp"

#include <stdexcept>
#include <system_error>

namespace {
	constexpr wchar_t kWindowClassName[] = L"TRExLap2WindowClass";
}

/// <summary>
/// Win32ウィンドウクラスを登録し、指定クライアントサイズのウィンドウを生成する。
/// </summary>
Win32Window::Win32Window(HINSTANCE instance, const wchar_t* windowTitle, std::uint32_t clientWidth, std::uint32_t clientHeight, int showCommand)
	: instance_(instance), clientWidth_(clientWidth), clientHeight_(clientHeight)
{
	RegisterWindowClass();
	CreateWindowInstance(windowTitle, clientWidth, clientHeight, showCommand);
}

/// <summary>
/// 生成済みのウィンドウと、このインスタンス専用のウィンドウクラスを破棄する。
/// </summary>
Win32Window::~Win32Window()
{
	if (windowHandle_ != nullptr)
	{
		DestroyWindow(windowHandle_);
	}

	if (windowClassAtom_ != 0)
	{
		UnregisterClassW(MAKEINTATOM(windowClassAtom_), instance_);
	}
}

/// <summary>
/// 保留中のWin32メッセージを処理し、終了要求の有無を返す。
/// </summary>
bool Win32Window::ProcessMessages()
{
	MSG msg{};
	while (PeekMessageW(&msg, nullptr, 0, 0, PM_REMOVE))
	{
		if (msg.message == WM_QUIT)
		{
			shouldClose_ = true;
			break;
		}
		TranslateMessage(&msg);
		DispatchMessageW(&msg);
	}
	return !shouldClose_;
}

/// <summary>
/// リサイズ通知を一度だけ取得し、取得後に通知状態をクリアする。
/// </summary>
bool Win32Window::ConsumeResize()
{
	const bool wasResized = resized_;
	resized_ = false;
	return wasResized;
}

/// <summary>
/// ウィンドウが最小化され、描画を停止すべき状態か返す。
/// </summary>
bool Win32Window::IsMinimized() const noexcept
{
	return minimized_;
}

/// <summary>
/// Vulkan Surface作成に使用するWin32ウィンドウハンドルを返す。
/// </summary>
HWND Win32Window::GetHandle() const noexcept
{
	return windowHandle_;
}

/// <summary>
/// 現在のクライアント領域幅をピクセル単位で返す。
/// </summary>
std::uint32_t Win32Window::GetClientWidth() const noexcept
{
	return clientWidth_;
}

/// <summary>
/// 現在のクライアント領域高をピクセル単位で返す。
/// </summary>
std::uint32_t Win32Window::GetClientHeight() const noexcept
{
	return clientHeight_;
}

/// <summary>
/// Win32の静的コールバックから、対応するWin32Windowインスタンスへメッセージを中継する。
/// </summary>
LRESULT CALLBACK Win32Window::WindowProcedure(HWND windowHandle, UINT message, WPARAM wParam, LPARAM lParam)
{
	if (message == WM_NCCREATE)
	{
		const auto* createInfo = reinterpret_cast<const CREATESTRUCTW*>(lParam);
		auto* window = static_cast<Win32Window*>(createInfo->lpCreateParams);
		SetWindowLongPtrW(windowHandle, GWLP_USERDATA, reinterpret_cast<LONG_PTR>(window));
	}

	auto* window = reinterpret_cast<Win32Window*>(GetWindowLongPtrW(windowHandle, GWLP_USERDATA));

	if (window != nullptr) return window->HandleMessage(windowHandle, message, wParam, lParam);
	return DefWindowProcW(windowHandle, message, wParam, lParam);
}

/// <summary>
/// 終了・最小化・リサイズを内部状態へ反映し、その他を既定プロシージャへ渡す。
/// </summary>
LRESULT Win32Window::HandleMessage(HWND windowHandle, UINT message, WPARAM wParam, LPARAM lParam)
{
	switch (message)
	{
	case WM_CLOSE:
		DestroyWindow(windowHandle);
		return 0;
	case WM_DESTROY:
		windowHandle_ = nullptr;
		shouldClose_ = true;
		PostQuitMessage(0);
		return 0;
	case WM_SIZE:
		minimized_ = (wParam == SIZE_MINIMIZED);

		if (!minimized_)
		{
			clientWidth_ = LOWORD(lParam);
			clientHeight_ = HIWORD(lParam);
			resized_ = true;
		}

		return 0;

	default:
		return DefWindowProcW(windowHandle, message, wParam, lParam);
	}
}

/// <summary>
/// このアプリケーションのWin32ウィンドウクラスを登録する。
/// </summary>
void Win32Window::RegisterWindowClass()
{
	WNDCLASSEXW windowClass{};
	windowClass.cbSize = sizeof(WNDCLASSEXW);
	windowClass.style = CS_HREDRAW | CS_VREDRAW;
	windowClass.lpfnWndProc = WindowProcedure;
	windowClass.hInstance = instance_;
	windowClass.hCursor = LoadCursorW(nullptr, IDC_ARROW);
	windowClass.lpszClassName = kWindowClassName;

	windowClassAtom_ = RegisterClassExW(&windowClass);

	if (windowClassAtom_ == 0) throw std::system_error(static_cast<int>(GetLastError()), std::system_category(), "RegisterClassExW failed");
}

/// <summary>
/// 指定したクライアント領域を満たす通常ウィンドウを生成して表示する。
/// </summary>
void Win32Window::CreateWindowInstance(const wchar_t* windowTitle, std::uint32_t clientWidth, std::uint32_t clientHeight, int showCommand)
{
	RECT windowRect{ 0, 0, static_cast<LONG>(clientWidth), static_cast<LONG>(clientHeight) };

	if (AdjustWindowRectEx(&windowRect, WS_OVERLAPPEDWINDOW, FALSE, 0) == FALSE) throw std::system_error(static_cast<int>(GetLastError()), std::system_category(), "AdjustWindowRectEx failed");
	windowHandle_ = CreateWindowExW(
		0,
		kWindowClassName,
		windowTitle,
		WS_OVERLAPPEDWINDOW,
		CW_USEDEFAULT,
		CW_USEDEFAULT,
		windowRect.right - windowRect.left,
		windowRect.bottom - windowRect.top,
		nullptr,
		nullptr,
		instance_,
		this);

	if (windowHandle_ == nullptr) throw std::system_error(static_cast<int>(GetLastError()), std::system_category(), "CreateWindowExW failed");

	ShowWindow(windowHandle_, showCommand);
	UpdateWindow(windowHandle_);
}
