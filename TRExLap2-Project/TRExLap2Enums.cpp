#include "pch.hpp"

#include <optional>
#include <string>
#include <string_view>

#include "TRExLap2Enums.hpp"

namespace
{
	/// <summary>
	/// ASCII英字だけを大文字化し、enum用の識別子比較を安定させる。
	///
	/// 日本語UTF-8バイト列は一切変更しない。
	///
	/// </summary>
	/// <param name="value">正規化前の文字列。</param>
	/// <returns>ASCII英字のみ大文字化した文字列。</returns>
	std::string NormalizeEnumToken(const std::string_view value)
	{
		std::string result{ value };

		for (char& character : result)
		{
			if (character >= 'a' && character <= 'z')
				character = static_cast<char>(character - 'a' + 'A');
		}

		return result;
	}
}

/// <summary>
/// 移動タイプ整数を変換する。
///
/// </summary>
/// <param name="value">-1から4までの移動タイプ値。</param>
/// <returns>対応する移動タイプ。範囲外ならnullopt。</returns>
std::optional<TRExLap2StageMovableType>
TRExLap2EnumParser<TRExLap2StageMovableType>::FromInt(const std::int64_t value)
{
	switch (value)
	{
	case -1: return TRExLap2StageMovableType::Underground;
	case 0: return TRExLap2StageMovableType::Ground;
	case 1: return TRExLap2StageMovableType::Air;
	case 2: return TRExLap2StageMovableType::Underwater;
	case 3: return TRExLap2StageMovableType::Overwater;
	case 4: return TRExLap2StageMovableType::Otherspace;
	default: return std::nullopt;
	}
}

/// <summary>
/// 移動タイプ文字列を変換する。
///
/// </summary>
/// <param name="value">英語識別子または日本語表記。</param>
/// <returns>対応する移動タイプ。未定義値ならnullopt。</returns>
std::optional<TRExLap2StageMovableType>
TRExLap2EnumParser<TRExLap2StageMovableType>::FromString(const std::string_view value)
{
	const std::string token = NormalizeEnumToken(value);

	if (token == "UNDERGROUND" || value == "地中")
		return TRExLap2StageMovableType::Underground;
	if (token == "GROUND" || value == "陸" || value == "地上")
		return TRExLap2StageMovableType::Ground;
	if (token == "AIR" || value == "空" || value == "空中")
		return TRExLap2StageMovableType::Air;
	if (token == "UNDERWATER" || value == "水中")
		return TRExLap2StageMovableType::Underwater;
	if (token == "OVERWATER" || value == "水上")
		return TRExLap2StageMovableType::Overwater;
	if (token == "OTHERSPACE" || value == "宇" || value == "宇宙" || value == "異空間")
		return TRExLap2StageMovableType::Otherspace;

	return std::nullopt;
}

/// <summary>
/// 地形適応整数を変換する。
///
/// </summary>
/// <param name="value">0から6までの地形適応値。</param>
/// <returns>対応する地形適応。範囲外ならnullopt。</returns>
std::optional<TRExLap2TerrainAdapt>
TRExLap2EnumParser<TRExLap2TerrainAdapt>::FromInt(const std::int64_t value)
{
	switch (value)
	{
	case 0: return TRExLap2TerrainAdapt::F;
	case 1: return TRExLap2TerrainAdapt::E;
	case 2: return TRExLap2TerrainAdapt::D;
	case 3: return TRExLap2TerrainAdapt::C;
	case 4: return TRExLap2TerrainAdapt::B;
	case 5: return TRExLap2TerrainAdapt::A;
	case 6: return TRExLap2TerrainAdapt::S;
	default: return std::nullopt;
	}
}

/// <summary>
/// 地形適応文字列を変換する。
///
/// </summary>
/// <param name="value">FからSまでの地形適応文字列。</param>
/// <returns>対応する地形適応。未定義値ならnullopt。</returns>
std::optional<TRExLap2TerrainAdapt>
TRExLap2EnumParser<TRExLap2TerrainAdapt>::FromString(const std::string_view value)
{
	const std::string token = NormalizeEnumToken(value);

	if (token == "F") return TRExLap2TerrainAdapt::F;
	if (token == "E") return TRExLap2TerrainAdapt::E;
	if (token == "D") return TRExLap2TerrainAdapt::D;
	if (token == "C") return TRExLap2TerrainAdapt::C;
	if (token == "B") return TRExLap2TerrainAdapt::B;
	if (token == "A") return TRExLap2TerrainAdapt::A;
	if (token == "S") return TRExLap2TerrainAdapt::S;

	return std::nullopt;
}

/// <summary>
/// パイロット性格整数を変換する。
///
/// </summary>
/// <param name="value">-1から13までの性格値。</param>
/// <returns>対応する性格。範囲外ならnullopt。</returns>
std::optional<TRExLap2PilotPersonalityType>
TRExLap2EnumParser<TRExLap2PilotPersonalityType>::FromInt(const std::int64_t value)
{
	if (value < -1 || value > 13)
		return std::nullopt;

	return static_cast<TRExLap2PilotPersonalityType>(value);
}

/// <summary>
/// パイロット性格文字列を変換する。
///
/// </summary>
/// <param name="value">性格を表す英語識別子。</param>
/// <returns>対応する性格。未定義値ならnullopt。</returns>
std::optional<TRExLap2PilotPersonalityType>
TRExLap2EnumParser<TRExLap2PilotPersonalityType>::FromString(const std::string_view value)
{
	const std::string token = NormalizeEnumToken(value);

	if (token == "MACHINE") return TRExLap2PilotPersonalityType::Machine;
	if (token == "NORMAL") return TRExLap2PilotPersonalityType::Normal;
	if (token == "CONFIDENT") return TRExLap2PilotPersonalityType::Confident;
	if (token == "EXTREME_CONFIDENT") return TRExLap2PilotPersonalityType::ExtremeConfident;
	if (token == "CALM") return TRExLap2PilotPersonalityType::Calm;
	if (token == "CAUTIOUS") return TRExLap2PilotPersonalityType::Cautious;
	if (token == "OPTIMISTIC") return TRExLap2PilotPersonalityType::Optimistic;
	if (token == "HARDWORKER") return TRExLap2PilotPersonalityType::Hardworker;
	if (token == "SHORTTEMPERED") return TRExLap2PilotPersonalityType::Shorttempered;
	if (token == "BIGSHOT") return TRExLap2PilotPersonalityType::Bigshot;
	if (token == "SUPERBIGSHOT") return TRExLap2PilotPersonalityType::Superbigshot;
	if (token == "CUNNING") return TRExLap2PilotPersonalityType::Cunning;
	if (token == "CRUEL") return TRExLap2PilotPersonalityType::Cruel;
	if (token == "REKINDLED_EMBERS") return TRExLap2PilotPersonalityType::RekindledEmbers;
	if (token == "MADNESS") return TRExLap2PilotPersonalityType::Madness;

	return std::nullopt;
}

/// <summary>
/// パイロット性別整数を変換する。
///
/// </summary>
/// <param name="value">-2から1までの性別値。</param>
/// <returns>対応する性別。範囲外ならnullopt。</returns>
std::optional<TRExLap2PilotGenderType>
TRExLap2EnumParser<TRExLap2PilotGenderType>::FromInt(const std::int64_t value)
{
	if (value < -2 || value > 1)
		return std::nullopt;

	return static_cast<TRExLap2PilotGenderType>(value);
}

/// <summary>
/// パイロット性別文字列を変換する。
///
/// 草案どおり、Machine、Male、Female以外はUnknownとして扱う。
///
/// </summary>
/// <param name="value">性別を表す文字列。</param>
/// <returns>対応する性別。</returns>
std::optional<TRExLap2PilotGenderType>
TRExLap2EnumParser<TRExLap2PilotGenderType>::FromString(const std::string_view value)
{
	const std::string token = NormalizeEnumToken(value);

	if (token == "MACHINE") return TRExLap2PilotGenderType::Machine;
	if (token == "MALE") return TRExLap2PilotGenderType::Male;
	if (token == "FEMALE") return TRExLap2PilotGenderType::Female;

	return TRExLap2PilotGenderType::Unknown;
}

/// <summary>
/// 改造タイプ整数を変換する。
///
/// </summary>
/// <param name="value">0から3までの改造タイプ値。</param>
/// <returns>対応する改造タイプ。範囲外ならnullopt。</returns>
std::optional<TRExLap2ModifyType>
TRExLap2EnumParser<TRExLap2ModifyType>::FromInt(const std::int64_t value)
{
	if (value < 0 || value > 3)
		return std::nullopt;

	return static_cast<TRExLap2ModifyType>(value);
}

/// <summary>
/// 改造タイプ文字列を変換する。
///
/// </summary>
/// <param name="value">改造タイプ文字列。</param>
/// <returns>対応する改造タイプ。未定義値ならnullopt。</returns>
std::optional<TRExLap2ModifyType>
TRExLap2EnumParser<TRExLap2ModifyType>::FromString(const std::string_view value)
{
	const std::string token = NormalizeEnumToken(value);

	if (token == "COMMON") return TRExLap2ModifyType::COMMON;
	if (token == "MELEE") return TRExLap2ModifyType::MELEE;
	if (token == "RANGE") return TRExLap2ModifyType::RANGE;
	if (token == "CASTER") return TRExLap2ModifyType::CASTER;

	return std::nullopt;
}
