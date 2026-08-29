#include "pch.hpp"
#include <cmath>
#include <cstdint>
#include <map>
#include <string>
#include <utility>
#include <vector>

#include "TomlLoader.hpp"
#include "TRExLap2Unit.hpp"


/// <summary>
/// TOMLユニット定義テーブルから、固定ユニットデータを構築する。
/// </summary>
/// <param name="tblUnit">読み込み対象のルートテーブル。</param>
TRExLap2Unit::TRExLap2Unit(toml::table& tblUnit)
{
    const std::string schemaVersion = RequireString(tblUnit, "schema_version", "schema_version");
    if (schemaVersion != "1.0") ParseErrorThrower("schema_version", "Only schema version 1.0 is supported.");

    const std::string dataType = RequireString(tblUnit, "type", "type");
    if (dataType != "units") ParseErrorThrower("type", "Expected 'units'.");

    const std::string unitId = RequireString(tblUnit, "id", "id");
    if (unitId.empty()) ParseErrorThrower("id", "Must not be empty.");
    this->UnitID = ToU8Str(unitId);
    this->UnitTags = ReadOptionalStringArray(tblUnit, "tags", "tags");

    const std::string imagePath = RequireString(tblUnit, "image", "image");
    if (imagePath.empty()) ParseErrorThrower("image", "Must not be empty.");
    this->UnitImagePath = ToU8Str(imagePath);

    const std::string bgmPath = RequireString(tblUnit, "bgm", "bgm");
    if (bgmPath.empty()) ParseErrorThrower("bgm", "Must not be empty.");
    this->UnitBGMPath = ToU8Str(bgmPath);

    const std::int64_t rawBgmPriority = RequireInt64(tblUnit, "bgm_priority", "bgm_priority");
    this->UnitBGMPriority = static_cast<std::int8_t>(
        rawBgmPriority < -5LL ? -5LL : rawBgmPriority > 15LL ? 14LL : rawBgmPriority);

    const toml::table& mainTable = RequireTable(tblUnit, "main", "main");
    const std::string nameJa = RequireString(mainTable, "name_ja", "main.name_ja");
    const std::string nameEn = RequireString(mainTable, "name_en", "main.name_en");
    if (nameJa.empty()) ParseErrorThrower("main.name_ja", "Must not be empty.");
    if (nameEn.empty()) ParseErrorThrower("main.name_en", "Must not be empty.");
    this->UnitNameJa = ToU8Str(nameJa);
    this->UnitNameEn = ToU8Str(nameEn);
    this->UnitDescJa = RequireStringArray(mainTable, "desc_ja", "main.desc_ja");
    this->UnitDescEn = RequireStringArray(mainTable, "desc_en", "main.desc_en");

    const toml::table& baseStats = RequireTable(tblUnit, "baseStats", "baseStats");
    const toml::table& unitStats = RequireTable(baseStats, "Unit", "baseStats.Unit");
    const toml::table& pilotStats = RequireTable(baseStats, "Pilot", "baseStats.Pilot");
    this->unitHP = RequirePositive(RequireInt64(unitStats, "HP", "baseStats.Unit.HP"), "baseStats.Unit.HP");
    this->unitMP = RequirePositive(RequireInt64(unitStats, "MP", "baseStats.Unit.MP"), "baseStats.Unit.MP");
    this->unitDEF = RequirePositive(RequireInt64(unitStats, "DEF", "baseStats.Unit.DEF"), "baseStats.Unit.DEF");
    this->unitMOB = RequirePositive(RequireInt64(unitStats, "MOB", "baseStats.Unit.MOB"), "baseStats.Unit.MOB");
    this->unitSIG = RequirePositive(RequireInt64(unitStats, "SIG", "baseStats.Unit.SIG"), "baseStats.Unit.SIG");
    this->unitMVR = RequirePositive(RequireInt64(unitStats, "MVR", "baseStats.Unit.MVR"), "baseStats.Unit.MVR");
    this->unitMVT = RequireMovableTypes(unitStats, "MVT", "baseStats.Unit.MVT");
    this->unitADP = RequireTerrainAdapts(unitStats, "ADP", "baseStats.Unit.ADP");

    this->unitPSL = RequireEnum<TRExLap2PilotPersonalityType>(pilotStats, "PSL", "baseStats.Pilot.PSL");
    this->unitGND = RequireEnum<TRExLap2PilotGenderType>(pilotStats, "GND", "baseStats.Pilot.GND");
    this->pilotMEL = RequirePositive(RequireInt64(pilotStats, "MEL", "baseStats.Pilot.MEL"), "baseStats.Pilot.MEL");
    this->pilotRNG = RequirePositive(RequireInt64(pilotStats, "RNG", "baseStats.Pilot.RNG"), "baseStats.Pilot.RNG");
    this->pilotMAG = RequirePositive(RequireInt64(pilotStats, "MAG", "baseStats.Pilot.MAG"), "baseStats.Pilot.MAG");
    this->pilotDEX = RequirePositive(RequireInt64(pilotStats, "DEX", "baseStats.Pilot.DEX"), "baseStats.Pilot.DEX");
    this->pilotDEF = RequirePositive(RequireInt64(pilotStats, "DEF", "baseStats.Pilot.DEF"), "baseStats.Pilot.DEF");
    this->pilotAVD = RequirePositive(RequireInt64(pilotStats, "AVD", "baseStats.Pilot.AVD"), "baseStats.Pilot.AVD");
    this->pilotRST = RequirePositive(RequireInt64(pilotStats, "RST", "baseStats.Pilot.RST"), "baseStats.Pilot.RST");
    this->pilotACC = RequirePositive(RequireInt64(pilotStats, "ACC", "baseStats.Pilot.ACC"), "baseStats.Pilot.ACC");
    this->pilotSPP = RequireInt64(pilotStats, "SPP", "baseStats.Pilot.SPP");
    if (this->pilotSPP < 0LL) ParseErrorThrower("baseStats.Pilot.SPP", "Expected a non-negative integer.");
    this->syncedUnit = RequireStrOrStrArray(pilotStats, "SyncedUnit", "baseStats.Pilot.SyncedUnit");

    const toml::table& loadout = RequireTable(tblUnit, "loadout", "loadout");
    const std::string jobId = RequireString(loadout, "job_id", "loadout.job_id");
    if (jobId.empty()) ParseErrorThrower("loadout.job_id", "Must not be empty.");
    this->eikonicSlotCount = RequireNonNegative(RequireInt64(loadout, "eikonic_slot_count", "loadout.eikonic_slot_count"), "loadout.eikonic_slot_count");
    this->weaponCostLimit = RequireNonNegative(RequireInt64(loadout, "weapon_cost_limit", "loadout.weapon_cost_limit"), "loadout.weapon_cost_limit");
    this->carryableItemLimit = RequireNonNegative(RequireInt64(loadout, "carry_item_slots", "loadout.carry_item_slots"), "loadout.carry_item_slots");
    this->unitModifyType = RequireEnum<TRExLap2ModifyType>(loadout, "modify_type", "loadout.modify_type");
}

/// <summary>
/// ユニットが保持する効果・技能配列を破棄する。
/// </summary>
TRExLap2Unit::~TRExLap2Unit()
{
	this->unitEffects.clear();
	this->unitSkills.clear();
}

void TRExLap2IngameUnit::setHP(std::int64_t toHP)
{
	if (toHP < 0LL) toHP = 0LL;
	if (toHP > this->currentMaxHP) toHP = this->currentMaxHP;
	this->currentHP = toHP;
}

void TRExLap2IngameUnit::setMP(std::int64_t toMP)
{
	if (toMP < 0LL) toMP = 0LL;
	if (toMP > this->currentMaxMP) toMP = this->currentMaxMP;
	this->currentMP = toMP;
}

void TRExLap2IngameUnit::setSP(std::int64_t toSP)
{
	if (toSP < 0LL) toSP = 0LL;
	if (toSP > this->currentMaxSP) toSP = this->currentMaxSP;
	this->currentSP = toSP;
}

std::int64_t TRExLap2IngameUnit::getLevel() const
{
	return this->level;
}

std::map<std::u8string, std::pair<std::int64_t, std::int64_t>> TRExLap2IngameUnit::getCurrentStatus()
{
	return std::map<std::u8string, std::pair<std::int64_t, std::int64_t>>{
		// HP
		{ u8"HP", std::make_pair(this->currentHP, this->currentMaxHP) },
			// MP
		{ u8"MP", std::make_pair(this->currentMP, this->currentMaxMP) },
			// 精神ポイント
		{ u8"SP", std::make_pair(this->currentSP, this->currentMaxSP) },
			// 装甲値
		{ u8"DEF", std::make_pair(this->currentBuffedDEF, this->currentDEF) },
			// 運動性
		{ u8"MOB", std::make_pair(this->currentBuffedMOB, this->currentMOB) },
			// 照準値
		{ u8"SIG", std::make_pair(this->currentBuffedSIG, this->currentSIG) },
			// 移動距離
		{ u8"MVR", std::make_pair(this->currentBuffedMVR, this->currentMVR) },
			// 格闘値
		{ u8"MEL", std::make_pair(this->currentBuffedPilotMEL, this->currentPilotMEL) },
			// 射撃値
		{ u8"RNG", std::make_pair(this->currentBuffedPilotRNG, this->currentPilotRNG) },
			// 魔力値
		{ u8"MAG", std::make_pair(this->currentBuffedPilotMAG, this->currentPilotMAG) },
			// 技量値
		{ u8"DEX", std::make_pair(this->currentBuffedPilotDEX, this->currentPilotDEX) },
			// 防御値
		{ u8"DEF", std::make_pair(this->currentBuffedPilotDEF, this->currentPilotDEF) },
			// 回避値
		{ u8"AVD", std::make_pair(this->currentBuffedPilotAVD, this->currentPilotAVD) },
			// 抵抗値
		{ u8"RST", std::make_pair(this->currentBuffedPilotRST, this->currentPilotRST) },
			// 命中値
		{ u8"ACC", std::make_pair(this->currentBuffedPilotACC, this->currentPilotACC) },
	};
}

double TRExLap2IngameUnit::getPercentHP() const
{
	return std::round(static_cast<double>(this->currentHP) / static_cast<double>(this->currentMaxHP) * 1000.0) / 10.0;
}

double TRExLap2IngameUnit::getPercentMP() const
{
	return std::round(static_cast<double>(this->currentMP) / static_cast<double>(this->currentMaxMP) * 1000.0) / 10.0;
}

void TRExLap2IngameUnit::decrHP(std::int64_t amount)
{
	if (amount < 0LL) return;
	this->setHP(this->currentHP - amount);
}

void TRExLap2IngameUnit::decrMP(std::int64_t amount)
{
	if (amount < 0LL) return;
	this->setMP(this->currentMP - amount);
}

void TRExLap2IngameUnit::decrSP(std::int64_t amount)
{
	if (amount < 0LL) return;
	this->setSP(this->currentSP - amount);
}

void TRExLap2IngameUnit::recvHP(std::int64_t amount)
{
	if (amount < 0LL) return;
	this->setHP(this->currentHP + amount);
}

void TRExLap2IngameUnit::recvMP(std::int64_t amount)
{
	if (amount < 0LL) return;
	this->setMP(this->currentMP + amount);
}

void TRExLap2IngameUnit::recvSP(std::int64_t amount)
{
	if (amount < 0LL) return;
	this->setSP(this->currentSP + amount);
}

void TRExLap2IngameUnit::eventModifyLifeStat(std::int64_t amountHP, std::int64_t amountMP, std::int64_t amountSP)
{
	if (amountHP >= 0LL) this->setHP(amountHP);
	if (amountMP >= 0LL) this->setMP(amountMP);
	if (amountSP >= 0LL) this->setSP(amountSP);
}

void TRExLap2IngameUnit::eventModifyHP(std::int64_t amountHP)
{
	this->eventModifyLifeStat(amountHP, -1LL, -1LL);
}

void TRExLap2IngameUnit::eventModifyMP(std::int64_t amountMP)
{
	this->eventModifyLifeStat(-1LL, amountMP, -1LL);
}

void TRExLap2IngameUnit::eventModifySP(std::int64_t amountSP)
{
	this->eventModifyLifeStat(-1LL, -1LL, amountSP);
}

void TRExLap2IngameUnit::approveDealDamageMultiplier(std::u8string gainId, double multiplier)
{
	this->dealDamageMultiplier.insert_or_assign(gainId, multiplier);
}

void TRExLap2IngameUnit::approveReceiveDamageMultiplier(std::u8string gainId, double multiplier)
{
	this->receiveDamageMultiplier.insert_or_assign(gainId, multiplier);
}

void TRExLap2IngameUnit::approveWeaponATKMultiplier(std::u8string gainId, double multiplier)
{
	this->weaponATKMultiplier.insert_or_assign(gainId, multiplier);
}

void TRExLap2IngameUnit::approveWeaponMeleeATKMultiplier(std::u8string gainId, double multiplier)
{
	this->weaponMeleeATKMultiplier.insert_or_assign(gainId, multiplier);
}

void TRExLap2IngameUnit::approveWeaponRangedATKMultiplier(std::u8string gainId, double multiplier)
{
	this->weaponRangedATKMultiplier.insert_or_assign(gainId, multiplier);
}

void TRExLap2IngameUnit::approveWeaponMagicATKMultiplier(std::u8string gainId, double multiplier)
{
	this->weaponMagicATKMultiplier.insert_or_assign(gainId, multiplier);
}

void TRExLap2IngameUnit::approveMovementRangeFixed(std::u8string gainId, std::int64_t fixedValue)
{
	this->movementRangeFixed.insert_or_assign(gainId, fixedValue);
}

void TRExLap2IngameUnit::approveDealDamageFixed(std::u8string gainId, std::int64_t fixedValue)
{
	this->dealDamageFixed.insert_or_assign(gainId, fixedValue);
}

void TRExLap2IngameUnit::approveReceiveDamageFixed(std::u8string gainId, std::int64_t fixedValue)
{
	this->receiveDamageFixed.insert_or_assign(gainId, fixedValue);
}

void TRExLap2IngameUnit::approveWeaponATKFixed(std::u8string gainId, std::int64_t fixedValue)
{
	this->weaponATKFixed.insert_or_assign(gainId, fixedValue);
}

void TRExLap2IngameUnit::approveWeaponMeleeATKFixed(std::u8string gainId, std::int64_t fixedValue)
{
	this->weaponMeleeATKFixed.insert_or_assign(gainId, fixedValue);
}

void TRExLap2IngameUnit::approveWeaponRangedATKFixed(std::u8string gainId, std::int64_t fixedValue)
{
	this->weaponRangedATKFixed.insert_or_assign(gainId, fixedValue);
}

void TRExLap2IngameUnit::approveWeaponMagicATKFixed(std::u8string gainId, std::int64_t fixedValue)
{
	this->weaponMagicATKFixed.insert_or_assign(gainId, fixedValue);
}
