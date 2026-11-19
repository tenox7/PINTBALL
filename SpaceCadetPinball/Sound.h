#pragma once
#include "maths.h"
#include "TPinballComponent.h"

// A sound effect, as 16-bit mono samples at Sound::MixRate.
struct WaveSample
{
	int16_t* Data;
	uint32_t Length;
};

struct ChannelInfo
{
	int TimeStamp;
	vector2 Position;
};

// Sound effects with waveOut: a mixer thread mixes the channels into a ring
// of waveOut buffers.
class Sound
{
public:
	static constexpr int MixRate = 22050;
	static std::vector<ChannelInfo> Channels;

	static bool Init(bool audioEnabled, int channels, bool enableFlag, int volume);
	static void Enable(bool enableFlag);
	static void Activate();
	static void Deactivate();
	static void Close();
	static void PlaySound(WaveSample* wavePtr, int time, TPinballComponent* soundSource, const char* info);
	static WaveSample* LoadWaveFile(const std::string& lpName);
	static void FreeSound(WaveSample* wave);
	static void SetChannels(int channels);
	static void SetVolume(int volume);
private:
	static constexpr int BufferCount = 4;
	static constexpr int BufferFrames = 512;

	struct MixChannel
	{
		const WaveSample* Wave;
		uint32_t Position;
		int VolumeLeft, VolumeRight; // 256 = full volume
	};

	static int num_channels;
	static bool enabled_flag;
	static int Volume;
	static bool DeviceOpen, LockReady;
	static HWAVEOUT WaveOut;
	static HANDLE MixEvent, MixThread;
	static volatile LONG StopThread;
	static CRITICAL_SECTION Lock;
	static WAVEHDR Headers[BufferCount];
	static int16_t* Buffers;
	static std::vector<MixChannel> MixChannels;

	static bool OpenDevice();
	static void CloseDevice();
	static DWORD WINAPI ThreadProc(LPVOID parameter);
	static void MixBuffer(int16_t* output, int frames);
	static void StopAll();
};
