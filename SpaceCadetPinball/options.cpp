#include "pch.h"
#include "options.h"

#include "fullscrn.h"
#include "midi.h"
#include "pb.h"
#include "render.h"
#include "resource.h"
#include "Sound.h"
#include "winmain.h"
#include "wintext.h"
#include "translations.h"

constexpr int options::MaxUps, options::MaxFps, options::MinUps, options::MinFps, options::DefUps, options::DefFps;
constexpr int options::MaxSoundChannels, options::MinSoundChannels, options::DefSoundChannels;
constexpr int options::MaxVolume, options::MinVolume, options::DefVolume;

std::vector<std::pair<std::string, std::string>> options::settings{};
std::string options::IniPath{};
bool options::ShowDialog = false;
int options::WaitingRow = -1, options::WaitingColumn = -1, options::SwallowKey = 0;
bool options::SwallowMouseUp = false;
WNDPROC options::ButtonProc = nullptr;
std::vector<OptionBase*> options::AllOptions{};

optionsStruct options::Options
{
	{
		{
			"Left Flipper key",
			Msg::KEYMAPPER_FlipperL,
			{InputTypes::Keyboard, 'Z'},
			{InputTypes::Mouse, MouseButtonLeft},
			{}
		},
		{
			"Right Flipper key",
			Msg::KEYMAPPER_FlipperR,
			{InputTypes::Keyboard, VK_OEM_2},
			{InputTypes::Mouse, MouseButtonRight},
			{}
		},
		{
			"Plunger key",
			Msg::KEYMAPPER_Plunger,
			{InputTypes::Keyboard, VK_SPACE},
			{InputTypes::Mouse, MouseButtonMiddle},
			{}
		},
		{
			"Left Table Bump key",
			Msg::KEYMAPPER_BumpLeft,
			{InputTypes::Keyboard, 'X'},
			{},
			{}
		},
		{
			"Right Table Bump key",
			Msg::KEYMAPPER_BumpRight,
			{InputTypes::Keyboard, VK_OEM_PERIOD},
			{},
			{}
		},
		{
			"Bottom Table Bump key",
			Msg::KEYMAPPER_BumpBottom,
			{InputTypes::Keyboard, VK_UP},
			{},
			{}
		},
		{
			"New Game",
			Msg::Menu1_New_Game,
			{InputTypes::Keyboard, VK_F2},
			{},
			{}
		},
		{
			"Toggle Pause",
			Msg::Menu1_Pause_Resume_Game,
			{InputTypes::Keyboard, VK_F3},
			{},
			{}
		},
		{
			"Toggle FullScreen",
			Msg::Menu1_Full_Screen,
			{InputTypes::Keyboard, VK_F4},
			{},
			{}
		},
		{
			"Toggle Sounds",
			Msg::Menu1_Sounds,
			{InputTypes::Keyboard, VK_F5},
			{},
			{}
		},
		{
			"Toggle Music",
			Msg::Menu1_Music,
			{InputTypes::Keyboard, VK_F6},
			{},
			{}
		},
		{
			"Show Control Dialog",
			Msg::Menu1_Player_Controls,
			{InputTypes::Keyboard, VK_F8},
			{},
			{}
		},
		{
			"Toggle Menu Display",
			Msg::Menu1_ToggleShowMenu,
			{InputTypes::Keyboard, VK_F9},
			{},
			{}
		},
		{
			"Exit",
			Msg::Menu1_Exit,
			{InputTypes::Keyboard, VK_ESCAPE},
			{},
			{}
		},
	},
	{"Sounds", true},
	{"Music", false},
	{"FullScreen", false},
	{"Players", 1},
	{"Screen Resolution", -1},
	{"Uniform scaling", true},
	{"Frames Per Second", DefFps},
	{"Updates Per Second", DefUps},
	{"ShowMenu", true},
	{"Uncapped Updates Per Second", false},
	{"Sound Channels", DefSoundChannels},
	{"HybridSleep", false},
	{"Prefer 3DPB Game Data", false},
	{"Integer Scaling", false},
	{"Sound Volume", DefVolume},
	{"Music Volume", DefVolume},
	{"Stereo Sound Effects", false},
	{"Language", translations::GetCurrentLanguage()->ShortName},
	{"Hide Cursor", false},
};

// The rows of the player controls dialog, in dialog units. The static
// controls above them are in IDD_CONTROLS of SpaceCadetPinball.rc.
static constexpr int ControlRowTop = 71, ControlRowStep = 12, ControlRowHeight = 11;
static constexpr int ControlNameX = 8, ControlNameWidth = 92;
static constexpr int ControlInputX = 104, ControlInputStep = 74, ControlInputWidth = 70;
static constexpr int ControlCount = static_cast<int>(GameBindings::Max);

void options::InitPrimary(const std::string& iniPath)
{
	IniPath = iniPath;
	LoadSettings();

	for (const auto opt : AllOptions)
		opt->Load();
	PostProcessOptions();
}

void options::InitSecondary()
{
	auto maxRes = fullscrn::GetMaxResolution();
	if (Options.Resolution >= 0 && Options.Resolution > maxRes)
		Options.Resolution = maxRes;
	fullscrn::SetResolution(Options.Resolution == -1 ? maxRes : Options.Resolution);
}

// Saves the options in the settings, then writes the INI file. Call it after
// pb::uninit, which writes the high scores to the settings.
void options::uninit()
{
	Options.Language.V = translations::GetCurrentLanguage()->ShortName;
	for (const auto opt : AllOptions)
	{
		opt->Save();
	}
	SaveSettings();
}

void options::LoadSettings()
{
	settings.clear();
	if (IniPath.empty())
		return;

	// The section comes as "key=value" texts that end with a zero, and one
	// more zero at the end.
	std::vector<char> buffer;
	for (DWORD size = 16384; ; size *= 2)
	{
		buffer.clear();
		buffer.resize(size);
		auto length = GetPrivateProfileSectionA("Settings", buffer.data(), size, IniPath.c_str());
		if (length < size - 2 || size >= 1024 * 1024)
			break;
	}

	for (auto entry = buffer.data(); *entry; entry += strlen(entry) + 1)
	{
		auto separator = strchr(entry, '=');
		if (separator && separator != entry)
			settings.push_back({std::string(entry, separator), std::string(separator + 1)});
	}
}

void options::SaveSettings()
{
	if (IniPath.empty())
		return;

	std::string section;
	for (const auto& setting : settings)
	{
		section += setting.first;
		section += '=';
		section += setting.second;
		section += '\0';
	}
	section += '\0';
	WritePrivateProfileSectionA("Settings", section.c_str(), IniPath.c_str());
}

int options::get_int(LPCSTR lpValueName, int defaultValue)
{
	auto value = GetSetting(lpValueName, std::to_string(defaultValue));
	return std::stoi(value);
}

void options::set_int(LPCSTR lpValueName, int data)
{
	SetSetting(lpValueName, std::to_string(data));
}

float options::get_float(LPCSTR lpValueName, float defaultValue)
{
	auto value = GetSetting(lpValueName, std::to_string(defaultValue));
	return std::stof(value);
}

void options::set_float(LPCSTR lpValueName, float data)
{
	SetSetting(lpValueName, std::to_string(data));
}

void options::GetInput(const std::string& rowName, GameInput (&values)[3])
{
	for (auto i = 0u; i <= 2; i++)
	{
		auto name = rowName + " " + std::to_string(i);
		auto inputType = static_cast<InputTypes>(get_int((name + " type").c_str(), -1));
		auto input = get_int((name + " input").c_str(), -1);
		if (inputType <= InputTypes::GameController && input != -1)
			values[i] = {inputType, input};
	}
}

void options::SetInput(const std::string& rowName, GameInput (&values)[3])
{
	for (auto i = 0u; i <= 2; i++)
	{
		auto input = values[i];
		auto name = rowName + " " + std::to_string(i);
		set_int((name + " type").c_str(), static_cast<int>(input.Type));
		set_int((name + " input").c_str(), input.Value);
	}
}

void options::toggle(Menu1 uIDCheckItem)
{
	switch (uIDCheckItem)
	{
	case Menu1::Sounds:
		Options.Sounds ^= true;
		Sound::Enable(Options.Sounds);
		return;
	case Menu1::SoundStereo:
		Options.SoundStereo ^= true;
		return;
	case Menu1::Music:
		Options.Music ^= true;
		if (!Options.Music)
			midi::music_stop();
		else
			midi::music_play();
		return;
	case Menu1::Show_Menu:
		Options.ShowMenu ^= true;
		winmain::UpdateMenuVisibility();
		return;
	case Menu1::Full_Screen:
		Options.FullScreen ^= true;
		fullscrn::set_screen_mode(Options.FullScreen);
		return;
	case Menu1::OnePlayer:
	case Menu1::TwoPlayers:
	case Menu1::ThreePlayers:
	case Menu1::FourPlayers:
		Options.Players = static_cast<int>(uIDCheckItem) - static_cast<int>(Menu1::OnePlayer) + 1;
		break;
	case Menu1::MaximumResolution:
	case Menu1::R640x480:
	case Menu1::R800x600:
	case Menu1::R1024x768:
		{
			auto restart = false;
			int newResolution = static_cast<int>(uIDCheckItem) - static_cast<int>(Menu1::R640x480);
			if (uIDCheckItem == Menu1::MaximumResolution)
			{
				restart = fullscrn::GetResolution() != fullscrn::GetMaxResolution();
				Options.Resolution = -1;
			}
			else if (newResolution <= fullscrn::GetMaxResolution())
			{
				restart = newResolution != (Options.Resolution == -1
					                            ? fullscrn::GetMaxResolution()
					                            : fullscrn::GetResolution());
				Options.Resolution = newResolution;
			}

			if (restart)
				winmain::Restart();
			break;
		}
	case Menu1::WindowUniformScale:
		Options.UniformScaling ^= true;
		fullscrn::window_size_changed();
		break;
	case Menu1::Prefer3DPBGameData:
		Options.Prefer3DPBGameData ^= true;
		winmain::Restart();
		break;
	case Menu1::WindowIntegerScale:
		Options.IntegerScaling ^= true;
		fullscrn::window_size_changed();
		break;
	default:
		break;
	}
}

// The controls dialog reads its keys itself; the game window does not wait
// for input.
void options::InputDown(GameInput input)
{
}

void options::ShowControlDialog()
{
	ShowDialog = true;
}

// Shows the player controls dialog, if ShowControlDialog asked for it.
// Returns true when the dialog was shown.
bool options::RenderControlDialog()
{
	if (!ShowDialog)
		return false;
	ShowDialog = false;

	// Save the current controls in the settings: Cancel loads them again.
	for (const auto& control : Options.Key)
	{
		control.Save();
	}

	WaitingRow = WaitingColumn = -1;
	SwallowKey = 0;
	SwallowMouseUp = false;
	DialogBoxParamW(winmain::Instance, MAKEINTRESOURCEW(IDD_CONTROLS), winmain::MainWindow, ControlDialogProc, 0);
	WaitingRow = WaitingColumn = -1;

	// The menu shows the shortcut keys.
	winmain::UpdateMenu();
	return true;
}

void options::UpdateControlTexts(HWND dialog)
{
	for (int row = 0; row < ControlCount; row++)
	{
		const auto& option = Options.Key[row];
		wintext::SetItemText(dialog, IDC_CTL_ROW0 + row, pb::get_rc_string(option.Description));
		for (int column = 0; column < 3; column++)
		{
			auto id = IDC_CTL_INPUT0 + row * 3 + column;
			if (row == WaitingRow && column == WaitingColumn)
				wintext::SetItemText(dialog, id, "Press the key");
			else
				wintext::SetItemText(dialog, id, option.Inputs[column].GetFullInputDescription().c_str());
		}
	}
}

// Stops the wait for a key. With an input, the input goes into the binding.
void options::EndWaiting(HWND dialog, const GameInput* input)
{
	if (WaitingRow < 0)
		return;

	auto row = WaitingRow, column = WaitingColumn;
	WaitingRow = WaitingColumn = -1;
	if (input)
		Options.Key[row].Inputs[column] = *input;
	UpdateControlTexts(dialog);
}

BOOL CALLBACK options::ControlDialogProc(HWND dialog, UINT message, WPARAM wParam, LPARAM lParam)
{
	switch (message)
	{
	case WM_INITDIALOG:
		{
			wintext::SetText(dialog, pb::get_rc_string(Msg::KEYMAPPER_Caption));
			std::string help = pb::get_rc_string(Msg::KEYMAPPER_Help1);
			help += "\n";
			help += pb::get_rc_string(Msg::KEYMAPPER_Help2);
			wintext::SetItemText(dialog, IDC_CTL_HELP, help.c_str());
			wintext::SetItemText(dialog, IDC_CTL_GROUP, pb::get_rc_string(Msg::KEYMAPPER_Groupbox1));
			wintext::SetItemText(dialog, IDOK, pb::get_rc_string(Msg::GenericOk));
			wintext::SetItemText(dialog, IDCANCEL, pb::get_rc_string(Msg::GenericCancel));
			wintext::SetItemText(dialog, IDC_CTL_DEFAULT, pb::get_rc_string(Msg::KEYMAPPER_Default));

			// One row for each control: a button with the name of the control,
			// which clears the row, and three buttons for the bindings.
			auto font = reinterpret_cast<HFONT>(SendMessageW(dialog, WM_GETFONT, 0, 0));
			for (int row = 0; row < ControlCount; row++)
			{
				for (int column = -1; column < 3; column++)
				{
					RECT rect;
					rect.left = column < 0 ? ControlNameX : ControlInputX + column * ControlInputStep;
					rect.top = ControlRowTop + row * ControlRowStep;
					rect.right = rect.left + (column < 0 ? ControlNameWidth : ControlInputWidth);
					rect.bottom = rect.top + ControlRowHeight;
					MapDialogRect(dialog, &rect);

					auto id = column < 0 ? IDC_CTL_ROW0 + row : IDC_CTL_INPUT0 + row * 3 + column;
					auto button = CreateWindowExW(0, L"BUTTON", L"", WS_CHILD | WS_VISIBLE | WS_TABSTOP | BS_PUSHBUTTON,
					                              rect.left, rect.top, rect.right - rect.left, rect.bottom - rect.top,
					                              dialog, reinterpret_cast<HMENU>(static_cast<size_t>(id)),
					                              winmain::Instance, nullptr);
					if (!button)
						continue;
					SendMessageW(button, WM_SETFONT, reinterpret_cast<WPARAM>(font), FALSE);
					if (column >= 0)
					{
						auto oldProc = reinterpret_cast<WNDPROC>(SetWindowLongW(
							button, GWL_WNDPROC, reinterpret_cast<LONG>(BindingButtonProc)));
						if (!ButtonProc)
							ButtonProc = oldProc;
					}
				}
			}

			// OK, Cancel and Default come after the rows in the tab order.
			for (auto id : {IDOK, IDCANCEL, IDC_CTL_DEFAULT})
				SetWindowPos(GetDlgItem(dialog, id), HWND_BOTTOM, 0, 0, 0, 0, SWP_NOMOVE | SWP_NOSIZE | SWP_NOACTIVATE);

			UpdateControlTexts(dialog);
			return TRUE;
		}
	case WM_COMMAND:
		{
			if (HIWORD(wParam) != BN_CLICKED)
				break;

			int id = LOWORD(wParam);
			if (id >= IDC_CTL_INPUT0 && id < IDC_CTL_INPUT0 + ControlCount * 3)
			{
				// Wait for a key, or a mouse button on this button.
				EndWaiting(dialog, nullptr);
				WaitingRow = (id - IDC_CTL_INPUT0) / 3;
				WaitingColumn = (id - IDC_CTL_INPUT0) % 3;
				SetFocus(GetDlgItem(dialog, id));
				UpdateControlTexts(dialog);
				return TRUE;
			}
			if (id >= IDC_CTL_ROW0 && id < IDC_CTL_ROW0 + ControlCount)
			{
				EndWaiting(dialog, nullptr);
				for (auto& input : Options.Key[id - IDC_CTL_ROW0].Inputs)
					input = {};
				UpdateControlTexts(dialog);
				return TRUE;
			}

			switch (id)
			{
			case IDOK:
				EndWaiting(dialog, nullptr);
				EndDialog(dialog, IDOK);
				return TRUE;
			case IDCANCEL:
				EndWaiting(dialog, nullptr);
				for (auto& control : Options.Key)
				{
					control.Load();
				}
				EndDialog(dialog, IDCANCEL);
				return TRUE;
			case IDC_CTL_DEFAULT:
				EndWaiting(dialog, nullptr);
				for (auto& control : Options.Key)
				{
					control.Reset();
				}
				UpdateControlTexts(dialog);
				return TRUE;
			default:
				break;
			}
			break;
		}
	default:
		break;
	}
	return FALSE;
}

// The binding buttons. While a button waits, it takes all keys (also Tab,
// Enter and Esc) and the mouse buttons, and the key goes into the binding.
LRESULT CALLBACK options::BindingButtonProc(HWND button, UINT message, WPARAM wParam, LPARAM lParam)
{
	auto index = GetDlgCtrlID(button) - IDC_CTL_INPUT0;
	auto waiting = WaitingRow >= 0 && index == WaitingRow * 3 + WaitingColumn;
	switch (message)
	{
	case WM_GETDLGCODE:
		if (waiting || SwallowKey)
			return DLGC_WANTALLKEYS;
		break;
	case WM_KEYDOWN:
	case WM_SYSKEYDOWN:
		if (waiting)
		{
			// Bit 30: the key was down before (auto-repeat).
			if (!(lParam & (1 << 30)))
			{
				GameInput input{InputTypes::Keyboard, static_cast<int>(wParam)};
				SwallowKey = static_cast<int>(wParam);
				EndWaiting(GetParent(button), &input);
			}
			return 0;
		}
		break;
	case WM_CHAR:
	case WM_SYSCHAR:
	case WM_DEADCHAR:
	case WM_SYSDEADCHAR:
		if (waiting || SwallowKey)
			return 0;
		break;
	case WM_KEYUP:
	case WM_SYSKEYUP:
		// The key up of a binding must not click the button or open the menu.
		if (SwallowKey && static_cast<int>(wParam) == SwallowKey)
		{
			SwallowKey = 0;
			return 0;
		}
		if (waiting)
			return 0;
		break;
	case WM_LBUTTONDOWN:
	case WM_LBUTTONDBLCLK:
	case WM_MBUTTONDOWN:
	case WM_MBUTTONDBLCLK:
	case WM_RBUTTONDOWN:
	case WM_RBUTTONDBLCLK:
		if (waiting)
		{
			auto value = MouseButtonLeft;
			if (message == WM_MBUTTONDOWN || message == WM_MBUTTONDBLCLK)
				value = MouseButtonMiddle;
			else if (message == WM_RBUTTONDOWN || message == WM_RBUTTONDBLCLK)
				value = MouseButtonRight;
			GameInput input{InputTypes::Mouse, value};
			SwallowMouseUp = true;
			EndWaiting(GetParent(button), &input);
			return 0;
		}
		break;
	case WM_LBUTTONUP:
	case WM_MBUTTONUP:
	case WM_RBUTTONUP:
		if (SwallowMouseUp)
		{
			SwallowMouseUp = false;
			return 0;
		}
		break;
	case WM_KILLFOCUS:
		SwallowKey = 0;
		SwallowMouseUp = false;
		if (waiting)
			EndWaiting(GetParent(button), nullptr);
		break;
	default:
		break;
	}
	return CallWindowProcW(ButtonProc, button, message, wParam, lParam);
}

std::vector<GameBindings> options::MapGameInput(GameInput key)
{
	std::vector<GameBindings> result;
	for (auto inputId = GameBindings::Min; inputId < GameBindings::Max; inputId++)
	{
		for (auto& inputValue : Options.Key[~inputId].Inputs)
		{
			if (key == inputValue)
			{
				result.push_back(inputId);
				break;
			}
		}
	}
	return result;
}

void options::ResetAllOptions()
{
	for (const auto opt : AllOptions)
		opt->Reset();
	PostProcessOptions();
}

void options::PostProcessOptions()
{
	Options.FramesPerSecond = Clamp(Options.FramesPerSecond.V, MinFps, MaxFps);
	Options.UpdatesPerSecond = Clamp(Options.UpdatesPerSecond.V, MinUps, MaxUps);
	Options.UpdatesPerSecond = std::max(Options.UpdatesPerSecond.V, Options.FramesPerSecond.V);
	Options.SoundChannels = Clamp(Options.SoundChannels.V, MinSoundChannels, MaxSoundChannels);
	Options.SoundVolume = Clamp(Options.SoundVolume.V, MinVolume, MaxVolume);
	Options.MusicVolume = Clamp(Options.MusicVolume.V, MinVolume, MaxVolume);
	translations::SetCurrentLanguage(Options.Language.V.c_str());
	winmain::UpdateFrameRate();
}

// The name of a key, in the language of the keyboard layout.
static std::string GetKeyName(int virtualKey)
{
	auto scanCode = MapVirtualKeyW(static_cast<UINT>(virtualKey), 0);
	auto keyData = static_cast<LONG>(scanCode << 16);
	switch (virtualKey)
	{
	// These keys share their scan codes with the numeric keypad; the
	// extended bit gives their own names.
	case VK_PRIOR:
	case VK_NEXT:
	case VK_END:
	case VK_HOME:
	case VK_LEFT:
	case VK_UP:
	case VK_RIGHT:
	case VK_DOWN:
	case VK_INSERT:
	case VK_DELETE:
	case VK_DIVIDE:
	case VK_NUMLOCK:
	case VK_RCONTROL:
	case VK_RMENU:
	case VK_LWIN:
	case VK_RWIN:
	case VK_APPS:
		keyData |= 1 << 24;
		break;
	default:
		break;
	}

	wchar_t name[64];
	if (scanCode && GetKeyNameTextW(keyData, name, 64) > 0)
		return wintext::ToUtf8(name);

	char buffer[16];
	snprintf(buffer, sizeof buffer, "Key 0x%02X", virtualKey);
	return buffer;
}

// One line, for the buttons of the controls dialog.
std::string GameInput::GetFullInputDescription() const
{
	switch (Type)
	{
	case InputTypes::Keyboard:
		return GetShortInputDescription();
	case InputTypes::Mouse:
		return "Mouse " + GetShortInputDescription();
	case InputTypes::GameController:
		return "Controller " + GetShortInputDescription();
	case InputTypes::None:
	default:
		return "Unused";
	}
}

std::string GameInput::GetShortInputDescription() const
{
	static LPCSTR mouseButtons[]
	{
		nullptr,
		"Left",
		"Middle",
		"Right",
	};

	std::string keyName;
	switch (Type)
	{
	case InputTypes::Keyboard:
		keyName = GetKeyName(Value);
		break;
	case InputTypes::Mouse:
		if (Value >= MouseButtonLeft && Value <= MouseButtonRight)
			keyName = mouseButtons[Value];
		else
			keyName = "MButton" + std::to_string(Value);
		break;
	case InputTypes::GameController:
		keyName = "CButton" + std::to_string(Value);
		break;
	case InputTypes::None:
	default:
		break;
	}

	return keyName;
}

std::string options::GetSetting(const std::string& key, const std::string& defaultValue)
{
	for (const auto& setting : settings)
	{
		if (setting.first == key)
			return setting.second;
	}
	settings.push_back({key, defaultValue});
	return defaultValue;
}

void options::SetSetting(const std::string& key, const std::string& value)
{
	for (auto& setting : settings)
	{
		if (setting.first == key)
		{
			setting.second = value;
			return;
		}
	}
	settings.push_back({key, value});
}

OptionBase::OptionBase(LPCSTR name): Name(name)
{
	options::AllOptions.push_back(this);
}

OptionBase::~OptionBase()
{
	auto& vec = options::AllOptions;
	auto position = std::find(vec.begin(), vec.end(), this);
	if (position != vec.end())
		vec.erase(position);
}

std::string ControlOption::GetShortcutDescription() const
{
	std::string result;
	for (const auto& input : Inputs)
	{
		if (input.Type != InputTypes::None)
		{
			result = input.GetShortInputDescription();
			break;
		}
	}
	return result;
}
