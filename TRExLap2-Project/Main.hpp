#pragma once
#include <exception>
#include <functional>
#include <map>
#include <string>

#include "TRExLap2Unit.hpp"
#include "TRExLap2UnitEffect.hpp"

struct TRExLap2GameMain {
protected:
	/// <summary>
	/// ユニット辞書。実体
	/// </summary>
	std::map<std::u8string, TRExLap2Unit> unitDictionary;
	/// <summary>
	/// ユニット効果辞書。出力用関数の羅列
	/// </summary>
	std::map<std::u8string, std::function<TRExLap2UnitEffect()>> unitEffectDictionary;
	std::map<std::u8string, std::function<TRExLap2UnitSkill()>> unitSkillDictionary;
	void initUnitDictionary();
	void initUnitEffectDictionary();
	void initUnitSkillDictionary();
public:
	TRExLap2UnitEffect getUnitEffectById(std::u8string effectId);
	TRExLap2UnitSkill getUnitSkillById(std::u8string skillId);
	TRExLap2GameMain();
	~TRExLap2GameMain();
};
