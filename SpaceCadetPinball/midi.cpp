#include "pch.h"
#include "midi.h"

#include "options.h"
#include "pb.h"
#include "winmain.h"


std::vector<std::string> midi::TempFiles{};
std::string midi::track1, midi::track2, midi::track3;
MidiTracks midi::active_track, midi::NextTrack;
int midi::Volume = options::MaxVolume;
bool midi::IsPlaying = false, midi::Enabled = false;
MCIDEVICEID midi::DeviceId = 0;

constexpr uint32_t MakeFourCC(uint8_t a, uint8_t b, uint8_t c, uint8_t d)
{
	return static_cast<uint32_t>((d << 24) | (c << 16) | (b << 8) | a);
}

int ToVariableLen(uint32_t value, uint32_t& dst)
{
	auto count = 1;
	dst = value & 0x7F;

	while ((value >>= 7))
	{
		dst <<= 8;
		dst |= ((value & 0x7F) | 0x80);
		count++;
	}

	return count;
}

void midi::music_play()
{
	if (!IsPlaying)
	{
		IsPlaying = true;
		play_track(NextTrack, true);
		NextTrack = MidiTracks::None;
	}
}

void midi::music_stop()
{
	if (IsPlaying)
	{
		IsPlaying = false;
		NextTrack = active_track;
		StopPlayback();
	}
}

void midi::StopPlayback()
{
	if (DeviceId)
	{
		mciSendCommandA(DeviceId, MCI_STOP, 0, 0);
		mciSendCommandA(DeviceId, MCI_CLOSE, 0, 0);
		DeviceId = 0;
	}
	active_track = MidiTracks::None;
}

int midi::music_init(bool enabled, int volume)
{
	// Without a MIDI output device, the sequencer can not play.
	Enabled = enabled && midiOutGetNumDevs() > 0;
	Volume = volume;
	active_track = MidiTracks::None;
	NextTrack = MidiTracks::None;
	IsPlaying = false;
	track1.clear();
	track2.clear();
	track3.clear();

	if (pb::FullTiltMode)
	{
		track1 = load_track("TABA1");
		track2 = load_track("TABA2");
		track3 = load_track("TABA3");

		// FT demo .006 has only one music track, but it is nearly 9 min. long
		if (track1.empty() && pb::FullTiltDemoMode)
			track1 = load_track("DEMO");
	}
	else
	{
		// 3DPB has only one music track. PINBALL2.MID is a bitmap font, in the same format as PB_MSGFT.bin
		track1 = load_track("PINBALL");
	}

	return !track1.empty();
}

void midi::music_shutdown()
{
	music_stop();
	StopPlayback();

	for (const auto& file : TempFiles)
		DeleteFileA(file.c_str());
	TempFiles.clear();
	track1.clear();
	track2.clear();
	track3.clear();
	active_track = MidiTracks::None;
}

void midi::SetVolume(int volume)
{
	Volume = volume;
	if (DeviceId)
		ApplyVolume();
}

// MCI has no volume for the sequencer: set the volume of the MIDI devices.
// Devices without a volume control return an error, which is not a problem.
void midi::ApplyVolume()
{
	auto level = static_cast<DWORD>(Clamp(Volume, 0, options::MaxVolume) * 0xFFFF / options::MaxVolume);
	auto stereoLevel = level | (level << 16);
	auto count = midiOutGetNumDevs();
	for (UINT device = 0; device < count; device++)
	{
#if WINVER >= 0x0400
		midiOutSetVolume(reinterpret_cast<HMIDIOUT>(static_cast<size_t>(device)), stereoLevel);
#else
		midiOutSetVolume(device, stereoLevel);
#endif
	}
}

std::string midi::load_track(std::string fileName)
{
	if (!Enabled || pb::quickFlag)
		return {};

	if (pb::FullTiltMode)
	{
		// FT sounds are in SOUND subfolder
		fileName.insert(0, 1, PathSeparator);
		fileName.insert(0, "SOUND");
	}

	auto path = load_track_sub(fileName, true);
	if (path.empty())
		path = load_track_sub(fileName, false);
	return path;
}

std::string midi::load_track_sub(std::string fileName, bool isMidi)
{
	// FT has music in two formats, depending on game version: MIDI in 16bit, MIDS in 32bit.
	// 3DPB music is MIDI only.
	fileName += isMidi ? ".MID" : ".MDS";
	auto filePath = pb::make_path_name(fileName);
	if (isMidi)
	{
		auto fileHandle = fopenu(filePath.c_str(), "rb");
		if (!fileHandle)
			return {};
		fclose(fileHandle);
		return filePath;
	}

	auto midi = MdsToMidi(filePath);
	if (!midi)
		return {};
	auto tempPath = WriteTempFile(*midi);
	delete midi;
	return tempPath;
}

// Writes a converted MIDI file to the temporary folder. music_shutdown deletes it.
std::string midi::WriteTempFile(const std::vector<uint8_t>& data)
{
	char tempFolder[MAX_PATH], tempPath[MAX_PATH];
	auto length = GetTempPathA(MAX_PATH, tempFolder);
	if (length == 0 || length >= MAX_PATH)
		return {};
	if (!GetTempFileNameA(tempFolder, "pbm", 0, tempPath))
		return {};

	auto written = 0u;
	auto fileHandle = fopenu(tempPath, "wb");
	if (fileHandle)
	{
		written = static_cast<unsigned>(fwrite(data.data(), 1, data.size(), fileHandle));
		fclose(fileHandle);
	}
	if (written != data.size())
	{
		DeleteFileA(tempPath);
		return {};
	}

	TempFiles.push_back(tempPath);
	return tempPath;
}

bool midi::OpenDevice(const std::string& path)
{
	MCI_OPEN_PARMSA openParms{};
	openParms.lpstrDeviceType = "sequencer";
	openParms.lpstrElementName = path.c_str();
	if (mciSendCommandA(0, MCI_OPEN, MCI_OPEN_TYPE | MCI_OPEN_ELEMENT, reinterpret_cast<DWORD>(&openParms)) != 0)
		return false;

	DeviceId = openParms.wDeviceID;
	return true;
}

// Plays from the start. MCI sends MM_MCINOTIFY to the main window at the end.
bool midi::PlayFromStart()
{
	MCI_PLAY_PARMS playParms{};
	playParms.dwCallback = reinterpret_cast<DWORD>(winmain::MainWindow);
	playParms.dwFrom = 0;
	return mciSendCommandA(DeviceId, MCI_PLAY, MCI_FROM | MCI_NOTIFY, reinterpret_cast<DWORD>(&playParms)) == 0;
}

void midi::OnMciNotify(WPARAM flags, LPARAM deviceId)
{
	// The track ended: play it again. A stopped or replaced track sends
	// MCI_NOTIFY_ABORTED or MCI_NOTIFY_SUPERSEDED.
	if (flags == MCI_NOTIFY_SUCCESSFUL && DeviceId && static_cast<MCIDEVICEID>(deviceId) == DeviceId &&
		IsPlaying && active_track != MidiTracks::None)
	{
		if (!PlayFromStart())
			StopPlayback();
	}
}

bool midi::play_track(MidiTracks track, bool replay)
{
	auto midi = TrackToMidi(track);
	if (!midi || (!replay && active_track == track))
		return false;

	StopPlayback();

	if (!IsPlaying)
	{
		NextTrack = track;
		return false;
	}

	if (!OpenDevice(*midi))
		return false;
	if (!PlayFromStart())
	{
		StopPlayback();
		return false;
	}

	ApplyVolume();
	active_track = track;
	return true;
}

MidiTracks midi::get_active_track()
{
	if (!IsPlaying)
		return NextTrack;
	else
		return active_track;
}

const std::string* midi::TrackToMidi(MidiTracks track)
{
	const std::string* midi;
	switch (track)
	{
	default:
	case MidiTracks::None:
		midi = nullptr;
		break;
	case MidiTracks::Track1:
		midi = &track1;
		break;
	case MidiTracks::Track2:
		midi = &track2;
		break;
	case MidiTracks::Track3:
		midi = &track3;
		break;
	}
	return midi && !midi->empty() ? midi : nullptr;
}

/// <summary>
/// The MCI sequencer does not play MIDS. To play FT music, a conversion to MIDI is required.
/// </summary>
/// <param name="file">Path to .MDS file</param>
/// <returns>Vector that contains MIDI file</returns>
std::vector<uint8_t>* midi::MdsToMidi(std::string file)
{
	auto fileHandle = fopenu(file.c_str(), "rb");
	if (!fileHandle)
		return nullptr;

	fseek(fileHandle, 0, SEEK_END);
	auto fileSize = static_cast<uint32_t>(ftell(fileHandle));
	auto buffer = new uint8_t[fileSize];
	auto fileBuf = reinterpret_cast<riff_header*>(buffer);
	fseek(fileHandle, 0, SEEK_SET);
	fread(fileBuf, 1, fileSize, fileHandle);
	fclose(fileHandle);

	int returnCode = 0;
	std::vector<uint8_t>* midiOut = nullptr;
	do
	{
		if (fileSize < 12)
		{
			returnCode = 3;
			break;
		}
		if (fileBuf->Riff != MakeFourCC('R', 'I', 'F', 'F') ||
			fileBuf->Mids != MakeFourCC('M', 'I', 'D', 'S') ||
			fileBuf->Fmt != MakeFourCC('f', 'm', 't', ' '))
		{
			returnCode = 3;
			break;
		}
		if (fileBuf->FileSize > fileSize - 8)
		{
			returnCode = 3;
			break;
		}
		if (fileSize - 12 < 8)
		{
			returnCode = 3;
			break;
		}
		if (fileBuf->FmtSize < 12 || fileBuf->FmtSize > fileSize - 12)
		{
			returnCode = 3;
			break;
		}

		auto streamIdUsed = fileBuf->dwFlags == 0;
		auto dataChunk = reinterpret_cast<riff_data*>(reinterpret_cast<char*>(&fileBuf->dwTimeFormat) + fileBuf->
			FmtSize);
		if (dataChunk->Data != MakeFourCC('d', 'a', 't', 'a'))
		{
			returnCode = 3;
			break;
		}
		if (dataChunk->DataSize < 4)
		{
			returnCode = 3;
			break;
		}

		auto srcPtr = dataChunk->Blocks;
		std::vector<midi_event> midiEvents{};
		for (auto blockIndex = dataChunk->BlocksPerChunk; blockIndex; blockIndex--)
		{
			auto eventSizeInt = streamIdUsed ? 3 : 2;
			auto eventCount = srcPtr->CbBuffer / (4 * eventSizeInt);

			auto currentTicks = srcPtr->TkStart;
			auto srcPtr2 = reinterpret_cast<uint32_t*>(srcPtr->AData);
			for (auto i = 0u; i < eventCount; i++)
			{
				currentTicks += srcPtr2[0];
				auto event = streamIdUsed ? srcPtr2[2] : srcPtr2[1];
				midiEvents.push_back({currentTicks, event});
				srcPtr2 += eventSizeInt;
			}

			srcPtr = reinterpret_cast<riff_block*>(&srcPtr->AData[srcPtr->CbBuffer]);
		}

		// MIDS events can be out of order in the file
		std::sort(midiEvents.begin(), midiEvents.end(), [](const midi_event& lhs, const midi_event& rhs)
		{
			return lhs.iTicks < rhs.iTicks;
		});

		// MThd chunk
		std::vector<uint8_t>& midiBytes = *new std::vector<uint8_t>();
		midiOut = &midiBytes;
		midi_header header(SwapByteOrderShort(static_cast<uint16_t>(fileBuf->dwTimeFormat)));
		auto headerData = reinterpret_cast<const uint8_t*>(&header);
		midiBytes.insert(midiBytes.end(), headerData, headerData + sizeof header);

		// MTrk chunk
		midi_track track(7);
		auto trackData = reinterpret_cast<const uint8_t*>(&track);
		midiBytes.insert(midiBytes.end(), trackData, trackData + sizeof track);
		auto lengthPos = midiBytes.size() - 4;

		auto prevTime = 0u;
		for (const auto& event : midiEvents)
		{
			assertm(event.iTicks >= prevTime, "MIDS events: negative delta-time");
			uint32_t delta = event.iTicks - prevTime;
			prevTime = event.iTicks;

			// Delta time is in variable quantity, Big Endian
			uint32_t deltaVarLen;
			auto count = ToVariableLen(delta, deltaVarLen);
			auto deltaData = reinterpret_cast<const uint8_t*>(&deltaVarLen);
			midiBytes.insert(midiBytes.end(), deltaData, deltaData + count);

			switch (event.iEvent >> 24)
			{
			case 0:
				{
					// Type 0 - MIDI short message. 3 bytes: xx p1 p2 00, where xx - message, p* - parameters
					// Some of the messages have only one parameter
					auto msgMask = (event.iEvent) & 0xF0;
					auto shortMsg = (msgMask == 0xC0 || msgMask == 0xD0);
					auto eventData = reinterpret_cast<const uint8_t*>(&event.iEvent);
					midiBytes.insert(midiBytes.end(), eventData, eventData + (shortMsg ? 2 : 3));
					break;
				}
			case 1:
				{
					// Type 1 - tempo change, 3 bytes: xx xx xx 01
					// Meta message, set tempo, 3 bytes payload
					const uint8_t metaSetTempo[] = {0xFF, 0x51, 0x03};
					midiBytes.insert(midiBytes.end(), metaSetTempo, metaSetTempo + 3);

					auto eventBE = SwapByteOrderInt(event.iEvent);
					auto eventData = reinterpret_cast<const uint8_t*>(&eventBE) + 1;
					midiBytes.insert(midiBytes.end(), eventData, eventData + 3);
					break;
				}
			default:
				assertm(0, "MIDS events: uknown event");
				break;
			}
		}

		// Meta message, end of track, 0 bytes payload
		const uint8_t metaEndTrack[] = {0x00, 0xFF, 0x2f, 0x00};
		midiBytes.insert(midiBytes.end(), metaEndTrack, metaEndTrack + 4);

		// Set final MTrk size
		auto lengthBE = SwapByteOrderInt(static_cast<uint32_t>(midiBytes.size()) - sizeof header - sizeof track);
		auto lengthData = reinterpret_cast<const uint8_t*>(&lengthBE);
		std::copy_n(lengthData, 4, midiBytes.begin() + lengthPos);
	}
	while (false);

	delete[] buffer;
	if (returnCode && midiOut)
	{
		delete midiOut;
		midiOut = nullptr;
	}

	return midiOut;
}
