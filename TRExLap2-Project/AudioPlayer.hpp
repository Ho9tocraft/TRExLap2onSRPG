#pragma once

/**
 * Win32 MCIを使い、アプリケーションの寿命に結び付いたBGM再生を管理する。
 *
 * MCIはWindows標準のメディアデコーダーを利用し、MP3/OGGを非同期再生する。
 * OGG VorbisにLOOPSTART/LOOPLENGTHタグがある場合は、その区間だけを繰り返す。
 */
class AudioPlayer final
{
public:
	/** 指定した音声ファイルをMCIで開き、再生可能な状態にする。 */
	explicit AudioPlayer(const std::filesystem::path& filePath);

	/** 再生停止とMCIデバイスのクローズを行い、例外を送出しない。 */
	~AudioPlayer();

	AudioPlayer(const AudioPlayer&) = delete;
	AudioPlayer& operator=(const AudioPlayer&) = delete;
	AudioPlayer(AudioPlayer&&) = delete;
	AudioPlayer& operator=(AudioPlayer&&) = delete;

	/** BGMを非同期で開始する。タグ付きOGGは先頭から最初のLOOPENDまで再生する。 */
	void PlayLooping();

	/** 初回再生の終了を監視し、タグ付きOGGをLOOPSTARTから反復再生へ切り替える。 */
	void UpdateLooping();

	/** 現在のMCI再生状態がplayingであるかを取得する。 */
	[[nodiscard]] bool IsPlaying() const;

	/** OGG Vorbisコメントから有効なループ区間を取得できたかを返す。 */
	[[nodiscard]] bool UsesTaggedLoop() const noexcept;

	/** 初回イントロ再生を終え、LOOPSTARTからの反復再生へ移行済みかを返す。 */
	[[nodiscard]] bool IsTaggedLoopRepeatPlaying() const noexcept;

	/** 現在のMCI再生位置をミリ秒で取得する。 */
	[[nodiscard]] std::uint64_t GetPlaybackPositionMilliseconds() const;

	/** OGGタグから変換した開始・終了ミリ秒を返す。タグなしならnulloptを返す。 */
	[[nodiscard]] const std::optional<std::pair<std::uint64_t, std::uint64_t>>& GetTaggedLoopRangeMilliseconds() const noexcept;

	/** 再生を停止する。停止済みでも安全に呼び出せる。 */
	void Stop() const;

private:
	/** MCIコマンドを送信し、失敗時はコマンドとMCIの詳細を含む例外を送出する。 */
	void SendCommand(const std::wstring& command, wchar_t* resultBuffer = nullptr, std::uint32_t resultBufferLength = 0) const;

	/** デストラクタ専用の例外を送出しないclose処理を行う。 */
	void CloseNoThrow() noexcept;

	std::optional<std::pair<std::uint64_t, std::uint64_t>> loopRangeMilliseconds_;
	std::wstring alias_;
	bool isOpen_ = false;
	bool taggedLoopRepeatStarted_ = false;
};
