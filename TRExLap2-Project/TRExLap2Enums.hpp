#pragma once
#include <cstdint>
#include <limits>
#include <optional>
#include <string>
#include <string_view>
#include <type_traits>

/// <summary>
/// ユニットの移動タイプを表す列挙型。
/// </summary>
enum class TRExLap2StageMovableType : std::int64_t
{
	/// <summary>
	/// 地中
	/// </summary>
	Underground = -1LL,
	/// <summary>
	/// 地上
	/// </summary>
	Ground,
	/// <summary>
	/// 空中
	/// </summary>
	Air,
	/// <summary>
	/// 水中
	/// </summary>
	Underwater,
	/// <summary>
	/// 水上
	/// </summary>
	Overwater,
	/// <summary>
	/// 宇宙・異空間のみ
	/// </summary>
	Otherspace,
};

/// <summary>
/// ユニットの地形適応度を表す列挙型。
/// </summary>
enum class TRExLap2TerrainAdapt : std::uint64_t
{
	/// <summary>
	/// 使用不可
	/// </summary>
	F,
	/// <summary>
	/// 補正値20％
	/// </summary>
	E,
	/// <summary>
	/// 補正値40％
	/// </summary>
	D,
	/// <summary>
	/// 補正値60％
	/// </summary>
	C,
	/// <summary>
	/// 補正値80％
	/// </summary>
	B,
	/// <summary>
	/// 等倍
	/// </summary>
	A,
	/// <summary>
	/// 補正値120％
	/// </summary>
	S,
};

enum class TRExLap2PilotPersonalityType : std::int64_t
{
	/// <summary>
	/// 機械
	/// </summary>
	Machine = -1LL,
	/// <summary>
	/// 普通
	/// </summary>
	Normal,
	/// <summary>
	/// 強気
	/// </summary>
	Confident,
	/// <summary>
	/// 超強気
	/// </summary>
	ExtremeConfident,
	/// <summary>
	/// 冷静
	/// </summary>
	Calm,
	/// <summary>
	/// 慎重
	/// </summary>
	Cautious,
	/// <summary>
	/// 楽天家
	/// </summary>
	Optimistic,
	/// <summary>
	/// 努力家
	/// </summary>
	Hardworker,
	/// <summary>
	/// 短気
	/// </summary>
	Shorttempered,
	/// <summary>
	/// 大物
	/// </summary>
	Bigshot,
	/// <summary>
	/// 超大物
	/// </summary>
	Superbigshot,
	/// <summary>
	/// 狡猾
	/// </summary>
	Cunning,
	/// <summary>
	/// 残忍
	/// </summary>
	Cruel,
	/// <summary>
	/// 残火
	/// </summary>
	RekindledEmbers,
	/// <summary>
	/// 狂気
	/// </summary>
	Madness,
};

enum class TRExLap2PilotGenderType : std::int64_t
{
	/// <summary>
	/// 機械
	/// </summary>
	Machine = -2LL,
	/// <summary>
	/// 不明
	/// </summary>
	Unknown,
	/// <summary>
	/// 男性
	/// </summary>
	Male,
	/// <summary>
	/// 女性
	/// </summary>
	Female,
};

enum class TRExLap2ModifyType : std::uint64_t
{
	/// <summary>
	/// 汎用
	/// </summary>
	COMMON,
	/// <summary>
	/// タンク・近接DPS向け
	/// </summary>
	MELEE,
	/// <summary>
	/// 遠隔物理DPS向け
	/// </summary>
	RANGE,
	/// <summary>
	/// 遠隔魔法DPS・ヒーラー向け
	/// </summary>
	CASTER,
};

enum class TRExLap2EffectType : std::uint64_t
{
	/// <summary>
	/// ユニット変化形
	/// </summary>
	UNIT_CHANGE,
	/// <summary>
	/// バリア・特殊装甲系
	/// </summary>
	BARRIER_EXARMOR,
	/// <summary>
	/// 防御・回避系
	/// </summary>
	DEFENSE_EVADE,
	/// <summary>
	/// 特殊効果耐性系
	/// </summary>
	RESIST_DEBUFF,
	/// <summary>
	/// ダメージ耐性系
	/// </summary>
	RESIST_WEAPON,
	/// <summary>
	/// 任意使用の回復系
	/// </summary>
	ACTIVE_RECOVERY,
	/// <summary>
	/// 自動発動の回復系
	/// </summary>
	PASSIVE_RECOVERY,
	/// <summary>
	/// 移動に制限が伴う自動発動の回復系
	/// </summary>
	PASSIVE_RECOVERY_MOVEMENT_LIMIT,
	/// <summary>
	/// 能力アップ系
	/// </summary>
	STATUS_BUFF,
	/// <summary>
	/// 戦艦系(搭載など)
	/// </summary>
	BATTLESHIP_EFFECT,
	/// <summary>
	/// 特殊。Zクリスタルのような特殊アビリティの行動部分は、アビリティに記載する。
	/// </summary>
	EXTRA_EFFECT,
};

enum class TRExLap2SkillType : std::uint64_t {
	/// <summary>
	/// パラメータ上昇系
	/// </summary>
	PARAM_BUFF,
	/// <summary>
	/// 攻撃系
	/// </summary>
	ATK_SKILL,
	/// <summary>
	/// 防御・完全回避系
	/// </summary>
	DEF_EVADE_SKILL,
	/// <summary>
	/// 援護系
	/// </summary>
	SUPPORT_SKILL,
	/// <summary>
	/// 気力系
	/// </summary>
	MORALE_SKILL,
	/// <summary>
	/// 回復系
	/// </summary>
	REGEN_SKILL,
	/// <summary>
	/// SP系
	/// </summary>
	SP_SKILL,
	/// <summary>
	/// 成長系
	/// </summary>
	GROWTH_SKILL,
	/// <summary>
	/// 戦闘補助系
	/// </summary>
	BATTLE_SUPPORT_SKILL,
	/// <summary>
	/// 周辺補助系
	/// </summary>
	AOE_SUPPORTING_SKILL,
	/// <summary>
	/// その他特殊スキル
	/// </summary>
	EXTRA_SKILL,
};

enum class TRExLap2SpiritualsType : std::uint64_t {
	/// <summary>
	/// ダメージ上昇系 (魂, 闘志, 熱血)
	/// </summary>
	DAMAGE_BOOST,
	/// <summary>
	/// 防御系 (鉄壁, 不屈, 信念, 強靱)
	/// </summary>
	DEFENSE_BOOST,
	/// <summary>
	/// 命中・回避系 (感応, 先見, 集中, 必中, 閃き)
	/// </summary>
	ACCURACY_EVADE,
	/// <summary>
	/// 移動・行動系 (覚醒, 再動, 加速, 疾風, 連撃)
	/// </summary>
	MOVEMENT_ACTION,
	/// <summary>
	/// 回復系 (祈り, 絆, 期待, 根性, 信頼, ド根性, 友情)
	/// </summary>
	REGENERATION,
	/// <summary>
	/// 気力上昇系 (気合, 気迫, 激励, 大激励)
	/// </summary>
	INCREASE_MORALE,
	/// <summary>
	/// 攻撃支援系 (重撃, 狙撃, 直撃, 手加減, 同調, 突撃)
	/// </summary>
	ATTACK_SUPPORT,
	/// <summary>
	/// 敵攻撃妨害系 (威圧, 撹乱, 偵察, 分析)
	/// </summary>
	ATTACK_JAMMING,
	/// <summary>
	/// 成長系 (応援, 幸運, 修行, 祝福, 努力)
	/// </summary>
	GROWING_BOOST,
	/// <summary>
	/// 複合系 (愛, 強襲, 奇跡, 希望, 切り札, 直感, 勇気)
	/// </summary>
	MULTIPLE_TRIGGERING,
	/// <summary>
	/// 特殊系 (決意, 想念)
	/// </summary>
	EXTRA_SPIRITUALS,
};

/// <summary>
/// enum型ごとの値変換仕様を定義する。
///
/// 未対応のenum型は特殊化されていないため、変換を要求できない。
///
/// </summary>
/// <typeparam name="EnumT">対象となるenum class型。</typeparam>
template <typename EnumT>
struct TRExLap2EnumParser;

/// <summary>
/// 符号付き整数からenum値への変換を試行する。
///
/// </summary>
/// <typeparam name="EnumT">対象enum型。</typeparam>
/// <param name="value">変換対象の整数値。</param>
/// <returns>定義済みenum値ならその値、未定義値ならnullopt。</returns>
template <typename EnumT>
	requires std::is_enum_v<EnumT>
std::optional<EnumT> TryParseEnum(const std::int64_t value)
{
	return TRExLap2EnumParser<EnumT>::FromInt(value);
}

/// <summary>
/// 符号なし整数からenum値への変換を試行する。
///
/// </summary>
/// <typeparam name="EnumT">対象enum型。</typeparam>
/// <param name="value">変換対象の整数値。</param>
/// <returns>定義済みenum値ならその値、未定義値ならnullopt。</returns>
template <typename EnumT>
	requires std::is_enum_v<EnumT>
std::optional<EnumT> TryParseEnum(const std::uint64_t value)
{
	if (value > static_cast<std::uint64_t>(std::numeric_limits<std::int64_t>::max()))
		return std::nullopt;

	return TryParseEnum<EnumT>(static_cast<std::int64_t>(value));
}

/// <summary>
/// UTF-8文字列からenum値への変換を試行する。
///
/// </summary>
/// <typeparam name="EnumT">対象enum型。</typeparam>
/// <param name="value">変換対象のUTF-8文字列。</param>
/// <returns>定義済みenum値ならその値、未定義値ならnullopt。</returns>
template <typename EnumT>
	requires std::is_enum_v<EnumT>
std::optional<EnumT> TryParseEnum(const std::string_view value)
{
	return TRExLap2EnumParser<EnumT>::FromString(value);
}

/// <summary>
/// u8string形式のUTF-8文字列からenum値への変換を試行する。
///
/// </summary>
/// <typeparam name="EnumT">対象enum型。</typeparam>
/// <param name="value">変換対象のUTF-8文字列。</param>
/// <returns>定義済みenum値ならその値、未定義値ならnullopt。</returns>
template <typename EnumT>
	requires std::is_enum_v<EnumT>
std::optional<EnumT> TryParseEnum(const std::u8string_view value)
{
	std::string narrowValue;
	narrowValue.reserve(value.size());

	for (const char8_t byte : value)
		narrowValue.push_back(static_cast<char>(byte));

	return TryParseEnum<EnumT>(std::string_view{ narrowValue });
}

/// <summary>
/// 移動タイプの変換仕様。
/// </summary>
template <>
struct TRExLap2EnumParser<TRExLap2StageMovableType>
{
	static std::optional<TRExLap2StageMovableType> FromInt(std::int64_t value);
	static std::optional<TRExLap2StageMovableType> FromString(std::string_view value);
};

/// <summary>
/// 地形適応の変換仕様。
/// </summary>
template <>
struct TRExLap2EnumParser<TRExLap2TerrainAdapt>
{
	static std::optional<TRExLap2TerrainAdapt> FromInt(std::int64_t value);
	static std::optional<TRExLap2TerrainAdapt> FromString(std::string_view value);
};

/// <summary>
/// パイロット性格の変換仕様。
/// </summary>
template <>
struct TRExLap2EnumParser<TRExLap2PilotPersonalityType>
{
	static std::optional<TRExLap2PilotPersonalityType> FromInt(std::int64_t value);
	static std::optional<TRExLap2PilotPersonalityType> FromString(std::string_view value);
};

/// <summary>
/// パイロット性別の変換仕様。
/// </summary>
template <>
struct TRExLap2EnumParser<TRExLap2PilotGenderType>
{
	static std::optional<TRExLap2PilotGenderType> FromInt(std::int64_t value);
	static std::optional<TRExLap2PilotGenderType> FromString(std::string_view value);
};

/// <summary>
/// 改造タイプの変換仕様。
/// </summary>
template <>
struct TRExLap2EnumParser<TRExLap2ModifyType>
{
	static std::optional<TRExLap2ModifyType> FromInt(std::int64_t value);
	static std::optional<TRExLap2ModifyType> FromString(std::string_view value);
};
