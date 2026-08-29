#pragma once

struct IXAudio2;
struct IXAudio2MasteringVoice;
struct IXAudio2SourceVoice;

/// <summary>
/// stb_vorbisでOGG音声をPCMへ展開し、XAudio2へ常駐バッファとして送るBGM再生器。
/// OGG VorbisのLOOPSTART/LOOPLENGTHタグはXAudio2の単一バッファ内ループとして扱う。
/// </summary>
class AudioPlayer final
{
public:
	/// <summary>指定した音声をPCMへデコードし、XAudio2のソースボイスを準備する。</summary>
	explicit AudioPlayer(const std::filesystem::path& filePath);
	/// <summary>再生を停止してXAudio2の資源を解放する。</summary>
	~AudioPlayer();
	AudioPlayer(const AudioPlayer&) = delete;
	AudioPlayer& operator=(const AudioPlayer&) = delete;
	AudioPlayer(AudioPlayer&&) = delete;
	AudioPlayer& operator=(AudioPlayer&&) = delete;
	/// <summary>初回は先頭から再生し、タグ付きOGGはLOOPEND後に指定範囲を無音なしで反復する。</summary>
	void PlayLooping();
	/// <summary>互換用の更新口。XAudio2のループは音声スレッド内で完結するため何もしない。</summary>
	void UpdateLooping() noexcept;
	/// <summary>XAudio2の再生状態が実行中かを取得する。</summary>
	[[nodiscard]] bool IsPlaying() const noexcept;
	/// <summary>有効なOGGループタグを検出したかを返す。</summary>
	[[nodiscard]] bool UsesTaggedLoop() const noexcept;
	/// <summary>初回イントロを終え、タグ反復区間に入ったかを返す。</summary>
	[[nodiscard]] bool IsTaggedLoopRepeatPlaying() const noexcept;
	/// <summary>現在位置を元ファイル内のミリ秒位置として返す。</summary>
	[[nodiscard]] std::uint64_t GetPlaybackPositionMilliseconds() const noexcept;
	/// <summary>タグ由来の開始・終了ミリ秒を返す。タグなしならnullopt。</summary>
	[[nodiscard]] const std::optional<std::pair<std::uint64_t, std::uint64_t>>& GetTaggedLoopRangeMilliseconds() const noexcept;
	/// <summary>再生を停止する。</summary>
	void Stop() noexcept;

private:
	/// <summary>デストラクタ専用の例外を送出しない解放処理を行う。</summary>
	void ReleaseNoThrow() noexcept;
	std::optional<std::pair<std::uint64_t, std::uint64_t>> loopRangeMilliseconds_;
	std::uint64_t loopStartSamples_ = 0;
	std::uint64_t loopLengthSamples_ = 0;
	std::uint32_t sampleRate_ = 0;
	std::vector<std::uint8_t> pcmData_;
	IXAudio2* xaudio2_ = nullptr;
	IXAudio2MasteringVoice* masteringVoice_ = nullptr;
	IXAudio2SourceVoice* sourceVoice_ = nullptr;
	bool playbackStarted_ = false;
};
