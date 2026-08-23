#include "pch.hpp"
#include "AudioPlayer.hpp"

namespace
{
	std::atomic_uint64_t gAudioPlayerSerial = 0;

	/** MCIコマンドでファイルパスを安全に引用符で囲む。 */
	std::wstring QuoteMciPath(const std::filesystem::path& filePath)
	{
		return L"\"" + filePath.wstring() + L"\"";
	}

	/** WindowsのUTF-16エラー文字列を例外メッセージ用のUTF-8へ変換する。 */
	std::string WideToUtf8(const std::wstring_view text)
	{
		if (text.empty()) return {};
		const int byteCount = WideCharToMultiByte(CP_UTF8, 0, text.data(), static_cast<int>(text.size()), nullptr, 0, nullptr, nullptr);
		std::string result(static_cast<std::size_t>(byteCount), '\0');
		WideCharToMultiByte(CP_UTF8, 0, text.data(), static_cast<int>(text.size()), result.data(), byteCount, nullptr, nullptr);
		return result;
	}

	/** バイト列中のリトルエンディアン32bit値を読み取る。 */
	std::uint32_t ReadLittleEndian32(const std::vector<std::uint8_t>& data, const std::size_t offset)
	{
		if (offset + 4 > data.size()) throw std::runtime_error("The OGG metadata is truncated.");
		return static_cast<std::uint32_t>(data[offset])
			| (static_cast<std::uint32_t>(data[offset + 1]) << 8)
			| (static_cast<std::uint32_t>(data[offset + 2]) << 16)
			| (static_cast<std::uint32_t>(data[offset + 3]) << 24);
	}

	/** VorbisコメントからLOOPSTARTとLOOPLENGTHを一度の走査で取得する。 */
	std::pair<std::optional<std::uint64_t>, std::optional<std::uint64_t>> ReadVorbisLoopTags(const std::vector<std::uint8_t>& packet, std::size_t& offset)
	{
		const std::uint32_t commentCount = ReadLittleEndian32(packet, offset);
		offset += 4;
		std::optional<std::uint64_t> loopStart;
		std::optional<std::uint64_t> loopLength;
		for (std::uint32_t index = 0; index < commentCount; ++index)
		{
			const std::uint32_t length = ReadLittleEndian32(packet, offset);
			offset += 4;
			if (offset + length > packet.size()) throw std::runtime_error("The OGG comment is truncated.");
			const std::string_view comment(reinterpret_cast<const char*>(packet.data() + offset), length);
			offset += length;
			const std::size_t separator = comment.find('=');
			if (separator == std::string_view::npos) continue;
			try
			{
				if (comment.substr(0, separator) == "LOOPSTART") loopStart = std::stoull(std::string(comment.substr(separator + 1)));
				if (comment.substr(0, separator) == "LOOPLENGTH") loopLength = std::stoull(std::string(comment.substr(separator + 1)));
			}
			catch (const std::exception&) { throw std::runtime_error("The OGG loop tag is not an unsigned integer."); }
		}
		return { loopStart, loopLength };
	}

	/** OGGページを辿り、VorbisのサンプルレートとLOOPSTART/LOOPLENGTHをミリ秒区間へ変換する。 */
	std::optional<std::pair<std::uint64_t, std::uint64_t>> ReadOggLoopRangeMilliseconds(const std::filesystem::path& filePath)
	{
		if (filePath.extension() != L".ogg") return std::nullopt;
		std::ifstream file(filePath, std::ios::binary | std::ios::ate);
		if (!file) throw std::runtime_error("The OGG BGM could not be opened for metadata parsing.");
		const std::streamsize byteCount = file.tellg();
		if (byteCount <= 0) throw std::runtime_error("The OGG BGM is empty.");
		std::vector<std::uint8_t> data(static_cast<std::size_t>(byteCount));
		file.seekg(0); file.read(reinterpret_cast<char*>(data.data()), byteCount);
		if (!file) throw std::runtime_error("The OGG BGM could not be read completely.");

		std::vector<std::uint8_t> packet;
		std::uint32_t sampleRate = 0;
		std::size_t position = 0;
		std::uint32_t completedPackets = 0;
		while (position + 27 <= data.size())
		{
			if (std::memcmp(data.data() + position, "OggS", 4) != 0) throw std::runtime_error("An invalid OGG page was found.");
			const std::size_t segmentCount = data[position + 26];
			const std::size_t segmentsOffset = position + 27;
			const std::size_t payloadOffset = segmentsOffset + segmentCount;
			if (payloadOffset > data.size()) throw std::runtime_error("The OGG segment table is truncated.");
			std::size_t payloadSize = 0;
			for (std::size_t segment = 0; segment < segmentCount; ++segment) payloadSize += data[segmentsOffset + segment];
			if (payloadOffset + payloadSize > data.size()) throw std::runtime_error("The OGG page payload is truncated.");
			std::size_t payloadPosition = payloadOffset;
			for (std::size_t segment = 0; segment < segmentCount; ++segment)
			{
				const std::size_t length = data[segmentsOffset + segment];
				packet.insert(packet.end(), data.begin() + static_cast<std::ptrdiff_t>(payloadPosition), data.begin() + static_cast<std::ptrdiff_t>(payloadPosition + length));
				payloadPosition += length;
				if (length == 255) continue;
				++completedPackets;
				if (completedPackets == 1)
				{
					if (packet.size() < 16 || packet[0] != 1 || std::memcmp(packet.data() + 1, "vorbis", 6) != 0) throw std::runtime_error("The OGG file does not contain a Vorbis identification packet.");
					sampleRate = ReadLittleEndian32(packet, 12);
				}
				else if (completedPackets == 2)
				{
					if (packet.size() < 11 || packet[0] != 3 || std::memcmp(packet.data() + 1, "vorbis", 6) != 0) throw std::runtime_error("The OGG file does not contain a Vorbis comment packet.");
					std::size_t commentOffset = 7;
					const std::uint32_t vendorLength = ReadLittleEndian32(packet, commentOffset);
					commentOffset += 4 + vendorLength;
					if (commentOffset > packet.size()) throw std::runtime_error("The OGG vendor comment is truncated.");
					const auto [loopStart, loopLength] = ReadVorbisLoopTags(packet, commentOffset);
					if (!loopStart || !loopLength || sampleRate == 0) return std::nullopt;
					const std::uint64_t startMilliseconds = (*loopStart * 1000) / sampleRate;
					const std::uint64_t endMilliseconds = ((*loopStart + *loopLength) * 1000) / sampleRate;
					if (endMilliseconds <= startMilliseconds) throw std::runtime_error("The OGG loop range is invalid.");
					return std::pair{ startMilliseconds, endMilliseconds };
				}
				packet.clear();
			}
			position = payloadOffset + payloadSize;
		}
		throw std::runtime_error("The OGG Vorbis comment packet was not found.");
	}
}

/** 音声ファイルをMCIデバイスとして開き、OGGならループタグも解析する。 */
AudioPlayer::AudioPlayer(const std::filesystem::path& filePath)
	: loopRangeMilliseconds_(ReadOggLoopRangeMilliseconds(filePath)), alias_(L"TRExLap2Bgm" + std::to_wstring(++gAudioPlayerSerial))
{
	if (!std::filesystem::exists(filePath))
	{
		throw std::runtime_error("BGM file was not found: " + filePath.string());
	}

	SendCommand(L"open " + QuoteMciPath(filePath) + L" type mpegvideo alias " + alias_);
	isOpen_ = true;
}

/** 再生を止め、MCIデバイスを閉じる。 */
AudioPlayer::~AudioPlayer()
{
	CloseNoThrow();
}

/** タグ付きOGGはイントロを含む先頭からLOOPENDまで、その他は通常の無限ループとして再生する。 */
void AudioPlayer::PlayLooping()
{
	taggedLoopRepeatStarted_ = false;
	std::wstring command = L"play " + alias_;
	if (loopRangeMilliseconds_)
	{
		command += L" from 0 to " + std::to_wstring(loopRangeMilliseconds_->second);
		SendCommand(command);
	}
	else SendCommand(command + L" repeat");
	if (!IsPlaying()) throw std::runtime_error("MCI accepted the BGM play command but did not enter the playing state.");
}

/** 初回のイントロ再生が停止した時点で、LOOPSTARTからLOOPENDまでの反復再生へ移行する。 */
void AudioPlayer::UpdateLooping()
{
	if (!loopRangeMilliseconds_ || taggedLoopRepeatStarted_ || IsPlaying()) return;
	const std::wstring command = L"play " + alias_
		+ L" from " + std::to_wstring(loopRangeMilliseconds_->first)
		+ L" to " + std::to_wstring(loopRangeMilliseconds_->second)
		+ L" repeat";
	SendCommand(command);
	if (!IsPlaying()) throw std::runtime_error("MCI did not enter the tagged OGG repeat state.");
	taggedLoopRepeatStarted_ = true;
}

/** MCIのmode問い合わせを用いて、実際の再生開始状態を検証する。 */
bool AudioPlayer::IsPlaying() const
{
	std::array<wchar_t, 32> mode{};
	SendCommand(L"status " + alias_ + L" mode", mode.data(), static_cast<std::uint32_t>(mode.size()));
	return std::wstring_view(mode.data()) == L"playing";
}

/** OGGのLOOPSTART/LOOPLENGTHから再生区間を構成したかを返す。 */
bool AudioPlayer::UsesTaggedLoop() const noexcept
{
	return loopRangeMilliseconds_.has_value();
}

/** 初回イントロの後にタグ付きループ範囲へ移行したかを返す。 */
bool AudioPlayer::IsTaggedLoopRepeatPlaying() const noexcept
{
	return taggedLoopRepeatStarted_;
}

/** MCIのposition問い合わせを整数ミリ秒として返す。 */
std::uint64_t AudioPlayer::GetPlaybackPositionMilliseconds() const
{
	std::array<wchar_t, 32> position{};
	SendCommand(L"status " + alias_ + L" position", position.data(), static_cast<std::uint32_t>(position.size()));
	try { return std::stoull(position.data()); }
	catch (const std::exception&) { throw std::runtime_error("MCI returned a non-numeric playback position."); }
}

/** OGGのループタグから構成した再生区間を返す。 */
const std::optional<std::pair<std::uint64_t, std::uint64_t>>& AudioPlayer::GetTaggedLoopRangeMilliseconds() const noexcept
{
	return loopRangeMilliseconds_;
}

/** MCIデバイスを停止する。 */
void AudioPlayer::Stop() const
{
	SendCommand(L"stop " + alias_);
}

/** MCIエラー文字列を含め、音声処理の失敗理由を呼び出し元へ伝える。 */
void AudioPlayer::SendCommand(const std::wstring& command, wchar_t* resultBuffer, std::uint32_t resultBufferLength) const
{
	const MCIERROR result = mciSendStringW(command.c_str(), resultBuffer, resultBufferLength, nullptr);
	if (result == 0) return;

	std::array<wchar_t, 256> errorMessage{};
	mciGetErrorStringW(result, errorMessage.data(), static_cast<UINT>(errorMessage.size()));
	throw std::runtime_error("MCI command failed: " + WideToUtf8(command) + " (" + WideToUtf8(errorMessage.data()) + ")");
}

/** 例外を許容できないデストラクタから、音声デバイスを確実に閉じる。 */
void AudioPlayer::CloseNoThrow() noexcept
{
	if (!isOpen_) return;
	mciSendStringW((L"stop " + alias_).c_str(), nullptr, 0, nullptr);
	mciSendStringW((L"close " + alias_).c_str(), nullptr, 0, nullptr);
	isOpen_ = false;
}
