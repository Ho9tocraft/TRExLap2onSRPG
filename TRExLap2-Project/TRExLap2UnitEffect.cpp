#include "pch.hpp"
#include "TRExLap2UnitEffect.hpp"

#include <algorithm>
#include <cstdint>
#include <functional>
#include <utility>
#include "TRExLap2Enums.hpp"
#include <string>
#include <vector>

void TRExLap2UnitEffect::setEffectFlag(TRExLap2EffectType type)
{
	effectType.set(static_cast<size_t>(type));
}

bool TRExLap2UnitEffect::checkEffectCondition(TRExLap2IngameUnit& unit)
{
	return this->effectCondFunction(unit);
}

bool TRExLap2UnitEffect::effectExecute(TRExLap2IngameUnit& unit)
{
	return this->effectExecFunction(unit);
}

TRExLap2UnitEffect::TRExLap2UnitEffect(
	std::function<bool(TRExLap2IngameUnit&)> execFunction, std::function<bool(TRExLap2IngameUnit&)> condFunction,
	std::function<void(TRExLap2UnitEffect&)> postInitFunction, std::u8string defaultDisplayNameJa, std::u8string defaultDisplayNameEn,
	std::vector<std::u8string> defaultDescJa, std::vector<std::u8string> defaultDescEn)
	: effectCondFunction(condFunction), effectExecFunction(execFunction), effectDefaultDisplayNameJa(defaultDisplayNameJa),
	effectDefaultDisplayNameEn(defaultDisplayNameEn), effectDescJa(defaultDescJa), effectDescEn(defaultDescEn)
{
	postInitFunction(*this);
}

bool TRExLap2UnitSkill::executeSkill(TRExLap2IngameUnit& unit)
{
	return this->skillExecFunction(unit, *this);
}

bool TRExLap2UnitSkill::checkSkillCondition(TRExLap2IngameUnit& unit)
{
	return this->skillCondFunction(unit, *this);
}

void TRExLap2UnitSkill::setSkillLearnTiming(std::int64_t skillLevel, std::int64_t unitLevel)
{
	this->skillLevelValues.push_back(std::make_pair(skillLevel, unitLevel));
	std::sort(this->skillLevelValues.begin(), this->skillLevelValues.end());
}

std::int64_t TRExLap2UnitSkill::getSkillLevel(std::int64_t unitLevel)
{
	size_t i = 0;
	while (i < this->skillLevelValues.size())
	{
		if (i == this->skillLevelValues.size()) break; // 仮に、最後の段階まで来た場合はそのまま返す (習得の限界点)
		if (this->skillLevelValues[i].second > unitLevel) {
			i--; // 恐らく+1してしまっているので、1つ前の段階を返す
			break;
		}
		i++;
	}
	return this->skillLevelValues[i].first;
}

TRExLap2UnitSkill::TRExLap2UnitSkill(
	std::function<bool(TRExLap2IngameUnit&, TRExLap2UnitSkill&)> execFunction,
	std::function<bool(TRExLap2IngameUnit&, TRExLap2UnitSkill&)> condFunction,
	bool isSkillHasLevel, std::u8string defaultDisplayNameJa, std::u8string defaultDisplayNameEn,
	std::vector<std::u8string> defaultDescJa, std::vector<std::u8string> defaultDescEn)
{
	this->skillCondFunction = condFunction;
	this->skillExecFunction = execFunction;
	this->isSkillHasLevel = isSkillHasLevel;
	this->skillDefaultDisplayNameJa = defaultDisplayNameJa;
	this->skillDefaultDisplayNameEn = defaultDisplayNameEn;
	this->skillDescJa = defaultDescJa;
	this->skillDescEn = defaultDescEn;
}
