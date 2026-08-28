#include "pch.hpp"
#include "AudioPlayer.hpp"
#include "ImageLoader.hpp"
#include "Win32Window.hpp"
#include "VulkanRenderer.hpp"
#include "Main.hpp"

#include <Windows.h>
#include <chrono>
#include <cstdint>
#include <exception>
#include <filesystem>
#include <sal.h>
#include <sstream>
#include <stdexcept>
#include <vector>
#include "TRExLap2Unit.hpp"
#include "TRExLap2UnitEffect.hpp"
#include <string>

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
		const std::filesystem::path bgmPath = GetExecutableDirectory() / L"assets/audio/FF16_Logos.ogg";
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
		AudioPlayer bgm(bgmPath);
		bgm.PlayLooping();
		if (!bgm.UsesTaggedLoop()) throw std::runtime_error("FF16_Logos.ogg does not expose a valid LOOPSTART/LOOPLENGTH tag.");
		OutputDebugStringA("[Audio] Logos is playing with its OGG loop range.\n");
		const auto& taggedLoopRange = bgm.GetTaggedLoopRangeMilliseconds();
		const auto introVerificationTime = std::chrono::steady_clock::now() + std::chrono::seconds(2);
		const auto taggedLoopVerificationTime = std::chrono::steady_clock::now() + std::chrono::milliseconds(taggedLoopRange->second + 5000);
		bool introVerified = false;
		bool taggedLoopVerified = false;

		while (window.ProcessMessages())
		{
			/** 初回イントロ終了後に、タグ付きOGGの反復範囲へ切り替える。 */
			bgm.UpdateLooping();

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

				/** 再生開始直後の位置がLOOPSTARTより前であることを確認し、イントロの再生を保証する。 */
				if (!introVerified && std::chrono::steady_clock::now() >= introVerificationTime)
				{
					const std::uint64_t playbackPosition = bgm.GetPlaybackPositionMilliseconds();
					if (playbackPosition == 0 || playbackPosition >= taggedLoopRange->first)
					{
						throw std::runtime_error("The OGG BGM did not begin from the file start before LOOPSTART.");
					}
					introVerified = true;
					OutputDebugStringA("[Audio] Logos intro playback from the file start verified.\n");
				}

				/** OGGの最初のループ終端通過後、再生位置がタグ区間へ戻ったことを一度だけ確認する。 */
				if (!taggedLoopVerified && std::chrono::steady_clock::now() >= taggedLoopVerificationTime)
				{
					const std::uint64_t playbackPosition = bgm.GetPlaybackPositionMilliseconds();
					if (!bgm.IsTaggedLoopRepeatPlaying() || playbackPosition < taggedLoopRange->first || playbackPosition >= taggedLoopRange->second)
					{
						throw std::runtime_error("The OGG BGM did not return to its LOOPSTART/LOOPLENGTH range after the first loop.");
					}
					taggedLoopVerified = true;
					OutputDebugStringA("[Audio] Logos OGG loop range verified after its first loop.\n");
				}
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

void TRExLap2GameMain::initUnitDictionary()
{
}

void TRExLap2GameMain::initUnitEffectDictionary()
{
	// 始原の火の残光
	this->unitEffectDictionary.insert_or_assign(u8"the_cindercurse", []() {
		return TRExLap2UnitEffect(
			[](TRExLap2IngameUnit& unit) -> bool { return true; },
			[](TRExLap2IngameUnit& unit) -> bool { return false; },
			[](TRExLap2UnitEffect& effect) { /* 実行される内容はないようです */ },
			u8"始原の火の残光", u8"The Cinder's Curse",
			{u8"気力130以上で与ダメージ1.2倍、被ダメージ1.4倍"}, {
				u8"With 130 or more Morale, damage dealt is multiplied by 1.2,",
				u8" and damage received is multiplied by 1.4"});
	});
	// ダミー能力
	this->unitEffectDictionary.insert_or_assign(u8"dummy_effect", []() {
		return TRExLap2UnitEffect(
			[](TRExLap2IngameUnit& unit) -> bool { return true; },
			[](TRExLap2IngameUnit& unit) -> bool { return false; },
			[](TRExLap2UnitEffect& effect) { /* 実行される内容はないようです */ },
			u8"ダミー能力", u8"Dummy Effect",
			{ u8"これはダミー能力です。" }, {
				u8"This is a dummy effect." });
	});
}

void TRExLap2GameMain::initUnitSkillDictionary()
{
	this->unitSkillDictionary.insert_or_assign(u8"infight", []() {
		// TODO:return TRExLap2UnitSkill();
	});
}

TRExLap2UnitEffect TRExLap2GameMain::getUnitEffectById(std::u8string effectId)
{
	const bool hasEffect = this->unitEffectDictionary.contains(effectId);
	return hasEffect ? this->unitEffectDictionary.at(effectId)()
		: this->unitEffectDictionary.at(u8"dummy_effect")();
}

TRExLap2GameMain::TRExLap2GameMain()
{
	this->initUnitEffectDictionary();
	this->initUnitSkillDictionary();
	this->initUnitDictionary();
}

TRExLap2GameMain::~TRExLap2GameMain()
{
	this->unitDictionary.clear();
}
