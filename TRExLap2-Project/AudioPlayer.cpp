#include "pch.hpp"
#include "AudioPlayer.hpp"

#include <tuple>
#include <xaudio2.h>

#define STB_VORBIS_HEADER_ONLY
#include "tp/stb_vorbis.c"

namespace
{
	/// <summary>HRESULT失敗をAPI名付きの例外へ変換する。</summary>
	void ThrowIfFailed(const HRESULT result, const char* const apiName)
	{
		if (SUCCEEDED(result)) return;
		std::ostringstream message;
		message << apiName << " failed. HRESULT=0x" << std::hex << static_cast<std::uint32_t>(result);
		throw std::runtime_error(message.str());
	}

	/// <summary>バイト列中のリトルエンディアン32bit値を読み取る。</summary>
	std::uint32_t ReadLittleEndian32(const std::vector<std::uint8_t>& data, const std::size_t offset)
	{
		if (offset + 4 > data.size()) throw std::runtime_error("The OGG metadata is truncated.");
		return static_cast<std::uint32_t>(data[offset]) | (static_cast<std::uint32_t>(data[offset + 1]) << 8)
			| (static_cast<std::uint32_t>(data[offset + 2]) << 16) | (static_cast<std::uint32_t>(data[offset + 3]) << 24);
	}

	/// <summary>VorbisコメントからLOOPSTARTとLOOPLENGTHを取得する。</summary>
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

	/// <summary>OGGページを辿り、ループ用サンプル位置とサンプルレートを取得する。</summary>
	std::optional<std::tuple<std::uint64_t, std::uint64_t, std::uint32_t>> ReadOggLoopTags(const std::filesystem::path& filePath)
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
					if (!loopStart || !loopLength || sampleRate == 0 || *loopLength == 0) throw std::runtime_error("The OGG loop tags are invalid.");
					return std::tuple{ *loopStart, *loopLength, sampleRate };
				}
				packet.clear();
			}
			position = payloadOffset + payloadSize;
		}
		throw std::runtime_error("The OGG Vorbis comment packet was not found.");
	}

	/// <summary>stb_vorbisでOGG Vorbisを16bit PCMへ全量デコードする。</summary>
	std::vector<std::uint8_t> DecodePcm16(const std::filesystem::path& filePath, WAVEFORMATEX& waveFormat)
	{
		std::ifstream file(filePath, std::ios::binary | std::ios::ate);
		if (!file) throw std::runtime_error("The OGG BGM could not be opened for decoding.");
		const std::streamsize byteCount = file.tellg();
		if (byteCount <= 0 || byteCount > std::numeric_limits<int>::max()) throw std::runtime_error("The OGG BGM has an unsupported size.");
		std::vector<std::uint8_t> encodedData(static_cast<std::size_t>(byteCount));
		file.seekg(0); file.read(reinterpret_cast<char*>(encodedData.data()), byteCount);
		if (!file) throw std::runtime_error("The OGG BGM could not be read completely.");
		int decoderError = 0;
		stb_vorbis* const decoder = stb_vorbis_open_memory(encodedData.data(), static_cast<int>(encodedData.size()), &decoderError, nullptr);
		if (!decoder) throw std::runtime_error("stb_vorbis could not open the OGG BGM. Error=" + std::to_string(decoderError));
		const stb_vorbis_info info = stb_vorbis_get_info(decoder);
		const int totalFrames = stb_vorbis_stream_length_in_samples(decoder);
		if (info.channels <= 0 || info.sample_rate <= 0 || totalFrames <= 0) { stb_vorbis_close(decoder); throw std::runtime_error("The OGG BGM has invalid PCM format information."); }
		waveFormat = { WAVE_FORMAT_PCM, static_cast<WORD>(info.channels), info.sample_rate, static_cast<DWORD>(info.sample_rate * info.channels * sizeof(std::int16_t)), static_cast<WORD>(info.channels * sizeof(std::int16_t)), 16, 0 };
		std::vector<std::int16_t> samples(static_cast<std::size_t>(totalFrames) * static_cast<std::size_t>(info.channels));
		int decodedFrames = 0;
		while (decodedFrames < totalFrames)
		{
			const int frames = stb_vorbis_get_samples_short_interleaved(decoder, info.channels, samples.data() + static_cast<std::size_t>(decodedFrames) * info.channels, (totalFrames - decodedFrames) * info.channels);
			if (frames <= 0) break;
			decodedFrames += frames;
		}
		stb_vorbis_close(decoder);
		samples.resize(static_cast<std::size_t>(decodedFrames) * static_cast<std::size_t>(info.channels));
		if (samples.empty()) throw std::runtime_error("The OGG decoder produced no PCM samples.");
		std::vector<std::uint8_t> pcmData(samples.size() * sizeof(std::int16_t));
		std::memcpy(pcmData.data(), samples.data(), pcmData.size());
		return pcmData;
	}
}

/// <summary>音声をPCMへデコードし、XAudio2によるシームレスなループ再生を準備する。</summary>
AudioPlayer::AudioPlayer(const std::filesystem::path& filePath)
{
	if (!std::filesystem::exists(filePath)) throw std::runtime_error("BGM file was not found: " + filePath.string());
	try
	{
		const auto loopTags = ReadOggLoopTags(filePath);
		WAVEFORMATEX waveFormat{};
		pcmData_ = DecodePcm16(filePath, waveFormat);
		sampleRate_ = waveFormat.nSamplesPerSec;
		const std::uint64_t totalSamples = pcmData_.size() / waveFormat.nBlockAlign;
		if (loopTags)
		{
			const auto [loopStart, loopLength, tagSampleRate] = *loopTags;
			if (tagSampleRate != sampleRate_ || loopStart >= totalSamples || loopLength > totalSamples - loopStart) throw std::runtime_error("The OGG loop tag range does not fit decoded PCM data.");
			loopStartSamples_ = loopStart;
			loopLengthSamples_ = loopLength;
			loopRangeMilliseconds_ = std::pair{ (loopStart * 1000) / sampleRate_, ((loopStart + loopLength) * 1000) / sampleRate_ };
		}
		else { loopStartSamples_ = 0; loopLengthSamples_ = totalSamples; }
		ThrowIfFailed(XAudio2Create(&xaudio2_), "XAudio2Create");
		ThrowIfFailed(xaudio2_->CreateMasteringVoice(&masteringVoice_), "IXAudio2::CreateMasteringVoice");
		ThrowIfFailed(xaudio2_->CreateSourceVoice(&sourceVoice_, &waveFormat), "IXAudio2::CreateSourceVoice");
	}
	catch (...) { ReleaseNoThrow(); throw; }
}

/// <summary>XAudio2およびMedia Foundationの資源を解放する。</summary>
AudioPlayer::~AudioPlayer() { ReleaseNoThrow(); }

/// <summary>PCM全体を一度だけ送信し、タグ範囲またはファイル全体をXAudio2内部で無限反復する。</summary>
void AudioPlayer::PlayLooping()
{
	if (playbackStarted_) return;
	XAUDIO2_BUFFER buffer{};
	buffer.AudioBytes = static_cast<UINT32>(pcmData_.size());
	buffer.pAudioData = pcmData_.data();
	buffer.LoopBegin = static_cast<UINT32>(loopStartSamples_);
	buffer.LoopLength = static_cast<UINT32>(loopLengthSamples_);
	buffer.LoopCount = XAUDIO2_LOOP_INFINITE;
	ThrowIfFailed(sourceVoice_->SubmitSourceBuffer(&buffer), "IXAudio2SourceVoice::SubmitSourceBuffer");
	ThrowIfFailed(sourceVoice_->Start(), "IXAudio2SourceVoice::Start");
	playbackStarted_ = true;
}

/// <summary>XAudio2のループは音声スレッドで処理されるため、フレーム更新は不要である。</summary>
void AudioPlayer::UpdateLooping() noexcept {}

/// <summary>ソースボイスが開始済みで、かつキューにPCMバッファが残っているかを返す。</summary>
bool AudioPlayer::IsPlaying() const noexcept
{
	if (!sourceVoice_ || !playbackStarted_) return false;
	XAUDIO2_VOICE_STATE state{};
	sourceVoice_->GetState(&state, XAUDIO2_VOICE_NOSAMPLESPLAYED);
	return state.BuffersQueued != 0;
}

/// <summary>タグ付きOGGとしてループ範囲を構成したかを返す。</summary>
bool AudioPlayer::UsesTaggedLoop() const noexcept { return loopRangeMilliseconds_.has_value(); }

/// <summary>初回イントロの終端を越え、XAudio2がタグ反復区間を再生中かを返す。</summary>
bool AudioPlayer::IsTaggedLoopRepeatPlaying() const noexcept
{
	if (!sourceVoice_ || !UsesTaggedLoop()) return false;
	XAUDIO2_VOICE_STATE state{};
	sourceVoice_->GetState(&state);
	return state.SamplesPlayed >= loopStartSamples_ + loopLengthSamples_;
}

/// <summary>XAudio2の総再生サンプル数を、元ファイル内の現在位置へ換算する。</summary>
std::uint64_t AudioPlayer::GetPlaybackPositionMilliseconds() const noexcept
{
	if (!sourceVoice_ || sampleRate_ == 0) return 0;
	XAUDIO2_VOICE_STATE state{};
	sourceVoice_->GetState(&state);
	std::uint64_t samplePosition = state.SamplesPlayed;
	const std::uint64_t loopEnd = loopStartSamples_ + loopLengthSamples_;
	if (samplePosition >= loopEnd && loopLengthSamples_ != 0) samplePosition = loopStartSamples_ + ((samplePosition - loopEnd) % loopLengthSamples_);
	return (samplePosition * 1000) / sampleRate_;
}

/// <summary>OGGタグ由来のミリ秒単位のループ範囲を返す。</summary>
const std::optional<std::pair<std::uint64_t, std::uint64_t>>& AudioPlayer::GetTaggedLoopRangeMilliseconds() const noexcept { return loopRangeMilliseconds_; }

/// <summary>ソースボイスを停止する。</summary>
void AudioPlayer::Stop() noexcept
{
	if (sourceVoice_) sourceVoice_->Stop();
	playbackStarted_ = false;
}

/// <summary>破棄順序を守って音声再生資源を解放する。</summary>
void AudioPlayer::ReleaseNoThrow() noexcept
{
	Stop();
	if (sourceVoice_) { sourceVoice_->DestroyVoice(); sourceVoice_ = nullptr; }
	if (masteringVoice_) { masteringVoice_->DestroyVoice(); masteringVoice_ = nullptr; }
	if (xaudio2_) { xaudio2_->Release(); xaudio2_ = nullptr; }
}
