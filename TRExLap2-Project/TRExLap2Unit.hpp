#pragma once
#include <array>
#include <cstdint>
#include <string>
#include <map>
#include <vector>
#include <utility>

#pragma warning(push)
#pragma warning(disable:6011 26800 26813)
#include <toml++/toml.hpp>
#pragma warning(pop)

#include "TRExLap2Enums.hpp"
#include "TRExLap2UnitEffect.hpp"
#include "TRExLap2SpCommand.hpp"

class TRExLap2Unit
{
protected:
	/* ---- ルート ---- */
	/// <summary>
	/// ユニットのID
	/// </summary>
	std::u8string UnitID;
	/// <summary>
	/// ユニットのタグ
	/// </summary>
	std::vector<std::u8string> UnitTags;
	/// <summary>
	/// ユニットの画像パス
	/// </summary>
	std::u8string UnitImagePath;
	/// <summary>
	/// ユニットのBGMパス
	/// </summary>
	std::u8string UnitBGMPath;
	/// <summary>
	/// ユニットのBGMの優先度
	/// </summary>
	std::int8_t UnitBGMPriority;
	/* ---- Main項 ---- */
	/// <summary>
	/// ユニットの名前（日本語）
	/// </summary>
	std::u8string UnitNameJa;
	/// <summary>
	/// ユニットの名前（英語）
	/// </summary>
	std::u8string UnitNameEn;
	/// <summary>
	/// ユニットの説明（日本語）
	/// </summary>
	std::vector<std::u8string> UnitDescJa;
	/// <summary>
	/// ユニットの説明（英語）
	/// </summary>
	std::vector<std::u8string> UnitDescEn;
	/* ---- baseStats項 ---- */
	/// <summary>
	/// ユニットのHP
	/// </summary>
	std::int64_t unitHP;
	/// <summary>
	/// ユニットのMP
	/// </summary>
	std::int64_t unitMP;
	/// <summary>
	/// ユニットの装甲値
	/// </summary>
	std::int64_t unitDEF;
	/// <summary>
	/// ユニットの運動性
	/// </summary>
	std::int64_t unitMOB;
	/// <summary>
	/// ユニットの照準値
	/// </summary>
	std::int64_t unitSIG;
	/// <summary>
	/// ユニットの移動距離
	/// </summary>
	std::int64_t unitMVR;
	/// <summary>
	/// ユニットの移動タイプ
	/// </summary>
	std::vector<TRExLap2StageMovableType> unitMVT;
	/// <summary>
	/// ユニットの地形適応値
	/// </summary>
	std::array<TRExLap2TerrainAdapt, 4> unitADP;
	/// <summary>
	/// ユニットのパイロット性格タイプ
	/// </summary>
	TRExLap2PilotPersonalityType unitPSL;
	/// <summary>
	/// ユニットのパイロットの性別
	/// </summary>
	TRExLap2PilotGenderType unitGND;
	/// <summary>
	/// ユニットのパイロットの格闘値
	/// </summary>
	std::int64_t pilotMEL;
	/// <summary>
	/// ユニットのパイロットの射撃値
	/// </summary>
	std::int64_t pilotRNG;
	/// <summary>
	/// ユニットのパイロットの魔力値
	/// </summary>
	std::int64_t pilotMAG;
	/// <summary>
	/// ユニットのパイロットの技量値
	/// </summary>
	std::int64_t pilotDEX;
	/// <summary>
	/// ユニットのパイロットの防御値
	/// </summary>
	std::int64_t pilotDEF;
	/// <summary>
	/// ユニットのパイロットの回避値
	/// </summary>
	std::int64_t pilotAVD;
	/// <summary>
	/// ユニットのパイロットの抵抗値
	/// </summary>
	std::int64_t pilotRST;
	/// <summary>
	/// ユニットのパイロットの命中値
	/// </summary>
	std::int64_t pilotACC;
	/// <summary>
	/// ユニットのパイロットの精神ポイント(SP)
	/// </summary>
	std::int64_t pilotSPP;
	/// <summary>
	/// このユニットと同期している別データユニットのID
	/// </summary>
	std::vector<std::u8string> syncedUnit;
	/* ---- loadout項 ---- */
	//TRExLap2Job unitJob;
	/// <summary>
	/// ユニットの召喚獣スロット数
	/// </summary>
	std::uint64_t eikonicSlotCount;
	/// <summary>
	/// ユニットの武器コスト上限
	/// </summary>
	std::uint64_t weaponCostLimit;
	/// <summary>
	/// ユニットの所持可能アイテム上限
	/// </summary>
	std::uint64_t carryableItemLimit;
	/// <summary>
	/// ユニットの強化タイプ
	/// </summary>
	TRExLap2ModifyType unitModifyType;
	/* ---- effects項 ---- */
	/// <summary>
	/// ユニットの効果一覧
	/// </summary>
	std::vector<TRExLap2UnitEffect> unitEffects;
	/* ---- skills項 ---- */
	std::vector<TRExLap2UnitSkill> unitSkills;
	/* ---- spiritual_command項 ---- */
	std::vector<TRExLap2SpCommand> unitSpiritualCommands;
public:
	TRExLap2Unit(toml::table& tblUnit);
	~TRExLap2Unit();
};

class TRExLap2IngameUnit : public TRExLap2Unit
{
protected:
	/* ---- 現在の固定値 ---- */
	/// <summary>
	/// ゲームデータ上でのユニットのレベル<para/>
	/// 変動するが、頻繫じゃねーのでゲームデータ上でのユニットのレベルは敢えて固定値として扱う。
	/// </summary>
	std::int64_t level;
	/// <summary>
	/// ゲームデータ上でのユニットの最大HP
	/// </summary>
	std::int64_t currentMaxHP;
	/// <summary>
	/// ゲームデータ上でのユニットの最大MP
	/// </summary>
	std::int64_t currentMaxMP;
	/// <summary>
	/// ゲームデータ上でのユニットの現在の装甲値
	/// </summary>
	std::int64_t currentDEF;
	/// <summary>
	/// ゲームデータ上でのユニットの現在の運動性
	/// </summary>
	std::int64_t currentMOB;
	/// <summary>
	/// ゲームデータ上でのユニットの現在の照準値
	/// </summary>
	std::int64_t currentSIG;
	/// <summary>
	/// ゲームデータ上でのユニットの現在の移動距離
	/// </summary>
	std::int64_t currentMVR;
	/// <summary>
	/// ゲームデータ上でのユニットの現在の格闘値
	/// </summary>
	std::int64_t currentPilotMEL;
	/// <summary>
	/// ゲームデータ上でのユニットの現在の射撃値
	/// </summary>
	std::int64_t currentPilotRNG;
	/// <summary>
	/// ゲームデータ上でのユニットの現在の魔力値
	/// </summary>
	std::int64_t currentPilotMAG;
	/// <summary>
	/// ゲームデータ上でのユニットの現在の技量値
	/// </summary>
	std::int64_t currentPilotDEX;
	/// <summary>
	/// ゲームデータ上でのユニットの現在の防御値
	/// </summary>
	std::int64_t currentPilotDEF;
	/// <summary>
	/// ゲームデータ上でのユニットの現在の回避値
	/// </summary>
	std::int64_t currentPilotAVD;
	/// <summary>
	/// ゲームデータ上でのユニットの現在の抵抗値
	/// </summary>
	std::int64_t currentPilotRST;
	/// <summary>
	/// ゲームデータ上でのユニットの現在の命中値
	/// </summary>
	std::int64_t currentPilotACC;
	/// <summary>
	/// ゲームデータ上でのユニットの現在の最大精神ポイント(SP)
	/// </summary>
	std::int64_t currentMaxSP;
	/* ---- 現在の変動値:リソース ---- */
	/// <summary>
	/// ゲームデータ上でのユニットの現在HP
	/// </summary>
	std::int64_t currentHP;
	/// <summary>
	/// ゲームデータ上でのユニットの現在MP
	/// </summary>
	std::int64_t currentMP;
	/// <summary>
	/// ゲームデータ上でのユニットの現在の精神ポイント(SP)
	/// </summary>
	std::int64_t currentSP;
	/* ---- 現在の変動値:パラメータ ---- */
	/// <summary>
	/// バフなどによって変動した、ゲームデータ上でのユニットの現在の装甲値
	/// </summary>
	std::int64_t currentBuffedDEF;
	/// <summary>
	/// バフなどによって変動した、ゲームデータ上でのユニットの現在の運動性
	/// </summary>
	std::int64_t currentBuffedMOB;
	/// <summary>
	/// バフなどによって変動した、ゲームデータ上でのユニットの現在の照準値
	/// </summary>
	std::int64_t currentBuffedSIG;
	/// <summary>
	/// バフなどによって変動した、ゲームデータ上でのユニットの現在の移動距離
	/// </summary>
	std::int64_t currentBuffedMVR;
	/// <summary>
	/// バフなどによって変動した、ゲームデータ上でのユニットの現在の格闘値
	/// </summary>
	std::int64_t currentBuffedPilotMEL;
	/// <summary>
	/// バフなどによって変動した、ゲームデータ上でのユニットの現在の射撃値
	/// </summary>
	std::int64_t currentBuffedPilotRNG;
	/// <summary>
	/// バフなどによって変動した、ゲームデータ上でのユニットの現在の魔力値
	/// </summary>
	std::int64_t currentBuffedPilotMAG;
	/// <summary>
	/// バフなどによって変動した、ゲームデータ上でのユニットの現在の技量値
	/// </summary>
	std::int64_t currentBuffedPilotDEX;
	/// <summary>
	/// バフなどによって変動した、ゲームデータ上でのユニットの現在の防御値
	/// </summary>
	std::int64_t currentBuffedPilotDEF;
	/// <summary>
	/// バフなどによって変動した、ゲームデータ上でのユニットの現在の回避値
	/// </summary>
	std::int64_t currentBuffedPilotAVD;
	/// <summary>
	/// バフなどによって変動した、ゲームデータ上でのユニットの現在の抵抗値
	/// </summary>
	std::int64_t currentBuffedPilotRST;
	/// <summary>
	/// バフなどによって変動した、ゲームデータ上でのユニットの現在の命中値
	/// </summary>
	std::int64_t currentBuffedPilotACC;

	/* 
	* △---- 現在の変動値:パラメータ ----△
	* -------------------------------------
	* ▽---- 倍率保存部 ----▽
	*/

	/// <summary>
	/// ユニットが与えるダメージの倍率
	/// </summary>
	std::map<std::u8string, double> dealDamageMultiplier;
	/// <summary>
	/// ユニットが受けるダメージの倍率
	/// </summary>
	std::map<std::u8string, double> receiveDamageMultiplier;
	/// <summary>
	/// ユニットの武器攻撃力の倍率
	/// </summary>
	std::map<std::u8string, double> weaponATKMultiplier;
	/// <summary>
	/// ユニットの格闘武器攻撃力の倍率
	/// </summary>
	std::map<std::u8string, double> weaponMeleeATKMultiplier;
	/// <summary>
	/// ユニットの射撃武器攻撃力の倍率
	/// </summary>
	std::map<std::u8string, double> weaponRangedATKMultiplier;
	/// <summary>
	/// ユニットの魔法武器攻撃力の倍率
	/// </summary>
	std::map<std::u8string, double> weaponMagicATKMultiplier;

	/*
	* △---- 倍率保存部 ----△
	* ----------------------------
	* ▽---- 固定増減保存部 ----▽
	*/

	/// <summary>
	/// ユニットの移動距離の固定補正値
	/// </summary>
	std::map<std::u8string, std::int64_t> movementRangeFixed;
	/// <summary>
	/// ユニットが与えるダメージの固定補正値
	/// </summary>
	std::map<std::u8string, std::int64_t> dealDamageFixed;
	/// <summary>
	/// ユニットが受けるダメージの固定補正値
	/// </summary>
	std::map<std::u8string, std::int64_t> receiveDamageFixed;
	/// <summary>
	/// ユニットの武器攻撃力の固定補正値
	/// </summary>
	std::map<std::u8string, std::int64_t> weaponATKFixed;
	/// <summary>
	/// ユニットの格闘武器攻撃力の固定補正値
	/// </summary>
	std::map<std::u8string, std::int64_t> weaponMeleeATKFixed;
	/// <summary>
	/// ユニットの射撃武器攻撃力の固定補正値
	/// </summary>
	std::map<std::u8string, std::int64_t> weaponRangedATKFixed;
	/// <summary>
	/// ユニットの魔法武器攻撃力の固定補正値
	/// </summary>
	std::map<std::u8string, std::int64_t> weaponMagicATKFixed;
	/* ---- 関数 ---- */
	/// <summary>
	/// HPを設定する。0未満は0、最大値を超える場合は最大値に設定される。
	/// </summary>
	/// <param name="toHP">設定するHPの値</param>
	void setHP(std::int64_t toHP);
	/// <summary>
	/// MPを設定する。0未満は0、最大値を超える場合は最大値に設定される。
	/// </summary>
	/// <param name="toMP">設定するMPの値</param>
	void setMP(std::int64_t toMP);
	/// <summary>
	/// SPを設定する。0未満は0、最大値を超える場合は最大値に設定される。
	/// </summary>
	/// <param name="toSP">設定するSPの値</param>
	void setSP(std::int64_t toSP);
public:
	/// <summary>
	/// 現在のレベルを返却する。
	/// </summary>
	/// <returns>現在のレベル</returns>
	std::int64_t getLevel() const;
	/// <summary>
	/// 現在のステータスを返却する。
	/// </summary>
	/// <returns>
	/// 現在のステータスのマップ。<para/>
	/// u8string: ステータス名<para/>
	/// std::pair&lt;std::int64_t, std::int64_t&gt;: 1個目が現在値、2個目が最大値(ないし基礎値)
	/// </returns>
	std::map<std::u8string, std::pair<std::int64_t, std::int64_t>> getCurrentStatus();
	/// <summary>
	/// HPのパーセンテージを返却する。0.0～100.0の範囲。
	/// </summary>
	/// <returns>HP％</returns>
	double getPercentHP() const;
	/// <summary>
	/// MPのパーセンテージを返却する。0.0～100.0の範囲。
	/// </summary>
	/// <returns>MP％</returns>
	double getPercentMP() const;
	void decrHP(std::int64_t amount);
	void decrMP(std::int64_t amount);
	void decrSP(std::int64_t amount);
	void recvHP(std::int64_t amount);
	void recvMP(std::int64_t amount);
	void recvSP(std::int64_t amount);
	/// <summary>
	/// Luaスクリプト側から呼び出す際の、HP/MP/SPの変動をまとめて行う関数。<para/>
	/// amountHP、amountMP、amountSPのいずれかが0以上の場合、その値に応じてHP、MP、SPを設定する。<para/>
	/// Luaでは「ModifyLifeStatus(amountHP, amountMP, amountSP)」となるだろうか？
	/// </summary>
	/// <param name="amountHP">設定するHPの値</param>
	/// <param name="amountMP">設定するMPの値</param>
	/// <param name="amountSP">設定するSPの値</param>
	void eventModifyLifeStat(std::int64_t amountHP, std::int64_t amountMP, std::int64_t amountSP);
	/// <summary>
	/// Luaスクリプト側から呼び出す際の、HPの変動を行う関数。
	/// </summary>
	/// <param name="amountHP">設定するHPの値</param>
	void eventModifyHP(std::int64_t amountHP);
	/// <summary>
	/// Luaスクリプト側から呼び出す際の、MPの変動を行う関数。
	/// </summary>
	/// <param name="amountMP">設定するMPの値</param>
	void eventModifyMP(std::int64_t amountMP);
	/// <summary>
	/// Luaスクリプト側から呼び出す際の、SPの変動を行う関数。
	/// </summary>
	/// <param name="amountSP">設定するSPの値</param>
	void eventModifySP(std::int64_t amountSP);
	/// <summary>
	/// ユニットが与えるダメージの倍率を記入する。
	/// </summary>
	/// <param name="gainId">倍率を適用する要因のID</param>
	/// <param name="multiplier">倍率の値</param>
	void approveDealDamageMultiplier(std::u8string gainId, double multiplier);
	/// <summary>
	/// ユニットが受けるダメージの倍率を記入する。
	/// </summary>
	/// <param name="gainId">倍率を適用する要因のID</param>
	/// <param name="multiplier">倍率の値</param>
	void approveReceiveDamageMultiplier(std::u8string gainId, double multiplier);
	/// <summary>
	/// ユニットの武器攻撃力の倍率を記入する。
	/// </summary>
	/// <param name="gainId">倍率を適用する要因のID</param>
	/// <param name="multiplier">倍率の値</param>
	void approveWeaponATKMultiplier(std::u8string gainId, double multiplier);
	/// <summary>
	/// ユニットの格闘武器攻撃力の倍率を記入する。
	/// </summary>
	/// <param name="gainId">倍率を適用する要因のID</param>
	/// <param name="multiplier">倍率の値</param>
	void approveWeaponMeleeATKMultiplier(std::u8string gainId, double multiplier);
	/// <summary>
	/// ユニットの射撃武器攻撃力の倍率を記入する。
	/// </summary>
	/// <param name="gainId">倍率を適用する要因のID</param>
	/// <param name="multiplier">倍率の値</param>
	void approveWeaponRangedATKMultiplier(std::u8string gainId, double multiplier);
	/// <summary>
	/// ユニットの魔法武器攻撃力の倍率を記入する。
	/// </summary>
	/// <param name="gainId">倍率を適用する要因のID</param>
	/// <param name="multiplier">倍率の値</param>
	void approveWeaponMagicATKMultiplier(std::u8string gainId, double multiplier);
	/// <summary>
	/// ユニットの移動範囲の固定値を記入する。
	/// </summary>
	/// <param name="gainId">固定値を適用する要因のID</param>
	/// <param name="fixedValue">固定値の値</param>
	void approveMovementRangeFixed(std::u8string gainId, std::int64_t fixedValue);
	/// <summary>
	/// ユニットが与えるダメージの固定値を記入する。
	/// </summary>
	/// <param name="gainId">固定値を適用する要因のID</param>
	/// <param name="fixedValue">固定値の値</param>
	void approveDealDamageFixed(std::u8string gainId, std::int64_t fixedValue);
	/// <summary>
	/// ユニットが受けるダメージの固定値を記入する。
	/// </summary>
	/// <param name="gainId">固定値を適用する要因のID</param>
	/// <param name="fixedValue">固定値の値</param>
	void approveReceiveDamageFixed(std::u8string gainId, std::int64_t fixedValue);
	/// <summary>
	/// ユニットの武器攻撃力の固定値を記入する。
	/// </summary>
	/// <param name="gainId">固定値を適用する要因のID</param>
	/// <param name="fixedValue">固定値の値</param>
	void approveWeaponATKFixed(std::u8string gainId, std::int64_t fixedValue);
	/// <summary>
	/// ユニットの格闘武器攻撃力の固定値を記入する。
	/// </summary>
	/// <param name="gainId">固定値を適用する要因のID</param>
	/// <param name="fixedValue">固定値の値</param>
	void approveWeaponMeleeATKFixed(std::u8string gainId, std::int64_t fixedValue);
	/// <summary>
	/// ユニットの射撃武器攻撃力の固定値を記入する。
	/// </summary>
	/// <param name="gainId">固定値を適用する要因のID</param>
	/// <param name="fixedValue">固定値の値</param>
	void approveWeaponRangedATKFixed(std::u8string gainId, std::int64_t fixedValue);
	/// <summary>
	/// ユニットの魔法武器攻撃力の固定値を記入する。
	/// </summary>
	/// <param name="gainId">固定値を適用する要因のID</param>
	/// <param name="fixedValue">固定値の値</param>
	void approveWeaponMagicATKFixed(std::u8string gainId, std::int64_t fixedValue);
};

/// <summary>
/// TODO: ジョブ一覧? 変数にはできない
/// - ナイト (Paladin)                      [FF14]     [2.x]
/// - 戦士 (Warrior)                        [FF14]     [2.x]
/// - 暗黒騎士 (Dark Knight)                [FF14]     [3.x]
/// - ガンブレイカー (Gunbreaker)           [FF14]     [5.x]
/// - バスティオン (Bastion)                [FF14]     [8.x]
/// - 白魔道士 (White Mage)                 [FF14]     [2.x]
/// - 学者 (Scholar)                        [FF14]     [2.x]
/// - 占星術師 (Astrologian)                [FF14]     [3.x]
/// - 賢者 (Sage)                           [FF14]     [6.x]
/// - 竜騎士 (Dragoon)                      [FF14]     [2.x]
/// - リーパー (Reaper)                     [FF14]     [6.x]
/// - オブスリヴァクォリヤ (Obsleviquarrya) [TRExLap2] [6.x]
/// - モンク (Monk)                         [FF14]     [2.x]
/// - 巫覡 (Exorcist)                       [TRExLap2] [2.x]
/// - 侍 (Samurai)                          [FF14]     [4.x]
/// - 忍者 (Ninja)                          [FF14]     [2.x]
/// - 魔拳士 (Lostmagia)                    [TRExLap2] [4.x]
/// - ヴァイパー (Viper)                    [FF14]     [7.x]
/// - 吟遊詩人 (Bard)                       [FF14]     [2.x]
/// - 機工士 (Machinist)                    [FF14]     [3.x]
/// - 踊り子 (Dancer)                       [FF14]     [5.x]
/// - ???                                   [FF14]     [8.x]
/// - ダイアロスアイドル (Diaros Idol)      [TRExLap2] [5.x]
/// - 黒魔道士 (Black Mage)                 [FF14]     [2.x]
/// - 召喚士 (Summoner)                     [FF14]     [2.x]
/// - 赤魔道士 (Red Mage)                   [FF14]     [4.x]
/// - ピクトマンサー (Pictomancer)          [FF14]     [7.x]
/// </summary>
