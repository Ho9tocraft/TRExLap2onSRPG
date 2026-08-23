#include "pch.hpp"
#include "ImageLoader.hpp"

#include <wincodec.h>
#include <wrl/client.h>

namespace
{
/** COM API失敗を、呼び出し式とHRESULTを含む例外として送出する。 */
[[noreturn]] void ThrowIfFailed(HRESULT result, std::string_view expression)
{
	std::ostringstream message;
	message << "WIC call failed: " << expression << " (HRESULT 0x" << std::hex << static_cast<unsigned long>(result) << ')';
	throw std::runtime_error(message.str());
}

/** HRESULTが失敗値なら画像読込を中断する。 */
void CheckHr(HRESULT result, std::string_view expression)
{
	if (FAILED(result)) ThrowIfFailed(result, expression);
}

/** 現在のスレッドでWICを利用する間だけ、MTA COMアパートメントを初期化する。 */
class ComApartment final
{
public:
	/** COMをMTAとして初期化し、失敗時は画像読込を中断する。 */
	ComApartment()
		: result_(CoInitializeEx(nullptr, COINIT_MULTITHREADED))
	{
		CheckHr(result_, "CoInitializeEx(nullptr, COINIT_MULTITHREADED)");
	}

	/** このインスタンスが行ったCOM初期化を対応するCoUninitializeで解放する。 */
	~ComApartment()
	{
		CoUninitialize();
	}

	ComApartment(const ComApartment&) = delete;
	ComApartment& operator=(const ComApartment&) = delete;

private:
	HRESULT result_ = E_FAIL;
};
}

/** PNGなどWIC対応画像を読み込み、32-bit RGBAへ統一して返す。 */
ImageRgba8 ImageLoader::LoadRgba8(const std::filesystem::path& filePath)
{
	ComApartment comApartment;
	Microsoft::WRL::ComPtr<IWICImagingFactory> factory;
	CheckHr(CoCreateInstance(CLSID_WICImagingFactory, nullptr, CLSCTX_INPROC_SERVER, IID_PPV_ARGS(&factory)), "CoCreateInstance(CLSID_WICImagingFactory)");

	Microsoft::WRL::ComPtr<IWICBitmapDecoder> decoder;
	CheckHr(factory->CreateDecoderFromFilename(filePath.c_str(), nullptr, GENERIC_READ, WICDecodeMetadataCacheOnLoad, &decoder), "IWICImagingFactory::CreateDecoderFromFilename");
	Microsoft::WRL::ComPtr<IWICBitmapFrameDecode> frame;
	CheckHr(decoder->GetFrame(0, &frame), "IWICBitmapDecoder::GetFrame(0)");

	UINT width = 0;
	UINT height = 0;
	CheckHr(frame->GetSize(&width, &height), "IWICBitmapFrameDecode::GetSize");
	if (width == 0 || height == 0) throw std::runtime_error("The image has an empty extent.");
	const std::size_t pixelCount = static_cast<std::size_t>(width) * height;
	if (pixelCount > std::numeric_limits<std::size_t>::max() / 4) throw std::runtime_error("The image is too large to decode.");

	Microsoft::WRL::ComPtr<IWICFormatConverter> converter;
	CheckHr(factory->CreateFormatConverter(&converter), "IWICImagingFactory::CreateFormatConverter");
	CheckHr(converter->Initialize(frame.Get(), GUID_WICPixelFormat32bppRGBA, WICBitmapDitherTypeNone, nullptr, 0.0, WICBitmapPaletteTypeCustom), "IWICFormatConverter::Initialize");

	ImageRgba8 result{};
	result.width = width;
	result.height = height;
	result.pixels.resize(pixelCount * 4);
	CheckHr(converter->CopyPixels(nullptr, width * 4, static_cast<UINT>(result.pixels.size()), result.pixels.data()), "IWICBitmapSource::CopyPixels");
	return result;
}
