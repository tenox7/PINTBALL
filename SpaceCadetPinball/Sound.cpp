#include "pch.h"
#include "options.h"
#include "Sound.h"
#include "maths.h"

constexpr int Sound::MixRate, Sound::BufferCount, Sound::BufferFrames;

int Sound::num_channels;
bool Sound::enabled_flag = false;
std::vector<ChannelInfo> Sound::Channels{};
int Sound::Volume = options::MaxVolume;
bool Sound::DeviceOpen = false, Sound::LockReady = false;
HWAVEOUT Sound::WaveOut = nullptr;
HANDLE Sound::MixEvent = nullptr, Sound::MixThread = nullptr;
volatile LONG Sound::StopThread = 0;
CRITICAL_SECTION Sound::Lock;
WAVEHDR Sound::Headers[BufferCount];
int16_t* Sound::Buffers = nullptr;
std::vector<Sound::MixChannel> Sound::MixChannels{};

// Returns true when the wave output device is open.
bool Sound::Init(bool audioEnabled, int channels, bool enableFlag, int volume)
{
	if (!LockReady)
	{
		InitializeCriticalSection(&Lock);
		LockReady = true;
	}

	Volume = volume;
	SetChannels(channels);
	if (audioEnabled)
		OpenDevice();
	Enable(enableFlag);
	return DeviceOpen;
}

bool Sound::OpenDevice()
{
	if (DeviceOpen)
		return true;

	WAVEFORMATEX format{};
	format.wFormatTag = WAVE_FORMAT_PCM;
	format.nChannels = 2;
	format.nSamplesPerSec = MixRate;
	format.wBitsPerSample = 16;
	format.nBlockAlign = 4;
	format.nAvgBytesPerSec = MixRate * 4;

	MixEvent = CreateEventA(nullptr, FALSE, FALSE, nullptr);
	if (!MixEvent)
		return false;

	if (waveOutOpen(&WaveOut, WAVE_MAPPER, &format, reinterpret_cast<DWORD>(MixEvent), 0,
	                CALLBACK_EVENT) != MMSYSERR_NOERROR)
	{
		CloseHandle(MixEvent);
		MixEvent = nullptr;
		WaveOut = nullptr;
		return false;
	}

	Buffers = new int16_t[BufferCount * BufferFrames * 2];
	std::memset(Buffers, 0, BufferCount * BufferFrames * 2 * sizeof(int16_t));
	for (int i = 0; i < BufferCount; i++)
	{
		auto& header = Headers[i];
		std::memset(&header, 0, sizeof header);
		header.lpData = reinterpret_cast<LPSTR>(Buffers + i * BufferFrames * 2);
		header.dwBufferLength = BufferFrames * 2 * sizeof(int16_t);
		waveOutPrepareHeader(WaveOut, &header, sizeof header);
	}

	StopThread = 0;
	DWORD threadId;
	MixThread = CreateThread(nullptr, 0, ThreadProc, nullptr, 0, &threadId);
	if (!MixThread)
	{
		for (auto& header : Headers)
			waveOutUnprepareHeader(WaveOut, &header, sizeof header);
		waveOutClose(WaveOut);
		WaveOut = nullptr;
		CloseHandle(MixEvent);
		MixEvent = nullptr;
		delete[] Buffers;
		Buffers = nullptr;
		return false;
	}
	SetThreadPriority(MixThread, THREAD_PRIORITY_HIGHEST);

	DeviceOpen = true;
	return true;
}

void Sound::CloseDevice()
{
	if (!DeviceOpen)
		return;

	InterlockedExchange(const_cast<LONG*>(&StopThread), 1);
	SetEvent(MixEvent);
	WaitForSingleObject(MixThread, 5000);
	CloseHandle(MixThread);
	MixThread = nullptr;

	waveOutReset(WaveOut);
	for (auto& header : Headers)
		waveOutUnprepareHeader(WaveOut, &header, sizeof header);
	waveOutClose(WaveOut);
	WaveOut = nullptr;

	CloseHandle(MixEvent);
	MixEvent = nullptr;
	delete[] Buffers;
	Buffers = nullptr;
	DeviceOpen = false;
}

DWORD WINAPI Sound::ThreadProc(LPVOID)
{
	// Queue all buffers, then fill each buffer again when it comes back,
	// in the same order.
	for (int i = 0; i < BufferCount; i++)
	{
		MixBuffer(reinterpret_cast<int16_t*>(Headers[i].lpData), BufferFrames);
		waveOutWrite(WaveOut, &Headers[i], sizeof(WAVEHDR));
	}

	int next = 0;
	while (true)
	{
		WaitForSingleObject(MixEvent, INFINITE);
		if (StopThread)
			break;

		while (Headers[next].dwFlags & WHDR_DONE)
		{
			auto& header = Headers[next];
			header.dwFlags &= ~WHDR_DONE;
			MixBuffer(reinterpret_cast<int16_t*>(header.lpData), BufferFrames);
			waveOutWrite(WaveOut, &header, sizeof header);
			next = (next + 1) % BufferCount;
		}
	}
	return 0;
}

void Sound::MixBuffer(int16_t* output, int frames)
{
	static int32_t mix[BufferFrames * 2];
	std::memset(mix, 0, frames * 2 * sizeof(int32_t));

	EnterCriticalSection(&Lock);
	for (auto& channel : MixChannels)
	{
		if (!channel.Wave)
			continue;

		auto remaining = channel.Wave->Length - channel.Position;
		auto count = remaining < static_cast<uint32_t>(frames) ? static_cast<int>(remaining) : frames;
		auto src = channel.Wave->Data + channel.Position;
		auto left = channel.VolumeLeft, right = channel.VolumeRight;
		for (int i = 0; i < count; i++)
		{
			int sample = src[i];
			mix[i * 2] += sample * left;
			mix[i * 2 + 1] += sample * right;
		}

		channel.Position += count;
		if (channel.Position >= channel.Wave->Length)
			channel.Wave = nullptr;
	}
	auto volume = Volume;
	LeaveCriticalSection(&Lock);

	for (int i = 0; i < frames * 2; i++)
	{
		auto value = (mix[i] >> 8) * volume / options::MaxVolume;
		if (value > 32767)
			value = 32767;
		else if (value < -32768)
			value = -32768;
		output[i] = static_cast<int16_t>(value);
	}
}

void Sound::StopAll()
{
	if (!LockReady)
		return;
	EnterCriticalSection(&Lock);
	for (auto& channel : MixChannels)
		channel.Wave = nullptr;
	LeaveCriticalSection(&Lock);
}

void Sound::Enable(bool enableFlag)
{
	enabled_flag = enableFlag;
	if (!enableFlag)
		StopAll();
}

void Sound::Activate()
{
	if (DeviceOpen)
		waveOutRestart(WaveOut);
}

void Sound::Deactivate()
{
	if (DeviceOpen)
		waveOutPause(WaveOut);
}

void Sound::Close()
{
	Enable(false);
	CloseDevice();
	Channels.clear();
	MixChannels.clear();
}

void Sound::PlaySound(WaveSample* wavePtr, int time, TPinballComponent* soundSource, const char* info)
{
	if (!DeviceOpen || !wavePtr || !enabled_flag || num_channels <= 0)
		return;

	auto left = 256, right = 256;
	vector2 soundPos{0.5f, 1.0f};
	if (options::Options.SoundStereo)
	{
		// Positional audio uses collision grid 2D coordinates normalized to [0, 1]
		// Point (0, 0) is bottom left table corner; point (1, 1) is top right table corner.
		// Z is defined as: 0 at table level, positive axis goes up from table surface.
		// Sound without position are assumed to be at the center top of the table.
		if (soundSource)
			soundPos = soundSource->get_coordinates();

		// Listener is positioned at the bottom center of the table,
		// at 0.5 height, so roughly a table half - length.
		vector3 soundPos3{soundPos.X, soundPos.Y, 0.0f};
		vector3 playerPos{0.5f, 0.0f, 0.5f};
		auto soundDir = maths::vector_sub(soundPos3, playerPos);

		// Angle from the positive Y axis, clockwise: positive is to the right.
		// A sound to the right makes the left channel quieter, as SDL_mixer does.
		auto angle = atan2(soundDir.X, soundDir.Y) * 180.0f / Pi;
		if (angle > 90.0f)
			angle = 90.0f;
		else if (angle < -90.0f)
			angle = -90.0f;
		if (angle >= 0)
			left = static_cast<int>(256 * (1.0f - angle / 90.0f));
		else
			right = static_cast<int>(256 * (1.0f + angle / 90.0f));

		// Distance is roughly in the [0, ~1.22] range; far sounds are quieter.
		auto distance = static_cast<int>(100.0f * maths::magnitude(soundDir));
		if (distance > 255)
			distance = 255;
		left = left * (255 - distance) / 255;
		right = right * (255 - distance) / 255;
	}

	EnterCriticalSection(&Lock);
	int channel = -1;
	for (int i = 0; i < num_channels; i++)
	{
		if (!MixChannels[i].Wave)
		{
			channel = i;
			break;
		}
	}
	if (channel < 0)
	{
		// All channels play: use the channel of the oldest sound.
		channel = 0;
		for (int i = 1; i < num_channels; i++)
		{
			if (Channels[i].TimeStamp < Channels[channel].TimeStamp)
				channel = i;
		}
	}

	auto& mixChannel = MixChannels[channel];
	mixChannel.Wave = wavePtr;
	mixChannel.Position = 0;
	mixChannel.VolumeLeft = left;
	mixChannel.VolumeRight = right;
	Channels[channel].TimeStamp = time;
	Channels[channel].Position = soundPos;
	LeaveCriticalSection(&Lock);
}

static uint32_t ReadU32(const uint8_t* data)
{
	return data[0] | (data[1] << 8) | (data[2] << 16) | (static_cast<uint32_t>(data[3]) << 24);
}

static uint16_t ReadU16(const uint8_t* data)
{
	return static_cast<uint16_t>(data[0] | (data[1] << 8));
}

// Loads a PCM WAV file and changes it to 16-bit mono at MixRate.
WaveSample* Sound::LoadWaveFile(const std::string& lpName)
{
	if (!DeviceOpen)
		return nullptr;

	auto file = fopenu(lpName.c_str(), "rb");
	if (!file)
		return nullptr;
	fseek(file, 0, SEEK_END);
	auto fileSize = ftell(file);
	fseek(file, 0, SEEK_SET);
	if (fileSize < 12)
	{
		fclose(file);
		return nullptr;
	}
	std::vector<uint8_t> data(static_cast<size_t>(fileSize));
	auto readSize = fread(data.data(), 1, data.size(), file);
	fclose(file);
	if (readSize != data.size() || memcmp(data.data(), "RIFF", 4) != 0 || memcmp(data.data() + 8, "WAVE", 4) != 0)
		return nullptr;

	unsigned formatTag = 0, channels = 0, sampleRate = 0, bits = 0;
	const uint8_t* samples = nullptr;
	uint32_t samplesSize = 0;
	for (size_t pos = 12; pos + 8 <= data.size();)
	{
		auto chunk = data.data() + pos;
		auto chunkSize = ReadU32(chunk + 4);
		auto available = data.size() - pos - 8;
		if (chunkSize > available)
			chunkSize = static_cast<uint32_t>(available);

		if (memcmp(chunk, "fmt ", 4) == 0 && chunkSize >= 16)
		{
			formatTag = ReadU16(chunk + 8);
			channels = ReadU16(chunk + 10);
			sampleRate = ReadU32(chunk + 12);
			bits = ReadU16(chunk + 22);
		}
		else if (memcmp(chunk, "data", 4) == 0)
		{
			samples = chunk + 8;
			samplesSize = chunkSize;
		}
		pos += 8 + ((static_cast<size_t>(chunkSize) + 1) & ~static_cast<size_t>(1));
	}

	if (formatTag != WAVE_FORMAT_PCM || !samples || channels < 1 || sampleRate < 1000 || (bits != 8 && bits != 16))
		return nullptr;

	auto frameSize = channels * (bits / 8);
	auto frames = samplesSize / frameSize;
	if (frames == 0)
		return nullptr;

	// Mono 16-bit samples at the rate of the file.
	std::vector<int16_t> mono(frames);
	for (uint32_t i = 0; i < frames; i++)
	{
		auto frame = samples + i * frameSize;
		int sum = 0;
		for (unsigned c = 0; c < channels; c++)
		{
			if (bits == 8)
				sum += (frame[c] - 128) << 8;
			else
				sum += static_cast<int16_t>(ReadU16(frame + c * 2));
		}
		mono[i] = static_cast<int16_t>(sum / static_cast<int>(channels));
	}

	// Change the rate, with linear interpolation.
	auto outLength = static_cast<uint32_t>(static_cast<uint64_t>(frames) * MixRate / sampleRate);
	if (outLength == 0)
		outLength = 1;
	auto wave = new WaveSample{new int16_t[outLength], outLength};
	auto step = static_cast<uint32_t>((static_cast<uint64_t>(sampleRate) << 16) / MixRate);
	uint32_t position = 0;
	for (uint32_t i = 0; i < outLength; i++, position += step)
	{
		auto index = position >> 16;
		auto fraction = static_cast<int>(position & 0xFFFF);
		if (index >= frames - 1)
		{
			wave->Data[i] = mono[frames - 1];
			continue;
		}
		int a = mono[index], b = mono[index + 1];
		wave->Data[i] = static_cast<int16_t>(a + (((b - a) * fraction) >> 16));
	}
	return wave;
}

void Sound::FreeSound(WaveSample* wave)
{
	if (!wave)
		return;

	// The mixer thread can play this sound now.
	if (LockReady)
	{
		EnterCriticalSection(&Lock);
		for (auto& channel : MixChannels)
		{
			if (channel.Wave == wave)
				channel.Wave = nullptr;
		}
		LeaveCriticalSection(&Lock);
	}

	delete[] wave->Data;
	delete wave;
}

void Sound::SetChannels(int channels)
{
	if (channels <= 0)
		channels = 8;

	if (LockReady)
		EnterCriticalSection(&Lock);
	num_channels = channels;
	Channels.resize(num_channels);
	MixChannels.resize(num_channels, MixChannel{nullptr, 0, 256, 256});
	if (LockReady)
		LeaveCriticalSection(&Lock);
	SetVolume(Volume);
}

void Sound::SetVolume(int volume)
{
	Volume = volume;
}
