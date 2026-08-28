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

void TRExLap2UnitSkill::setSkillLearnTiming(std::int64_t skillLevel, std::int64_t unitLevel)
{
	this->skillLevelValues.push_back(std::make_pair(skillLevel, unitLevel));
	std::sort(this->skillLevelValues.begin(), this->skillLevelValues.end());
}

TRExLap2UnitSkill::TRExLap2UnitSkill(std::function<bool(TRExLap2IngameUnit&)> execFunction, std::function<bool(TRExLap2IngameUnit&)> condFunction, bool isSkillHasLevel)
{
	this->skillCondFunction = condFunction;
	this->skillExecFunction = execFunction;
	this->isSkillHasLevel = isSkillHasLevel;
}
