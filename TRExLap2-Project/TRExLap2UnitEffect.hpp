#pragma once
#include <bitset>
#include <cstdint>
#include <functional>
#include <string>
#include <utility>
#include <vector>

#include "TRExLap2Enums.hpp"

/* ---- プロトタイプ宣言 ---- */
class TRExLap2IngameUnit;

/* ---- クラス宣言本体 ---- */
/// <summary>
/// TRExLap2のユニットの特殊効果を表すクラス。
/// </summary>
class TRExLap2UnitEffect
{
protected:
	/// <summary>
	/// 特殊効果の実行関数。
	/// </summary>
	std::function<bool(TRExLap2IngameUnit&)> effectExecFunction;
	/// <summary>
	/// 特殊効果の発動条件関数。
	/// </summary>
	std::function<bool(TRExLap2IngameUnit&)> effectCondFunction;
	/// <summary>
	/// 特殊効果のID。<para/>
	/// インスタンス生成直後には設定されない。<para/>
	/// 命名規則は「(特殊効果のid)_(それを持つユニットのid)」
	/// </summary>
	std::u8string effectId;
	/// <summary>
	/// 特殊効果のタイプ。複合系に備え、bitsetとした。
	/// </summary>
	std::bitset<static_cast<size_t>(TRExLap2EffectType::EXTRA_EFFECT)> effectType;
	/// <summary>
	/// 特殊効果のデフォルト表示名（日本語）
	/// </summary>
	std::u8string effectDefaultDisplayNameJa;
	/// <summary>
	/// 特殊効果のデフォルト表示名（英語）
	/// </summary>
	std::u8string effectDefaultDisplayNameEn;
	/// <summary>
	/// 特殊効果の表示名（日本語）。未設定時は空文字列なのでフォールバックされる。
	/// </summary>
	std::u8string effectDisplayNameJa;
	/// <summary>
	/// 特殊効果の表示名（英語）。未設定時は空文字列なのでフォールバックされる。
	/// </summary>
	std::u8string effectDisplayNameEn;
	/// <summary>
	/// 特殊効果の説明（日本語）
	/// </summary>
	std::vector<std::u8string> effectDescJa;
	/// <summary>
	/// 特殊効果の説明（英語）
	/// </summary>
	std::vector<std::u8string> effectDescEn;
	/// <summary>
	/// エフェクトのフラグをビットセットに投下する。
	/// </summary>
	/// <param name="type">対象のフラグ</param>
	void setEffectFlag(TRExLap2EffectType type);
public:
	bool checkEffectCondition(TRExLap2IngameUnit& unit);
	bool effectExecute(TRExLap2IngameUnit& unit);
	/// <summary>
	/// コンストラクタ
	/// </summary>
	/// <param name="execFunction">実行するときの関数の中身</param>
	/// <param name="condFunction">条件を確認するときの関数の中身</param>
	/// <param name="postInitFunction">初期化後に実行される関数の中身。説明文のオーバーライトなど。</param>
	/// <param name="defaultDisplayNameJa">デフォルトの表示名（日本語）</param>
	/// <param name="defaultDisplayNameEn">デフォルトの表示名（英語）</param>
	/// <param name="defaultDescJa">デフォルトの説明（日本語）</param>
	/// <param name="defaultDescEn">デフォルトの説明（英語）</param>
	TRExLap2UnitEffect(
		std::function<bool(TRExLap2IngameUnit&)> execFunction,
		std::function<bool(TRExLap2IngameUnit&)> condFunction,
		std::function<void(TRExLap2UnitEffect&)> postInitFunction,
		std::u8string defaultDisplayNameJa, std::u8string defaultDisplayNameEn,
		std::vector<std::u8string> defaultDescJa, std::vector<std::u8string> defaultDescEn);
};

class TRExLap2UnitSkill
{
protected:
	/// <summary>
	/// 特殊スキルの実行関数。
	/// </summary>
	std::function<bool(TRExLap2IngameUnit&)> skillExecFunction;
	/// <summary>
	/// 特殊スキルの発動条件関数。
	/// </summary>
	std::function<bool(TRExLap2IngameUnit&)> skillCondFunction;
	/// <summary>
	/// 特殊スキルのID。<para/>
	/// インスタンス生成直後には設定されない。<para/>
	/// 命名規則は「(特殊スキルのid)_(それを持つユニットのid)」
	///	</summary>
	std::u8string skillId;
	/// <summary>
	/// 特殊スキルの説明（日本語）
	/// </summary>
	std::vector<std::u8string> skillDescJa;
	/// <summary>
	/// 特殊スキルの説明（英語）
	/// </summary>
	std::vector<std::u8string> skillDescEn;
	/// <summary>
	/// 特殊スキルがレベルを持つかどうか
	/// </summary>
	bool isSkillHasLevel;
	/// <summary>
	/// 特殊スキルが習得されたレベルの配列(pairの1個目がスキルレベル、2個目がユニットレベル)。<para/>
	/// デフォルトでは空配列(辞書登録時の内容のため)。
	/// </summary>
	std::vector<std::pair<std::int64_t, std::int64_t>> skillLevelValues;
public:
	void setSkillLearnTiming(std::int64_t skillLevel, std::int64_t unitLevel);
	TRExLap2UnitSkill(
		std::function<bool(TRExLap2IngameUnit&)> execFunction,
		std::function<bool(TRExLap2IngameUnit&)> condFunction,
		bool isSkillHasLevel);
};
