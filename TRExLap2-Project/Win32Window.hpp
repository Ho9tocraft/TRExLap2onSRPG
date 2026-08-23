#pragma once

#include <cstdint>
#include <Windows.h>

class Win32Window final
{
public:
	Win32Window(
		HINSTANCE instance,
		const wchar_t* windowTitle,
		std::uint32_t clientWidth,
		std::uint32_t clientHeight,
		int showCommand);
	~Win32Window();

	Win32Window(const Win32Window&) = delete;
	Win32Window(Win32Window&&) = delete;

	bool ProcessMessages();
	bool ConsumeResize();
	bool IsMinimized() const noexcept;

	HWND GetHandle() const noexcept;
	std::uint32_t GetClientWidth() const noexcept;
	std::uint32_t GetClientHeight() const noexcept;
private:
	static LRESULT CALLBACK WindowProcedure(HWND hwnd, UINT uMsg, WPARAM wParam, LPARAM lParam);
	LRESULT HandleMessage(HWND windowHandle, UINT message, WPARAM wParam, LPARAM lParam);

	void RegisterWindowClass();
	void CreateWindowInstance(const wchar_t* windowTitle, std::uint32_t clientWidth, std::uint32_t clientHeight, int showCommand);

	HINSTANCE instance_ = nullptr;
	HWND windowHandle_ = nullptr;
	ATOM windowClassAtom_ = 0;

	bool shouldClose_ = false;
	bool resized_ = false;
	bool minimized_ = false;

	std::uint32_t clientWidth_ = 0;
	std::uint32_t clientHeight_ = 0;
};
