#include "pch.hpp"
#include "TomlLoader.hpp"

[[noreturn]] void ParseErrorThrower(const std::string_view fieldPath, const std::string_view msg) {
	throw std::runtime_error("The TOML file validation Error at '"
		+ std::string(fieldPath) + "': " + std::string(msg));
}

const toml::table& RequireTable(const toml::table& parent, std::string_view key, std::string_view fieldPath)
{
	const toml::table* value = parent[key].as_table();
	if (value == nullptr) ParseErrorThrower(fieldPath, "Expected a table.");
	return *value;
}

std::int64_t RequireInt64(const toml::table& parent, std::string_view key, std::string_view fieldPath)
{
	const auto& value = parent[key].value<std::int64_t>();
	if (!value.has_value()) ParseErrorThrower(fieldPath, "Expected an integer.");
	return *value;
}

std::string RequireString(const toml::table& parent, std::string_view key, std::string_view fieldPath)
{
	const auto value = parent[key].value<std::string>();
	if (!value.has_value()) ParseErrorThrower(fieldPath, "Expected a string.");
	return *value;
}

std::vector<std::u8string> ReadStringArray(const toml::array& array, std::string_view fieldPath)
{
	std::vector<std::u8string> result;
	result.reserve(array.size());

	for (const toml::node& node : array)
	{
		const auto str = node.value<std::string>();
		if (!str.has_value()) ParseErrorThrower(fieldPath, "Expected an array of strings.");
		result.push_back(ToU8Str(*str));
	}

	return result;
}

std::vector<std::u8string> RequireStringArray(const toml::table& parent, std::string_view key, std::string_view fieldPath)
{
	const toml::array* array = parent[key].as_array();
	if (array == nullptr) ParseErrorThrower(fieldPath, "Expected an array of strings.");

	return ReadStringArray(*array, fieldPath);
}

std::vector<std::u8string> ReadOptionalStringArray(const toml::table& parent, std::string_view key, std::string_view fieldPath)
{
	const toml::node* node = parent.get(key);
	if (node == nullptr) return {};

	const toml::array* array = node->as_array();
	if (array == nullptr) ParseErrorThrower(fieldPath, "Expected an array of strings.");

	return ReadStringArray(*array, fieldPath);
}

std::vector<std::u8string> RequireStrOrStrArray(const toml::table& parent, std::string_view key, std::string_view fieldPath)
{
	const toml::node* node = parent.get(key);

	if (node == nullptr) ParseErrorThrower(fieldPath, "Expected a string or an array of strings.");

	if (const auto value = node->value<std::string>(); value.has_value()) return { ToU8Str(*value) };

	if (const toml::array* array = node->as_array(); array != nullptr) return ReadStringArray(*array, fieldPath);

	ParseErrorThrower(fieldPath, "Expected a string or an array of strings.");
}

std::uint64_t RequireNonNegative(const std::int64_t value, std::string_view fieldPath)
{
	if (value < 0) ParseErrorThrower(fieldPath, "Expected a non-negative integer.");

	return static_cast<std::uint64_t>(value);
}

/// <summary>
/// 正の整数を検証する。
/// </summary>
/// <param name="value">検証対象の整数。</param>
/// <param name="fieldPath">エラー表示に使用するフィールドパス。</param>
/// <returns>検証済みの整数。</returns>
std::int64_t RequirePositive(const std::int64_t value, const std::string_view fieldPath)
{
	if (value <= 0LL) ParseErrorThrower(fieldPath, "Expected a positive integer.");
	return value;
}

/// <summary>
/// 数値配列または移動タイプ文字列を、移動タイプ配列へ正規化する。
/// </summary>
/// <param name="parent">取得元の親テーブル。</param>
/// <param name="key">テーブル内キー。</param>
/// <param name="fieldPath">エラー表示に使用するフィールドパス。</param>
/// <returns>正規化済み移動タイプ配列。</returns>
std::vector<TRExLap2StageMovableType> RequireMovableTypes(
	const toml::table& parent,
	const std::string_view key,
	const std::string_view fieldPath)
{
	const toml::node* node = parent.get(key);
	if (node == nullptr) ParseErrorThrower(fieldPath, "Expected an integer array or a string.");

	if (const toml::array* array = node->as_array(); array != nullptr)
	{
		if (array->empty()) ParseErrorThrower(fieldPath, "Expected a non-empty integer array.");

		std::vector<TRExLap2StageMovableType> result;
		result.reserve(array->size());

		for (const toml::node& element : *array)
		{
			const auto integerValue = element.value<std::int64_t>();
			if (!integerValue.has_value()) ParseErrorThrower(fieldPath, "Expected an array of integers.");

			const auto parsed = TryParseEnum<TRExLap2StageMovableType>(*integerValue);
			if (!parsed.has_value()) ParseErrorThrower(fieldPath, "The array contains an undefined movable type.");
			result.push_back(*parsed);
		}

		return result;
	}

	const auto stringValue = node->value<std::string>();
	if (!stringValue.has_value()) ParseErrorThrower(fieldPath, "Expected an integer array or a string.");

	if (const auto singleValue = TryParseEnum<TRExLap2StageMovableType>(*stringValue); singleValue.has_value())
		return { *singleValue };

	std::vector<TRExLap2StageMovableType> result;
	if (stringValue->find("空") != std::string::npos) result.push_back(TRExLap2StageMovableType::Air);
	if (stringValue->find("陸") != std::string::npos) result.push_back(TRExLap2StageMovableType::Ground);
	if (stringValue->find("水") != std::string::npos) result.push_back(TRExLap2StageMovableType::Underwater);

	if (result.empty()) ParseErrorThrower(fieldPath, "The string does not contain a valid movable type.");
	return result;
}

/// <summary>
/// 4要素の整数配列を、地形適応配列へ正規化する。
/// </summary>
/// <param name="parent">取得元の親テーブル。</param>
/// <param name="key">テーブル内キー。</param>
/// <param name="fieldPath">エラー表示に使用するフィールドパス。</param>
/// <returns>空・陸・海・宇の順に格納した地形適応配列。</returns>
std::array<TRExLap2TerrainAdapt, 4> RequireTerrainAdapts(
	const toml::table& parent,
	const std::string_view key,
	const std::string_view fieldPath)
{
	const toml::array* array = parent[key].as_array();
	if (array == nullptr || array->size() != 4)
		ParseErrorThrower(fieldPath, "Expected an array containing exactly four integers.");

	std::array<TRExLap2TerrainAdapt, 4> result{};
	std::size_t index = 0;

	for (const toml::node& element : *array)
	{
		const auto integerValue = element.value<std::int64_t>();
		if (!integerValue.has_value()) ParseErrorThrower(fieldPath, "Expected an array of integers.");

		const auto parsed = TryParseEnum<TRExLap2TerrainAdapt>(*integerValue);
		if (!parsed.has_value()) ParseErrorThrower(fieldPath, "The array contains an undefined terrain adapt value.");
		result[index] = *parsed;
		++index;
	}

	return result;
}

std::u8string ToU8Str(std::string_view str)
{
	std::u8string result;
	result.reserve(str.size());

	for (const unsigned char sbyte : str) { result.push_back(static_cast<char8_t>(sbyte)); }
	return result;
}
