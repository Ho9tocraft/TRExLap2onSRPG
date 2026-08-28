#include "pch.hpp"
#include "TRExLap2Unit.hpp"
#include <cmath>
#include <cstdint>
#include <map>
#include <string>
#include <utility>

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
