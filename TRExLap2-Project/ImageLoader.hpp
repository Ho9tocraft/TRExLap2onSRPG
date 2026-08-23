#pragma once

#include <cstdint>
#include <filesystem>
#include <vector>

/**
 * @brief CPUメモリ上でRGBA8形式に復号された画像データ。
 *
 * pixelsは左上を原点とする行優先順で、各ピクセルをR/G/B/Aの4バイトで格納する。
 */
struct ImageRgba8 final
{
	std::uint32_t width = 0;
	std::uint32_t height = 0;
	std::vector<std::uint8_t> pixels;
};

/** @brief Windows Imaging Componentを用いて画像ファイルをVulkan転送用RGBA8へ復号する。 */
class ImageLoader final
{
public:
	/** PNGなどWIC対応画像を読み込み、32-bit RGBAへ統一して返す。 */
	static ImageRgba8 LoadRgba8(const std::filesystem::path& filePath);
};
