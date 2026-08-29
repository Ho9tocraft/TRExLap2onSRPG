#pragma once
#include <array>
#include <cstdint>
#include <limits>
#include <string>
#include <string_view>
#include <type_traits>
#include <utility>
#include <vector>

#pragma warning(push)
#pragma warning(disable:6011 26800 26813)
#include <toml++/toml.hpp>
#pragma warning(pop)

#include "TRExLap2Enums.hpp"

[[noreturn]] void ParseErrorThrower(const std::string_view fieldPath, const std::string_view msg);

const toml::table& RequireTable(const toml::table& parent, std::string_view key, std::string_view fieldPath);

std::int64_t RequireInt64(const toml::table& parent, std::string_view key, std::string_view fieldPath);

std::string RequireString(const toml::table& parent, std::string_view key, std::string_view fieldPath);

std::vector<std::u8string> ReadStringArray(const toml::array& array, std::string_view fieldPath);

std::vector<std::u8string> RequireStringArray(const toml::table& parent, std::string_view key, std::string_view fieldPath);

std::vector<std::u8string> ReadOptionalStringArray(const toml::table& parent, std::string_view key, std::string_view fieldPath);

std::vector<std::u8string> RequireStrOrStrArray(const toml::table& parent, std::string_view key, std::string_view fieldPath);

std::uint64_t RequireNonNegative(const std::int64_t value, std::string_view fieldPath);

/// <summary>
/// 正の整数を検証する。
/// </summary>
/// <param name="value">検証対象の整数。</param>
/// <param name="fieldPath">エラー表示に使用するフィールドパス。</param>
/// <returns>検証済みの整数。</returns>
std::int64_t RequirePositive(std::int64_t value, std::string_view fieldPath);

/// <summary>
/// TOMLノードの整数または文字列を、定義済みのenum値へ正規化する。
/// </summary>
/// <typeparam name="EnumT">変換対象のenum class型。</typeparam>
/// <param name="parent">取得元の親テーブル。</param>
/// <param name="key">テーブル内キー。</param>
/// <param name="fieldPath">エラー表示に使用するフィールドパス。</param>
/// <returns>正規化済みのenum値。</returns>
template <typename EnumT>
	requires std::is_enum_v<EnumT>
EnumT RequireEnum(const toml::table& parent, std::string_view key, std::string_view fieldPath)
{
	const toml::node* node = parent.get(key);
	if (node == nullptr) ParseErrorThrower(fieldPath, "Expected an integer or a string.");

	if (const auto integerValue = node->value<std::int64_t>(); integerValue.has_value())
	{
		if (const auto parsed = TryParseEnum<EnumT>(*integerValue); parsed.has_value()) return *parsed;
		ParseErrorThrower(fieldPath, "The integer value is not defined for this enum.");
	}

	if (const auto stringValue = node->value<std::string>(); stringValue.has_value())
	{
		if (const auto parsed = TryParseEnum<EnumT>(*stringValue); parsed.has_value()) return *parsed;
		ParseErrorThrower(fieldPath, "The string value is not defined for this enum.");
	}

	ParseErrorThrower(fieldPath, "Expected an integer or a string.");
}

/// <summary>
/// 数値配列または移動タイプ文字列を、移動タイプ配列へ正規化する。
/// </summary>
/// <param name="parent">取得元の親テーブル。</param>
/// <param name="key">テーブル内キー。</param>
/// <param name="fieldPath">エラー表示に使用するフィールドパス。</param>
/// <returns>正規化済み移動タイプ配列。</returns>
std::vector<TRExLap2StageMovableType> RequireMovableTypes(const toml::table& parent, std::string_view key, std::string_view fieldPath);

/// <summary>
/// 4要素の整数配列を、地形適応配列へ正規化する。
/// </summary>
/// <param name="parent">取得元の親テーブル。</param>
/// <param name="key">テーブル内キー。</param>
/// <param name="fieldPath">エラー表示に使用するフィールドパス。</param>
/// <returns>空・陸・海・宇の順に格納した地形適応配列。</returns>
std::array<TRExLap2TerrainAdapt, 4> RequireTerrainAdapts(const toml::table& parent, std::string_view key, std::string_view fieldPath);

std::u8string ToU8Str(std::string_view str);
