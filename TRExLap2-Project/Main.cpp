#include "pch.hpp"
#include "AudioPlayer.hpp"
#include "ImageLoader.hpp"
#include "Win32Window.hpp"
#include "VulkanRenderer.hpp"
#include "Main.hpp"

#include <algorithm>
#include <chrono>
#include <cstdint>
#include <exception>
#include <filesystem>
#include <sal.h>
#include <sstream>
#include <stdexcept>
#include <string>
#include <vector>
#include <Windows.h>
#include "TRExLap2Unit.hpp"
#include "TRExLap2UnitEffect.hpp"

namespace {
	constexpr wchar_t kWindowTitle[ ] = L"TRExLap2 on SRPG";

	/// <summary>
	/// 実行ファイルの配置先を取得し、作業ディレクトリに依存しないアセット探索に使う。
	/// </summary>
	std::filesystem::path GetExecutableDirectory()
	{
		std::vector<wchar_t> pathBuffer(MAX_PATH);
		while ( true )
		{
			const DWORD copiedLength = GetModuleFileNameW(nullptr, pathBuffer.data(), static_cast< DWORD >( pathBuffer.size() ));
			if ( copiedLength == 0 ) throw std::runtime_error("GetModuleFileNameW failed.");
			if ( copiedLength < pathBuffer.size() - 1 ) return std::filesystem::path(pathBuffer.data()).parent_path();
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
		if ( !bgm.UsesTaggedLoop() ) throw std::runtime_error("FF16_Logos.ogg does not expose a valid LOOPSTART/LOOPLENGTH tag.");
		OutputDebugStringA("[Audio] Logos is playing with its OGG loop range.\n");
		const auto& taggedLoopRange = bgm.GetTaggedLoopRangeMilliseconds();
		const auto introVerificationTime = std::chrono::steady_clock::now() + std::chrono::seconds(2);
		const auto taggedLoopVerificationTime = std::chrono::steady_clock::now() + std::chrono::milliseconds(taggedLoopRange->second + 5000);
		bool introVerified = false;
		bool taggedLoopVerified = false;

		while ( window.ProcessMessages() )
		{
			/// <summary>
			/// WM_SIZEを受けたフレームでだけ、Swapchain依存資源を作り直す。
			/// </summary>
			if ( window.ConsumeResize() && !window.IsMinimized() )
			{
				renderer.RecreateSwapchain(
					window.GetClientWidth(),
					window.GetClientHeight());
			}

			if ( !window.IsMinimized() )
			{
				/// <summary>
				/// 画像付き矩形を描画する。後でSRPGマップとUI描画をここへ追加する。
				/// </summary>
				renderer.DrawFrame();

				/// <summary>
				/// 再生開始直後の位置がLOOPSTARTより前であることを確認し、イントロの再生を保証する。
				/// </summary>
				if ( !introVerified && std::chrono::steady_clock::now() >= introVerificationTime )
				{
					const std::uint64_t playbackPosition = bgm.GetPlaybackPositionMilliseconds();
					if ( playbackPosition == 0 || playbackPosition >= taggedLoopRange->first )
					{
						throw std::runtime_error("The OGG BGM did not begin from the file start before LOOPSTART.");
					}
					introVerified = true;
					OutputDebugStringA("[Audio] Logos intro playback from the file start verified.\n");
				}

				/// <summary>
				/// OGGの最初のループ終端通過後、再生位置がタグ区間へ戻ったことを一度だけ確認する。
				/// </summary>
				if ( !taggedLoopVerified && std::chrono::steady_clock::now() >= taggedLoopVerificationTime )
				{
					const std::uint64_t playbackPosition = bgm.GetPlaybackPositionMilliseconds();
					if ( !bgm.IsTaggedLoopRepeatPlaying() || playbackPosition < taggedLoopRange->first || playbackPosition >= taggedLoopRange->second )
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
	catch ( const std::exception& exception )
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
	this->unitEffectDictionary.insert_or_assign(u8"the_cindercurse", [ ] () {
		return TRExLap2UnitEffect(
			[ ] (TRExLap2IngameUnit& unit) -> bool {
				unit.approveDealDamageMultiplier(u8"the_cindercurse", 1.2);
				unit.approveReceiveDamageMultiplier(u8"the_cindercurse", 1.4);
				return true;
			},
			[ ] (TRExLap2IngameUnit& unit) -> bool { return unit.checkMoraleCond(130, false); },
			[ ] (TRExLap2UnitEffect& effect) { /* 実行される内容はないようです */ },
			u8"始原の火の残光", u8"The Cinder's Curse",
			{ u8"気力130以上で与ダメージ1.2倍、被ダメージ1.4倍" }, {
				u8"With 130 or more Morale, damage dealt is multiplied by 1.2,",
				u8" and damage received is multiplied by 1.4" });
		}
	);
	// ダミー能力
	this->unitEffectDictionary.insert_or_assign(u8"dummy_effect", [ ] () {
		return TRExLap2UnitEffect(
			[ ] (TRExLap2IngameUnit& unit) -> bool { return true; },
			[ ] (TRExLap2IngameUnit& unit) -> bool { return false; },
			[ ] (TRExLap2UnitEffect& effect) { /* 実行される内容はないようです */ },
			u8"ダミー能力", u8"Dummy Effect",
			{ u8"これはダミー能力です。" },
			{ u8"This is a dummy effect." });
		}
	);
}

void TRExLap2GameMain::initUnitSkillDictionary()
{
	// アタッカー
	this->unitSkillDictionary.insert_or_assign(u8"attacker", [ ] () {
		return TRExLap2UnitSkill(
			[ ] (TRExLap2IngameUnit& unit, TRExLap2UnitSkill& skill) {
				// skillは使用しない。
				unit.approveDealDamageMultiplier(u8"attacker", 1.2);
				return true;
			},
			[ ] (TRExLap2IngameUnit& unit, TRExLap2UnitSkill& skill) { return unit.checkMoraleCond(130, false); },
			false, u8"アタッカー", u8"Attacker",
			{ u8"気力130以上のとき、与ダメージ1.2倍" },
			{ u8"Attack power increases by +50 according to level." });
		}
	);
	// インファイト
	this->unitSkillDictionary.insert_or_assign(u8"infight", [ ] () {
		return TRExLap2UnitSkill(
			[ ] (TRExLap2IngameUnit& unit, TRExLap2UnitSkill& skill) {
				// 格闘武器攻撃力補正・移動力補正を反映する。
				const std::int64_t skillLevel = skill.getSkillLevel(unit.getLevel());
				unit.approveWeaponMeleeATKFixed(u8"infight", 50 * skillLevel);
				unit.approveMovementRangeFixed(u8"infight", skillLevel >= 4LL ? ( ( skillLevel - 1LL ) / 3LL ) : 0LL);
				return true;
			},
			[ ] (TRExLap2IngameUnit& unit, TRExLap2UnitSkill& skill) {
				// unitとskillは使用しない。
				return true; // 常時発動なので常時true
			},
			true, u8"インファイト", u8"Infight",
			{ u8"スキルレベルに応じて、格闘武器の攻撃力が+50増加。",
				u8"スキルレベル4以上で、それを3で割った余りが1の場合に、それぞれで更に移動力+1(累積可)。" },
			{ u8"Attack power of melee weapons increases by +50 according to level.",
				u8"At Lv4 and Lv7, movement +1 (the increase per level accumulates)." });
		}
	);
	// ガンファイト
	this->unitSkillDictionary.insert_or_assign(u8"gunfight", [ ] () {
		return TRExLap2UnitSkill(
			[ ] (TRExLap2IngameUnit& unit, TRExLap2UnitSkill& skill) {
				// 射撃武器攻撃力補正・移動力補正を反映する。
				const std::int64_t skillLevel = skill.getSkillLevel(unit.getLevel());
				unit.approveWeaponRangedATKFixed(u8"gunfight", 50 * skillLevel);
				unit.approveWeaponRangedRangeFixed(u8"gunfight", skillLevel >= 4LL ? ( ( skillLevel - 1LL ) / 3LL ) : 0LL);
				return true;
			},
			[ ] (TRExLap2IngameUnit& unit, TRExLap2UnitSkill& skill) {
				// unitとskillは使用しない。
				return true; // 常時発動なので常時true
			},
			true, u8"ガンファイト", u8"Gunfight",
			{ u8"スキルレベルに応じて、射撃武器の攻撃力が+50増加。",
				u8"スキルレベル4以上で、それを3で割った余りが1の場合に、それぞれで更に射程+1(累積可)。" },
			{ u8"Attack power of ranged weapons increases by +50 according to level.",
				u8"At Lv4 and Lv7, range +1 (the increase per level accumulates)." });
		}
	);
	// メイジファイト
	this->unitSkillDictionary.insert_or_assign(u8"magefight", [ ] () {
		return TRExLap2UnitSkill(
			[ ] (TRExLap2IngameUnit& unit, TRExLap2UnitSkill& skill) {
				// 魔法武器攻撃力補正・移動力補正を反映する。
				const std::int64_t skillLevel = skill.getSkillLevel(unit.getLevel());
				unit.approveWeaponMagicATKFixed(u8"magefight", 50 * skillLevel);
				unit.approveWeaponMagicAccuracyFixed(u8"magefight", skillLevel >= 4LL ? ( ( skillLevel - 1LL ) / 3LL ) * 5LL : 0LL);
				return true;
			},
			[ ] (TRExLap2IngameUnit& unit, TRExLap2UnitSkill& skill) {
				// unitとskillは使用しない。
				return true; // 常時発動なので常時true
			},
			true, u8"メイジファイト", u8"Magefight",
			{ u8"スキルレベルに応じて、魔法武器の攻撃力が+50増加。",
				u8"スキルレベル4以上で、それを3で割った余りが1の場合に、それぞれで更に命中補正+5(累積可)。" },
			{ u8"Attack power of magic weapons increases by +50 according to level.",
				u8"At Lv4 and Lv7, accuracy modifier +5 (the increase per level accumulates)." });
		}
	);
	// 強運
	this->unitSkillDictionary.insert_or_assign(u8"high_luck", [ ] () {
		return TRExLap2UnitSkill(
			[ ] (TRExLap2IngameUnit& unit, TRExLap2UnitSkill& skill) {
				return true;
			},
			[ ] (TRExLap2IngameUnit& unit, TRExLap2UnitSkill& skill) { return true; },
			false, u8"強運", u8"High Luck",
			{ u8"獲得資金が1.2倍になる。" },
			{ u8"Funds acquired are multiplied by 1.2." });
		}
	);
	// 指揮官
	this->unitSkillDictionary.insert_or_assign(u8"commander", [ ] () {
		return TRExLap2UnitSkill(
			[ ] (TRExLap2IngameUnit& unit, TRExLap2UnitSkill& skill) {
				return true;
			},
			[ ] (TRExLap2IngameUnit& unit, TRExLap2UnitSkill& skill) { return true; },
			false, u8"指揮官", u8"Commander",
			{ u8"指揮範囲内にいる味方の命中・回避率が上昇する。" },
			{ u8"Accuracy and evasion of allied units within command range are increased." });
		}
	);
	// 天才
	this->unitSkillDictionary.insert_or_assign(u8"genius", [ ] () {
		return TRExLap2UnitSkill(
			[ ] (TRExLap2IngameUnit& unit, TRExLap2UnitSkill& skill) {
				return true;
			},
			[ ] (TRExLap2IngameUnit& unit, TRExLap2UnitSkill& skill) { return true; },
			false, u8"天才", u8"Genius",
			{ u8"命中・回避・クリティカル率に+20％" },
			{ u8"Accuracy, evasion, and critical rate are increased by +20%." });
		}
	);
	// 念動力
	this->unitSkillDictionary.insert_or_assign(u8"psychokinesis", [ ] () {
		return TRExLap2UnitSkill(
			[ ] (TRExLap2IngameUnit& unit, TRExLap2UnitSkill& skill) {
				const std::int64_t skillLevel = skill.getSkillLevel(unit.getLevel());
				return true;
			},
			[ ] (TRExLap2IngameUnit& unit, TRExLap2UnitSkill& skill) {
				return true;
			},
			true, u8"念動力", u8"Psychokinesis",
			{ u8"スキルレベルに応じて、命中率・回避率が上昇する。",
				u8"また、念動系の武器や特殊能力「念動フィールド」の使用条件としても求められる。" },
			{ u8"Accuracy and evasion increase according to skill level.",
				u8R"(In addition, it is also required as a condition for using psychokinetic weapons and the special ability "Psychokinetic Field".)" });
		}
	);
	// 予知
	this->unitSkillDictionary.insert_or_assign(u8"precognition", [ ] () {
		return TRExLap2UnitSkill(
			[ ] (TRExLap2IngameUnit& unit, TRExLap2UnitSkill& skill) {
				return true;
			},
			[ ] (TRExLap2IngameUnit& unit, TRExLap2UnitSkill& skill) {
				return unit.checkMoraleCond(130, false);
			},
			true, u8"予知", u8"Precognition",
			{ u8"気力130以上のとき、敵フェイズ中の最終回避率に+20％" },
			{ u8"When morale is 130 or higher, the final evasion rate during the enemy phase is increased by +20%." });
		}
	);
	// 戦場の支配者 (ドミナント)
	this->unitSkillDictionary.insert_or_assign(u8"dominance_sight", [ ] () {
		return TRExLap2UnitSkill(
			[ ] (TRExLap2IngameUnit& unit, TRExLap2UnitSkill& skill) {
				return true;
			},
			[ ] (TRExLap2IngameUnit& unit, TRExLap2UnitSkill& skill) {
				return unit.checkMoraleCond(140, false);
			},
			false, u8"戦場の支配者", u8"Dominance Sight",
			{ u8"気力140以上のとき、敵フェイズ中の最終命中率・回避率に+20％" },
			{ u8"When morale is 140 or higher, the final hit rate and evasion rate during the enemy phase are increased by +20%." });
		}
	);
	// ラッキー
	this->unitSkillDictionary.insert_or_assign(u8"lucky", [ ] () {
		return TRExLap2UnitSkill(
			[ ] (TRExLap2IngameUnit& unit, TRExLap2UnitSkill& skill) {
				const std::uint64_t skillLevel = std::clamp(
					static_cast< std::uint64_t >( std::max<std::int64_t>(skill.getSkillLevel(unit.getLevel()), 0LL) ),
					0ULL, 4ULL);
				if ( unit.getRandomRoller(std::uniform_int_distribution<std::uint64_t>(1, 6400)) <= skillLevel * 100 ) {
					unit.approveWeaponAccuracyFixed(u8"lucky", 100);
					unit.approveWeaponCriticalFixed(u8"lucky", 100);
					unit.approveEvasionRateFixed(u8"lucky", 100);
					unit.armEffectsAndSkillsExpiringTimer(u8"lucky");
				}
				return true;
			},
			[ ] (TRExLap2IngameUnit& unit, TRExLap2UnitSkill& skill) {
				return true;
			},
			true, u8"ラッキー", u8"Lucky",
			{ u8"たまに命中・回避・クリティカル率が100％になる。" },
			{ u8"Sometimes, accuracy, evasion, and critical rate become 100%." });
		}
	);
	// SP回復
	this->unitSkillDictionary.insert_or_assign(u8"sp_regen", [ ] () {
		return TRExLap2UnitSkill(
			[ ] (TRExLap2IngameUnit& unit, TRExLap2UnitSkill& skill) {
				unit.recvSP(10);
				return true;
			},
			[ ] (TRExLap2IngameUnit& unit, TRExLap2UnitSkill& skill) {
				return true;
			},
			true, u8"SP回復", u8"SP Regeneration",
			{ u8"毎ターンSPが10回復する。" },
			{ u8"Every turn, SP recovers by 10." });
		}
	);

	// ダミースキル
	this->unitSkillDictionary.insert_or_assign(u8"dummy_skill", [ ] () {
		return TRExLap2UnitSkill(
			[ ] (TRExLap2IngameUnit& unit, TRExLap2UnitSkill& skill) {
				return true;
			},
			[ ] (TRExLap2IngameUnit& unit, TRExLap2UnitSkill& skill) {
				// ダミーなので、unitとskillは使用しない。また、常時false。
				return false;
			},
			false, u8"ダミースキル", u8"Dummy Skill",
			{ u8"これはダミースキルです。" },
			{ u8"This is a dummy skill." });
		}
	);
}

TRExLap2UnitEffect TRExLap2GameMain::getUnitEffectById(std::u8string effectId)
{
	const bool hasEffect = this->unitEffectDictionary.contains(effectId);
	return hasEffect ? this->unitEffectDictionary.at(effectId)( )
		: this->unitEffectDictionary.at(u8"dummy_effect")( );
}

TRExLap2UnitSkill TRExLap2GameMain::getUnitSkillById(std::u8string skillId)
{
	const bool hasSkill = this->unitSkillDictionary.contains(skillId);
	return hasSkill ? this->unitSkillDictionary.at(skillId)( )
		: this->unitSkillDictionary.at(u8"dummy_skill")( );
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
