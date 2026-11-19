#include "pch.h"
#include "winmain.h"

#include "control.h"
#include "fullscrn.h"
#include "high_score.h"
#include "midi.h"
#include "options.h"
#include "pb.h"
#include "render.h"
#include "Sound.h"
#include "translations.h"
#include "wintext.h"

constexpr const char* winmain::Version;

HINSTANCE winmain::Instance = nullptr;
HWND winmain::MainWindow = nullptr;

int winmain::return_value = 0;
bool winmain::bQuit = false;
bool winmain::activated = false;
bool winmain::DispFrameRate = false;
bool winmain::single_step = false;
bool winmain::has_focus = true;
int winmain::last_mouse_x;
int winmain::last_mouse_y;
int winmain::mouse_down;
bool winmain::no_time_loss = false;

bool winmain::restart = false;

bool winmain::ShowAboutDialog = false;
bool winmain::ShowExitPopup = false;
bool winmain::LaunchBallEnabled = true;
bool winmain::HighScoresEnabled = true;
bool winmain::DemoActive = false;
double winmain::UpdateToFrameRatio;
double winmain::TargetFrameTime;
optionsStruct& winmain::Options = options::Options;
double winmain::SpinThreshold = 0.005;
WelfordState winmain::SleepState{};
int winmain::CursorIdleCounter = 0;
bool winmain::CursorHidden = false;
POINT winmain::LastCursorPos{-1, -1};
HMENU winmain::MainMenu = nullptr;
double winmain::CounterToMs = 0;

namespace
{
	const wchar_t WindowClassName[] = L"PINTBALL";

	// The ids of the menu items, beside the ids of Menu1.
	enum MenuIds
	{
		LanguageMenuId = 1000,
		SoundVolumeMenuId = 1100,
		SoundChannelsMenuId = 1200,
		MusicVolumeMenuId = 1300,
		UpsMenuId = 1400,
		FpsMenuId = 1500,
		DefaultRatesMenuId = 1600,
		UncappedUpsMenuId,
		PreciseSleepMenuId,
		HideCursorMenuId,
		ResetOptionsMenuId,
		CheatMenuId = 1700,
		BindingMenuId = 1800,
		// The item of the system menu. WM_SYSCOMMAND keeps the 4 low bits for the system.
		SystemShowMenuId = 0x0E00,
	};

	const int VolumeSteps = 10;
	const int SoundChannelValues[] = {1, 2, 4, 8, 16, 24, 32};
	const int UpsValues[] = {60, 90, 120, 180, 240, 300, 360};
	const int FpsValues[] = {60, 75, 90, 100, 120, 144, 180, 240, 300, 360};
	const char* const CheatNames[] = {"hidden test", "1max", "bmax", "gmax", "rmax", "quote", "easy mode"};
	const int CheatHiddenTest = 0, CheatBmax = 2, CheatQuote = 5, CheatEasyMode = 6;

	template <class T, size_t N>
	constexpr int CountOf(const T (&)[N]) { return static_cast<int>(N); }

	int MouseX(LPARAM lParam) { return static_cast<short>(LOWORD(lParam)); }
	int MouseY(LPARAM lParam) { return static_cast<short>(HIWORD(lParam)); }

	void AppendItem(HMENU menu, UINT id, const std::string& text)
	{
		AppendMenuW(menu, MF_STRING, id, wintext::ToMenuText(text).data());
	}

	void AppendItem(HMENU menu, Menu1 id, Msg text)
	{
		AppendItem(menu, static_cast<UINT>(id), pb::get_rc_string(text));
	}

	void AppendPopup(HMENU menu, HMENU popup, const std::string& text)
	{
		AppendMenuW(menu, MF_POPUP, reinterpret_cast<UINT>(popup), wintext::ToMenuText(text).data());
	}

	void AppendSeparator(HMENU menu)
	{
		AppendMenuW(menu, MF_SEPARATOR, 0, nullptr);
	}

	void CheckItem(HMENU menu, UINT id, bool checked)
	{
		CheckMenuItem(menu, id, MF_BYCOMMAND | (checked ? MF_CHECKED : MF_UNCHECKED));
	}

	void CheckItem(HMENU menu, Menu1 id, bool checked)
	{
		CheckItem(menu, static_cast<UINT>(id), checked);
	}

	void EnableItem(HMENU menu, Menu1 id, bool enabled)
	{
		EnableMenuItem(menu, static_cast<UINT>(id), MF_BYCOMMAND | (enabled ? MF_ENABLED : MF_GRAYED));
	}

	int VolumeToStep(int volume)
	{
		return (volume * VolumeSteps + options::MaxVolume / 2) / options::MaxVolume;
	}
}

int winmain::WinMain(HINSTANCE instance, LPCSTR lpCmdLine)
{
	Instance = instance;
	_set_new_handler(NewHandler);
	if (!lpCmdLine)
		lpCmdLine = "";

	pb::quickFlag = strstr(lpCmdLine, "-quick") != nullptr;
	auto noAudio = strstr(lpCmdLine, "-noaudio") != nullptr;

	// Sleep(1) sleeps 1 ms; the performance counter gives the time.
	timeBeginPeriod(1);
	LARGE_INTEGER frequency;
	if (QueryPerformanceFrequency(&frequency) && frequency.QuadPart > 0)
		CounterToMs = 1000.0 / static_cast<double>(frequency.QuadPart);

	WNDCLASSW windowClass{};
	windowClass.style = CS_OWNDC | CS_HREDRAW | CS_VREDRAW;
	windowClass.lpfnWndProc = WindowProc;
	windowClass.hInstance = instance;
	windowClass.hIcon = LoadIconW(instance, L"ICON_1");
	windowClass.hCursor = LoadCursorA(nullptr, IDC_ARROW);
	windowClass.lpszClassName = WindowClassName;
	if (!RegisterClassW(&windowClass))
	{
		pb::ShowMessageBox(MB_ICONHAND, "Could not create window", "RegisterClass failed.");
		timeEndPeriod(1);
		return 1;
	}

	MainWindow = CreateWindowExW(0, WindowClassName, L"PINTBALL", WS_OVERLAPPEDWINDOW,
	                             CW_USEDEFAULT, CW_USEDEFAULT, 800, 556, nullptr, nullptr, instance, nullptr);
	if (!MainWindow)
	{
		pb::ShowMessageBox(MB_ICONHAND, "Could not create window", "CreateWindow failed.");
		timeEndPeriod(1);
		return 1;
	}

	// The system menu can show the menu bar again, when it is hidden.
	auto systemMenu = GetSystemMenu(MainWindow, FALSE);
	if (systemMenu)
	{
		AppendMenuW(systemMenu, MF_SEPARATOR, 0, nullptr);
		AppendMenuW(systemMenu, MF_STRING, SystemShowMenuId, L"Show Menu");
	}

	auto exeFolder = GetExeFolder();
	auto pinballFolder = GetPinballFolder();
	auto iniPath = exeFolder + "PINTBALL.INI";
	auto resetAllOptions = strstr(lpCmdLine, "-reset") != nullptr;
	auto firstStart = true;
	do
	{
		restart = false;

		// First option initialization step: just load settings from .ini.
		options::InitPrimary(iniPath);
		if (resetAllOptions)
		{
			resetAllOptions = false;
			options::ResetAllOptions();
		}
		wintext::SetText(MainWindow, pb::get_rc_string(Msg::STRING139));

		// Data search order: WD, executable path, the Pinball folder of Windows NT.
		std::vector<const char*> searchPaths{"", exeFolder.c_str(), pinballFolder.c_str()};
		pb::SelectDatFile(searchPaths);

		// Second step: run updates that depend on .DAT file selection
		options::InitSecondary();

		if (!Sound::Init(!noAudio, Options.SoundChannels, Options.Sounds, Options.SoundVolume))
			Options.Sounds = false;

		if (!midi::music_init(!noAudio, Options.MusicVolume))
			Options.Music = false;

		if (pb::init())
		{
			std::string message = "The .dat file is missing.\n"
				"Make sure that the game data is present in any of the following locations:\n";
			for (auto path : searchPaths)
			{
				if (path)
				{
					message = message + (path[0] ? path : "working directory") + "\n";
				}
			}
			pb::ShowMessageBox(MB_ICONHAND, "Could not load game data", message.c_str());
			midi::music_shutdown();
			Sound::Close();
			DestroyWindow(MainWindow);
			MainWindow = nullptr;
			timeEndPeriod(1);
			return 1;
		}

		fullscrn::init();

		pb::reset_table();
		pb::firsttime_setup();

		UpdateMenu();

		if (strstr(lpCmdLine, "-fullscreen"))
		{
			Options.FullScreen = true;
		}

		if (!Options.FullScreen)
		{
			// The client area gets the size of the table.
			auto resInfo = &fullscrn::resolution_array[fullscrn::GetResolution()];
			RECT rect{0, 0, resInfo->TableWidth, resInfo->TableHeight};
			AdjustWindowRect(&rect, WS_OVERLAPPEDWINDOW, Options.ShowMenu ? TRUE : FALSE);
			SetWindowPos(MainWindow, nullptr, 0, 0, rect.right - rect.left, rect.bottom - rect.top,
			             SWP_NOMOVE | SWP_NOZORDER | SWP_NOACTIVATE);
		}
		if (firstStart)
		{
			firstStart = false;
			ShowWindow(MainWindow, SW_SHOW);
		}
		fullscrn::set_screen_mode(Options.FullScreen);
		fullscrn::window_size_changed();
		UpdateWindow(MainWindow);

		if (strstr(lpCmdLine, "-demo"))
			pb::toggle_demo();
		else
			pb::replay_level(false);

		MainLoop();

		midi::music_shutdown();
		Sound::Close();
		pb::uninit();
		// After pb::uninit, which puts the high scores in the settings.
		options::uninit();
	}
	while (restart);

	fullscrn::shutdown();
	if (MainMenu)
	{
		SetMenu(MainWindow, nullptr);
		DestroyMenu(MainMenu);
		MainMenu = nullptr;
	}
	DestroyWindow(MainWindow);
	MainWindow = nullptr;
	UnregisterClassW(WindowClassName, instance);
	timeEndPeriod(1);

	return return_value;
}

// Time in milliseconds.
double winmain::Now()
{
	if (CounterToMs > 0)
	{
		LARGE_INTEGER counter;
		QueryPerformanceCounter(&counter);
		return static_cast<double>(counter.QuadPart) * CounterToMs;
	}
	return static_cast<double>(timeGetTime());
}

void winmain::MainLoop()
{
	bQuit = false;
	unsigned updateCounter = 0, frameCounter = 0;
	auto frameStart = Now();
	double UpdateToFrameCounter = 0;
	double sleepRemainder = 0, frameDuration = TargetFrameTime;
	auto prevTime = frameStart;

	while (true)
	{
		if (DispFrameRate)
		{
			auto curTime = Now();
			if (curTime - prevTime > 1000.0)
			{
				char buf[60];
				auto elapsedSec = (curTime - prevTime) * 0.001;
				snprintf(buf, sizeof buf, "Updates/sec = %02.02f Frames/sec = %02.02f ",
				         updateCounter / elapsedSec, frameCounter / elapsedSec);
				SetWindowTextA(MainWindow, buf);
				frameCounter = updateCounter = 0;
				prevTime = curTime;
			}
		}

		if (!ProcessWindowMessages() || bQuit)
			break;

		// The dialogs are modal: the game waits while they are open.
		if (RunDialogs())
		{
			if (bQuit)
				break;
			frameStart = Now();
			sleepRemainder = 0;
			frameDuration = TargetFrameTime;
			no_time_loss = true;
		}

		if (has_focus)
		{
			if (mouse_down)
			{
				POINT point;
				RECT client;
				GetCursorPos(&point);
				ScreenToClient(MainWindow, &point);
				GetClientRect(MainWindow, &client);
				int x = point.x, y = point.y, w = client.right, h = client.bottom;
				if (w > 2 && h > 2)
				{
					float dx = static_cast<float>(last_mouse_x - x) / static_cast<float>(w);
					float dy = static_cast<float>(y - last_mouse_y) / static_cast<float>(h);
					pb::ballset(dx, dy);

					// Original creates continuous mouse movement with mouse capture.
					// Alternative solution: mouse warp at window edges.
					int xMod = 0, yMod = 0;
					if (x <= 0 || x >= w - 1)
						xMod = w - 2;
					if (y <= 0 || y >= h - 1)
						yMod = h - 2;
					if (xMod != 0 || yMod != 0)
					{
						x = abs(x - xMod);
						y = abs(y - yMod);
						POINT warp{x, y};
						ClientToScreen(MainWindow, &warp);
						SetCursorPos(warp.x, warp.y);
					}

					last_mouse_x = x;
					last_mouse_y = y;
				}
			}
			if (!single_step && !no_time_loss)
			{
				auto dt = static_cast<float>(frameDuration);
				pb::frame(dt);
				updateCounter++;
			}
			no_time_loss = false;

			if (UpdateToFrameCounter >= UpdateToFrameRatio)
			{
				auto dc = GetDC(MainWindow);
				if (dc)
				{
					render::PresentVScreen(dc);
					ReleaseDC(MainWindow, dc);
				}
				frameCounter++;
				UpdateToFrameCounter -= UpdateToFrameRatio;
			}

			auto updateEnd = Now();
			auto targetTimeDelta = TargetFrameTime - (updateEnd - frameStart) - sleepRemainder;

			double frameEnd;
			if (targetTimeDelta > 0 && !Options.UncappedUpdatesPerSecond)
			{
				if (Options.HybridSleep)
					HybridSleep(targetTimeDelta);
				else
					Sleep(static_cast<DWORD>(targetTimeDelta));
				frameEnd = Now();
			}
			else
			{
				frameEnd = updateEnd;
			}

			// Limit duration to 2 * target time
			sleepRemainder = Clamp((frameEnd - updateEnd) - targetTimeDelta, -TargetFrameTime, TargetFrameTime);
			frameDuration = std::min(frameEnd - frameStart, 2 * TargetFrameTime);
			frameStart = frameEnd;
			UpdateToFrameCounter++;

			CursorIdleCounter = std::max(CursorIdleCounter - static_cast<int>(frameDuration), 0);
			if (Options.HideCursor && CursorIdleCounter <= 0 && !CursorHidden)
			{
				// Hide the cursor when it is still on the table.
				POINT point;
				RECT client;
				GetCursorPos(&point);
				if (WindowFromPoint(point) == MainWindow)
				{
					ScreenToClient(MainWindow, &point);
					GetClientRect(MainWindow, &client);
					if (PtInRect(&client, point))
					{
						CursorHidden = true;
						SetCursor(nullptr);
					}
				}
			}
		}
	}
}

int winmain::ProcessWindowMessages()
{
	static auto idleWait = 0;
	MSG message;
	if (!has_focus)
	{
		// Progressively wait longer when transitioning to idle
		idleWait = std::min(idleWait + static_cast<int>(TargetFrameTime), 500);
		if (MsgWaitForMultipleObjects(0, nullptr, FALSE, idleWait, QS_ALLINPUT) != WAIT_OBJECT_0)
			return 1;
	}

	idleWait = static_cast<int>(TargetFrameTime);
	while (PeekMessageW(&message, nullptr, 0, 0, PM_REMOVE))
	{
		if (message.message == WM_QUIT)
			return 0;
		TranslateMessage(&message);
		DispatchMessageW(&message);
	}
	return 1;
}

// Shows the dialogs that the game asked for. Returns true when a dialog was shown.
bool winmain::RunDialogs()
{
	auto shown = false;
	if (ShowAboutDialog)
	{
		ShowAboutDialog = false;
		a_dialog();
		shown = true;
	}

	if (high_score::RenderHighScoreDialog())
		shown = true;
	if (options::RenderControlDialog())
		shown = true;

	if (ShowExitPopup)
	{
		ShowExitPopup = false;
		pause(false);
		auto answer = MessageBoxW(MainWindow, L"Exit the game?",
		                          wintext::ToWide(translations::GetTranslation(Msg::Menu1_Exit)).data(),
		                          MB_OKCANCEL | MB_ICONQUESTION);
		if (answer == IDOK)
			RequestQuit();
		else
			end_pause();
		shown = true;
	}
	return shown;
}

LRESULT CALLBACK winmain::WindowProc(HWND window, UINT message, WPARAM wParam, LPARAM lParam)
{
	switch (message)
	{
	case WM_CLOSE:
		RequestQuit();
		return 0;
	case WM_ACTIVATEAPP:
		// Not WM_ACTIVATE: the dialogs of the game do not take the focus from the game.
		if (!wParam)
			FocusLost();
		else if (!IsIconic(window))
			FocusGained();
		return 0;
	case WM_SIZE:
		if (wParam == SIZE_MINIMIZED)
		{
			FocusLost();
		}
		else
		{
			// Restored from the task bar: the activation came while the window was an icon.
			auto foreground = GetForegroundWindow();
			if (!has_focus && foreground && GetWindowThreadProcessId(foreground, nullptr) == GetCurrentThreadId())
				FocusGained();
			fullscrn::window_size_changed();
		}
		return 0;
	case WM_PAINT:
		{
			PAINTSTRUCT paint;
			auto dc = BeginPaint(window, &paint);
			if (dc)
				render::PaintWindow(dc);
			EndPaint(window, &paint);
			return 0;
		}
	case WM_ERASEBKGND:
		return 1;
	case WM_KEYDOWN:
	case WM_SYSKEYDOWN:
	case WM_KEYUP:
	case WM_SYSKEYUP:
		if (OnKey(message, wParam, lParam))
			return 0;
		break;
	case WM_SYSCHAR:
		// The menu has no access keys: only Alt+Space (the system menu) is for
		// Windows. The other Alt keys must not beep.
		if (wParam != ' ')
			return 0;
		break;
	case WM_LBUTTONDOWN:
	case WM_MBUTTONDOWN:
	case WM_RBUTTONDOWN:
	case WM_LBUTTONUP:
	case WM_MBUTTONUP:
	case WM_RBUTTONUP:
		OnMouseButton(message, wParam, lParam);
		return 0;
	case WM_MOUSEMOVE:
		{
			POINT point{MouseX(lParam), MouseY(lParam)};
			if (point.x != LastCursorPos.x || point.y != LastCursorPos.y)
			{
				LastCursorPos = point;
				CursorIdleCounter = 1000;
				if (CursorHidden)
				{
					CursorHidden = false;
					SetCursor(LoadCursorA(nullptr, IDC_ARROW));
				}
			}
			return 0;
		}
	case WM_SETCURSOR:
		if (LOWORD(lParam) == HTCLIENT && CursorHidden)
		{
			SetCursor(nullptr);
			return TRUE;
		}
		break;
	case WM_CAPTURECHANGED:
		if (mouse_down && reinterpret_cast<HWND>(lParam) != window)
		{
			mouse_down = 0;
			ClipCursor(nullptr);
		}
		return 0;
	case WM_COMMAND:
		// Menu items: the high word is 0.
		if (HIWORD(wParam) == 0)
			OnMenuCommand(LOWORD(wParam));
		return 0;
	case WM_SYSCOMMAND:
		switch (wParam & 0xFFF0)
		{
		case SystemShowMenuId:
			options::toggle(Menu1::Show_Menu);
			return 0;
		case SC_SCREENSAVE:
#ifdef SC_MONITORPOWER
		case SC_MONITORPOWER:
#endif
			// No screen saver while the game runs.
			if (has_focus && !single_step)
				return 0;
			break;
		default:
			break;
		}
		break;
	case WM_INITMENU:
		InitMenuStates(reinterpret_cast<HMENU>(wParam));
		return 0;
	case WM_ENTERMENULOOP:
	case WM_EXITMENULOOP:
	case WM_ENTERSIZEMOVE:
	case WM_EXITSIZEMOVE:
		// The game stops in the menu and while the window moves.
		no_time_loss = true;
		return 0;
	case MM_MCINOTIFY:
		midi::OnMciNotify(wParam, lParam);
		return 0;
	default:
		break;
	}
	return DefWindowProcW(window, message, wParam, lParam);
}

// Keyboard input. The input values are virtual-key codes. Returns false when
// Windows must see the key too: Alt+F4, Alt+Space, and Alt or F10 for the menu.
bool winmain::OnKey(UINT message, WPARAM wParam, LPARAM lParam)
{
	auto virtualKey = static_cast<int>(wParam);
	auto isSystemKey = message == WM_SYSKEYDOWN || message == WM_SYSKEYUP;
	auto altDown = (lParam & (1 << 29)) != 0;
	if (isSystemKey && altDown && (virtualKey == VK_F4 || virtualKey == VK_SPACE))
		return false;

	GameInput input{InputTypes::Keyboard, virtualKey};
	auto forGame = !isSystemKey || !options::MapGameInput(input).empty() ||
		(pb::cheat_mode && virtualKey == VK_F10);

	if (message == WM_KEYUP || message == WM_SYSKEYUP)
	{
		pb::InputUp(input);
		return forGame;
	}

	// Bit 30: the key was down before (auto-repeat).
	if (lParam & (1 << 30))
		return forGame;

	pb::InputDown(input);
	if (pb::cheat_mode)
	{
		switch (virtualKey)
		{
		case 'O':
			{
				auto plt = new ColorRgba[4 * 256];
				auto pltPtr = &plt[10]; // first 10 entries are system colors hardcoded in display_palette()
				for (int i1 = 0, i2 = 0; i1 < 256 - 10; ++i1, i2 += 8)
				{
					unsigned char blue = i2, redGreen = i2;
					if (i2 > 255)
					{
						blue = 255;
						redGreen = i1;
					}

					*pltPtr++ = ColorRgba{blue, redGreen, redGreen, 0};
				}
				gdrv::display_palette(plt);
				delete[] plt;
			}
			break;
		case 'Y':
			wintext::SetText(MainWindow, pb::get_rc_string(Msg::STRING139));
			DispFrameRate ^= true;
			break;
		case VK_F1:
			pb::frame(10);
			break;
		case VK_F10:
			single_step ^= true;
			if (!single_step)
				no_time_loss = true;
			break;
		default:
			break;
		}
	}
	return forGame;
}

void winmain::OnMouseButton(UINT message, WPARAM wParam, LPARAM lParam)
{
	CursorIdleCounter = 1000;

	int button;
	switch (message)
	{
	case WM_LBUTTONDOWN:
	case WM_LBUTTONUP:
		button = MouseButtonLeft;
		break;
	case WM_MBUTTONDOWN:
	case WM_MBUTTONUP:
		button = MouseButtonMiddle;
		break;
	default:
		button = MouseButtonRight;
		break;
	}

	if (message == WM_LBUTTONDOWN || message == WM_MBUTTONDOWN || message == WM_RBUTTONDOWN)
	{
		// The button up comes also when the cursor is out of the window.
		SetCapture(MainWindow);
		if (button == MouseButtonLeft && pb::cheat_mode)
		{
			// Move the ball with the mouse; the cursor stays in the window.
			mouse_down = 1;
			last_mouse_x = MouseX(lParam);
			last_mouse_y = MouseY(lParam);
			RECT rect;
			GetClientRect(MainWindow, &rect);
			MapWindowPoints(MainWindow, nullptr, reinterpret_cast<POINT*>(&rect), 2);
			ClipCursor(&rect);
			return;
		}
		pb::InputDown({InputTypes::Mouse, button});
		return;
	}

	auto noInput = false;
	if (button == MouseButtonLeft && mouse_down)
	{
		mouse_down = 0;
		ClipCursor(nullptr);
		noInput = true;
	}
	if (!noInput)
		pb::InputUp({InputTypes::Mouse, button});
	if (!(wParam & (MK_LBUTTON | MK_MBUTTON | MK_RBUTTON)) && GetCapture() == MainWindow)
		ReleaseCapture();
}

void winmain::FocusGained()
{
	if (has_focus)
		return;

	activated = true;
	Sound::Activate();
	if (Options.Music && !single_step)
		midi::music_play();
	no_time_loss = true;
	has_focus = true;
}

void winmain::FocusLost()
{
	if (!has_focus)
		return;

	activated = false;
	fullscrn::activate(0);
	Options.FullScreen = false;
	Sound::Deactivate();
	midi::music_stop();
	has_focus = false;
	pb::loose_focus();

	if (mouse_down)
	{
		mouse_down = 0;
		ClipCursor(nullptr);
	}
	if (GetCapture() == MainWindow)
		ReleaseCapture();
}

void winmain::RequestQuit()
{
	end_pause();
	bQuit = true;
	fullscrn::shutdown();
	return_value = 0;
}

int __cdecl winmain::NewHandler(size_t)
{
	memalloc_failure();
}

void winmain::memalloc_failure()
{
	midi::music_stop();
	Sound::Close();
	const char* caption = pb::get_rc_string(Msg::STRING270);
	const char* text = pb::get_rc_string(Msg::STRING279);
	pb::ShowMessageBox(MB_ICONHAND, caption, text);
	std::exit(1);
}

void winmain::a_dialog()
{
	std::string text = pb::get_rc_string(Msg::STRING139);
	text += "\nOriginal game by Cinematronics, Microsoft\n\n";
	text += "Decompiled -> Ported to SDL -> Ported to Win32 for PINT\n";
	text += "Version ";
	text += Version;
	text += "\nBased on: https://github.com/k4zmu2a/SpaceCadetPinball\n\n";
	text += "Full Tilt! was created by Cinematronics for Maxis.";
	MessageBoxW(MainWindow, wintext::ToWide(text).data(), wintext::ToWide(pb::get_rc_string(Msg::STRING204)).data(),
	            MB_OK | MB_ICONINFORMATION);
}

void winmain::end_pause()
{
	if (single_step)
	{
		pb::pause_continue();
		no_time_loss = true;
	}
}

void winmain::new_game()
{
	end_pause();
	pb::replay_level(false);
}

void winmain::pause(bool toggle)
{
	if (toggle || !single_step)
	{
		pb::pause_continue();
		no_time_loss = true;
	}
}

void winmain::Restart()
{
	restart = true;
	RequestQuit();
}

void winmain::UpdateFrameRate()
{
	// UPS >= FPS
	auto fps = Options.FramesPerSecond.V, ups = Options.UpdatesPerSecond.V;
	UpdateToFrameRatio = static_cast<double>(ups) / fps;
	TargetFrameTime = 1000.0 / ups;
}

void winmain::HandleGameBinding(GameBindings binding, bool shortcut)
{
	switch (binding)
	{
	case GameBindings::TogglePause:
		pause();
		break;
	case GameBindings::NewGame:
		new_game();
		break;
	case GameBindings::ToggleFullScreen:
		options::toggle(Menu1::Full_Screen);
		break;
	case GameBindings::ToggleSounds:
		options::toggle(Menu1::Sounds);
		break;
	case GameBindings::ToggleMusic:
		options::toggle(Menu1::Music);
		break;
	case GameBindings::ShowControlDialog:
		pause(false);
		options::ShowControlDialog();
		break;
	case GameBindings::ToggleMenuDisplay:
		options::toggle(Menu1::Show_Menu);
		break;
	case GameBindings::Exit:
		if (!shortcut)
			RequestQuit();
		else
			ShowExitPopup = true;
		break;
	default:
		break;
	}
}

void winmain::HybridSleep(double sleepTarget)
{
	static constexpr double StdDevFactor = 0.5;

	// This nice concept is from https://blat-blatnik.github.io/computerBear/making-accurate-sleep-function/
	// Sacrifices some CPU time for smaller frame time jitter
	while (sleepTarget > SpinThreshold)
	{
		auto start = Now();
		Sleep(1);
		auto end = Now();

		auto actualDuration = end - start;
		sleepTarget -= actualDuration;

		// Update expected sleep duration using Welford's online algorithm
		// With bad timer, this will run away to 100% spin
		SleepState.Advance(actualDuration);
		SpinThreshold = SleepState.mean + SleepState.GetStdDev() * StdDevFactor;
	}

	// spin lock
	for (auto start = Now(); Now() - start < sleepTarget;);
}

// A menu item of a game binding, with its first key as the shortcut text.
void winmain::AppendBindingItem(HMENU menu, GameBindings binding)
{
	const auto& keyDef = Options.Key[~binding];
	std::string text = pb::get_rc_string(keyDef.Description);
	auto shortcut = keyDef.GetShortcutDescription();
	if (!shortcut.empty())
		text += "\t" + shortcut;
	AppendItem(menu, static_cast<UINT>(BindingMenuId + ~binding), text);
}

// Makes the menu bar again: for the language and the shortcut keys.
void winmain::UpdateMenu()
{
	if (!MainWindow)
		return;

	auto menu = CreateMenu();

	auto gameMenu = CreatePopupMenu();
	AppendBindingItem(gameMenu, GameBindings::NewGame);
	AppendItem(gameMenu, Menu1::Launch_Ball, Msg::Menu1_Launch_Ball);
	AppendBindingItem(gameMenu, GameBindings::TogglePause);
	AppendSeparator(gameMenu);
	AppendItem(gameMenu, Menu1::High_Scores, Msg::Menu1_High_Scores);
	AppendItem(gameMenu, Menu1::Demo, Msg::Menu1_Demo);
	AppendBindingItem(gameMenu, GameBindings::Exit);
	AppendPopup(menu, gameMenu, pb::get_rc_string(Msg::Menu1_Game));

	auto optionsMenu = CreatePopupMenu();
	AppendBindingItem(optionsMenu, GameBindings::ToggleMenuDisplay);
	AppendBindingItem(optionsMenu, GameBindings::ToggleFullScreen);
	auto playersMenu = CreatePopupMenu();
	AppendItem(playersMenu, Menu1::OnePlayer, Msg::Menu1_1Player);
	AppendItem(playersMenu, Menu1::TwoPlayers, Msg::Menu1_2Players);
	AppendItem(playersMenu, Menu1::ThreePlayers, Msg::Menu1_3Players);
	AppendItem(playersMenu, Menu1::FourPlayers, Msg::Menu1_4Players);
	AppendPopup(optionsMenu, playersMenu, pb::get_rc_string(Msg::Menu1_Select_Players));
	AppendBindingItem(optionsMenu, GameBindings::ShowControlDialog);
	auto languageMenu = CreatePopupMenu();
	for (int i = 0; i < static_cast<int>(Lang::Max); i++)
		AppendItem(languageMenu, static_cast<UINT>(LanguageMenuId + i), translations::Languages[i].DisplayName);
	AppendPopup(optionsMenu, languageMenu, "Language");
	AppendSeparator(optionsMenu);

	auto audioMenu = CreatePopupMenu();
	AppendBindingItem(audioMenu, GameBindings::ToggleSounds);
	AppendItem(audioMenu, static_cast<UINT>(Menu1::SoundStereo), "Stereo Sound Effects");
	auto soundVolumeMenu = CreatePopupMenu();
	auto musicVolumeMenu = CreatePopupMenu();
	for (int step = 0; step <= VolumeSteps; step++)
	{
		auto text = std::to_string(step * 100 / VolumeSteps) + "%";
		AppendItem(soundVolumeMenu, static_cast<UINT>(SoundVolumeMenuId + step), text);
		AppendItem(musicVolumeMenu, static_cast<UINT>(MusicVolumeMenuId + step), text);
	}
	AppendPopup(audioMenu, soundVolumeMenu, "Sound Volume");
	auto channelsMenu = CreatePopupMenu();
	for (int i = 0; i < CountOf(SoundChannelValues); i++)
		AppendItem(channelsMenu, static_cast<UINT>(SoundChannelsMenuId + i), std::to_string(SoundChannelValues[i]));
	AppendPopup(audioMenu, channelsMenu, "Sound Channels");
	AppendSeparator(audioMenu);
	AppendBindingItem(audioMenu, GameBindings::ToggleMusic);
	AppendPopup(audioMenu, musicVolumeMenu, "Music Volume");
	AppendPopup(optionsMenu, audioMenu, "Audio");

	auto graphicsMenu = CreatePopupMenu();
	AppendItem(graphicsMenu, Menu1::WindowUniformScale, Msg::Menu1_WindowUniformScale);
	AppendItem(graphicsMenu, static_cast<UINT>(Menu1::WindowIntegerScale), "Integer Scaling");
	AppendSeparator(graphicsMenu);
	AppendItem(graphicsMenu, DefaultRatesMenuId, "Set Default UPS/FPS");
	auto upsMenu = CreatePopupMenu();
	for (int i = 0; i < CountOf(UpsValues); i++)
		AppendItem(upsMenu, static_cast<UINT>(UpsMenuId + i), std::to_string(UpsValues[i]));
	AppendPopup(graphicsMenu, upsMenu, "UPS");
	auto fpsMenu = CreatePopupMenu();
	for (int i = 0; i < CountOf(FpsValues); i++)
		AppendItem(fpsMenu, static_cast<UINT>(FpsMenuId + i), std::to_string(FpsValues[i]));
	AppendPopup(graphicsMenu, fpsMenu, "FPS");
	AppendItem(graphicsMenu, UncappedUpsMenuId, "Uncapped UPS");
	AppendItem(graphicsMenu, PreciseSleepMenuId, "Precise Sleep");
	AppendSeparator(graphicsMenu);
	AppendItem(graphicsMenu, HideCursorMenuId, "Hide Cursor");
	AppendPopup(optionsMenu, graphicsMenu, "Graphics");

	auto resolutionMenu = CreatePopupMenu();
	auto resolutionStringId = Msg::Menu1_UseMaxResolution_640x480;
	switch (fullscrn::GetMaxResolution())
	{
	case 0: resolutionStringId = Msg::Menu1_UseMaxResolution_640x480;
		break;
	case 1: resolutionStringId = Msg::Menu1_UseMaxResolution_800x600;
		break;
	case 2: resolutionStringId = Msg::Menu1_UseMaxResolution_1024x768;
		break;
	default:
		break;
	}
	AppendItem(resolutionMenu, Menu1::MaximumResolution, resolutionStringId);
	for (auto i = 0; i <= fullscrn::GetMaxResolution(); i++)
	{
		char buffer[20];
		auto& res = fullscrn::resolution_array[i];
		snprintf(buffer, sizeof buffer, "%d x %d", res.ScreenWidth, res.ScreenHeight);
		AppendItem(resolutionMenu, static_cast<UINT>(static_cast<int>(Menu1::R640x480) + i), buffer);
	}
	AppendPopup(optionsMenu, resolutionMenu, pb::get_rc_string(Msg::Menu1_Table_Resolution));

	auto gameDataMenu = CreatePopupMenu();
	AppendItem(gameDataMenu, static_cast<UINT>(Menu1::Prefer3DPBGameData), "Prefer 3DPB Data");
	AppendPopup(optionsMenu, gameDataMenu, "Game Data");
	AppendSeparator(optionsMenu);
	AppendItem(optionsMenu, ResetOptionsMenuId, "Reset All Options");
	AppendPopup(menu, optionsMenu, pb::get_rc_string(Msg::Menu1_Options));

	auto helpMenu = CreatePopupMenu();
	auto cheatsMenu = CreatePopupMenu();
	for (int i = 0; i < CountOf(CheatNames); i++)
	{
		if (i != CheatQuote || pb::FullTiltMode)
			AppendItem(cheatsMenu, static_cast<UINT>(CheatMenuId + i), CheatNames[i]);
	}
	AppendPopup(helpMenu, cheatsMenu, "Cheats");
	AppendSeparator(helpMenu);
	AppendItem(helpMenu, Menu1::About_Pinball, Msg::Menu1_About_Pinball);
	AppendPopup(menu, helpMenu, pb::get_rc_string(Msg::Menu1_Help));

	auto systemMenu = GetSystemMenu(MainWindow, FALSE);
	if (systemMenu)
	{
		ModifyMenuW(systemMenu, SystemShowMenuId, MF_BYCOMMAND | MF_STRING, SystemShowMenuId,
		            wintext::ToMenuText(pb::get_rc_string(Msg::Menu1_ToggleShowMenu)).data());
	}

	auto oldMenu = MainMenu;
	MainMenu = menu;
	UpdateMenuVisibility();
	if (oldMenu)
		DestroyMenu(oldMenu);
}

void winmain::UpdateMenuVisibility()
{
	if (!MainWindow)
		return;

	SetMenu(MainWindow, Options.ShowMenu ? MainMenu : nullptr);
	fullscrn::window_size_changed();
}

// The check marks and the disabled items, when the menu opens.
void winmain::InitMenuStates(HMENU menu)
{
	if (menu == GetSystemMenu(MainWindow, FALSE))
	{
		CheckItem(menu, static_cast<UINT>(SystemShowMenuId), Options.ShowMenu);
		return;
	}
	if (!menu || menu != MainMenu)
		return;

	EnableItem(menu, Menu1::Launch_Ball, LaunchBallEnabled);
	EnableItem(menu, Menu1::High_Scores, HighScoresEnabled);
	CheckItem(menu, Menu1::Demo, DemoActive);
	CheckItem(menu, static_cast<UINT>(BindingMenuId + ~GameBindings::ToggleMenuDisplay), Options.ShowMenu);
	CheckItem(menu, static_cast<UINT>(BindingMenuId + ~GameBindings::ToggleFullScreen), Options.FullScreen);
	CheckItem(menu, static_cast<UINT>(BindingMenuId + ~GameBindings::ToggleSounds), Options.Sounds);
	CheckItem(menu, static_cast<UINT>(BindingMenuId + ~GameBindings::ToggleMusic), Options.Music);

	for (int i = 0; i < 4; i++)
		CheckItem(menu, static_cast<UINT>(static_cast<int>(Menu1::OnePlayer) + i), Options.Players == i + 1);

	auto currentLanguage = translations::GetCurrentLanguage();
	for (int i = 0; i < static_cast<int>(Lang::Max); i++)
		CheckItem(menu, static_cast<UINT>(LanguageMenuId + i),
		          currentLanguage->Language == translations::Languages[i].Language);

	CheckItem(menu, Menu1::SoundStereo, Options.SoundStereo);
	auto soundStep = VolumeToStep(Options.SoundVolume), musicStep = VolumeToStep(Options.MusicVolume);
	for (int step = 0; step <= VolumeSteps; step++)
	{
		CheckItem(menu, static_cast<UINT>(SoundVolumeMenuId + step), step == soundStep);
		CheckItem(menu, static_cast<UINT>(MusicVolumeMenuId + step), step == musicStep);
	}
	for (int i = 0; i < CountOf(SoundChannelValues); i++)
		CheckItem(menu, static_cast<UINT>(SoundChannelsMenuId + i), Options.SoundChannels == SoundChannelValues[i]);

	CheckItem(menu, Menu1::WindowUniformScale, Options.UniformScaling);
	CheckItem(menu, Menu1::WindowIntegerScale, Options.IntegerScaling);
	for (int i = 0; i < CountOf(UpsValues); i++)
		CheckItem(menu, static_cast<UINT>(UpsMenuId + i), Options.UpdatesPerSecond == UpsValues[i]);
	for (int i = 0; i < CountOf(FpsValues); i++)
		CheckItem(menu, static_cast<UINT>(FpsMenuId + i), Options.FramesPerSecond == FpsValues[i]);

	char buffer[80];
	snprintf(buffer, sizeof buffer, "Uncapped UPS (FPS ratio %02.02f)", UpdateToFrameRatio);
	ModifyMenuW(menu, UncappedUpsMenuId,
	            MF_BYCOMMAND | MF_STRING | (Options.UncappedUpdatesPerSecond ? MF_CHECKED : MF_UNCHECKED),
	            UncappedUpsMenuId, wintext::ToWide(buffer).data());
	CheckItem(menu, static_cast<UINT>(PreciseSleepMenuId), Options.HybridSleep);
	CheckItem(menu, static_cast<UINT>(HideCursorMenuId), Options.HideCursor);

	CheckItem(menu, Menu1::MaximumResolution, Options.Resolution == -1);
	for (auto i = 0; i <= fullscrn::GetMaxResolution(); i++)
		CheckItem(menu, static_cast<UINT>(static_cast<int>(Menu1::R640x480) + i), Options.Resolution == i);
	CheckItem(menu, Menu1::Prefer3DPBGameData, Options.Prefer3DPBGameData);

	CheckItem(menu, static_cast<UINT>(CheatMenuId + CheatHiddenTest), pb::cheat_mode);
	CheckItem(menu, static_cast<UINT>(CheatMenuId + CheatBmax), control::table_unlimited_balls);
	CheckItem(menu, static_cast<UINT>(CheatMenuId + CheatEasyMode), control::easyMode);
}

void winmain::OnMenuCommand(int id)
{
	if (id >= BindingMenuId && id < BindingMenuId + ~GameBindings::Max)
	{
		HandleGameBinding(static_cast<GameBindings>(id - BindingMenuId), false);
		return;
	}
	if (id >= LanguageMenuId && id < LanguageMenuId + static_cast<int>(Lang::Max))
	{
		auto& item = translations::Languages[id - LanguageMenuId];
		if (translations::GetCurrentLanguage()->Language != item.Language)
		{
			translations::SetCurrentLanguage(item.ShortName);
			Restart();
		}
		return;
	}
	if (id >= SoundVolumeMenuId && id <= SoundVolumeMenuId + VolumeSteps)
	{
		Options.SoundVolume = (id - SoundVolumeMenuId) * options::MaxVolume / VolumeSteps;
		Sound::SetVolume(Options.SoundVolume);
		return;
	}
	if (id >= MusicVolumeMenuId && id <= MusicVolumeMenuId + VolumeSteps)
	{
		Options.MusicVolume = (id - MusicVolumeMenuId) * options::MaxVolume / VolumeSteps;
		midi::SetVolume(Options.MusicVolume);
		return;
	}
	if (id >= SoundChannelsMenuId && id < SoundChannelsMenuId + CountOf(SoundChannelValues))
	{
		Options.SoundChannels = SoundChannelValues[id - SoundChannelsMenuId];
		Sound::SetChannels(Options.SoundChannels);
		return;
	}
	if (id >= UpsMenuId && id < UpsMenuId + CountOf(UpsValues))
	{
		Options.UpdatesPerSecond = UpsValues[id - UpsMenuId];
		Options.FramesPerSecond = std::min(Options.UpdatesPerSecond.V, Options.FramesPerSecond.V);
		UpdateFrameRate();
		return;
	}
	if (id >= FpsMenuId && id < FpsMenuId + CountOf(FpsValues))
	{
		Options.FramesPerSecond = FpsValues[id - FpsMenuId];
		Options.UpdatesPerSecond = std::max(Options.UpdatesPerSecond.V, Options.FramesPerSecond.V);
		UpdateFrameRate();
		return;
	}
	if (id >= CheatMenuId && id < CheatMenuId + CountOf(CheatNames))
	{
		pb::PushCheat(CheatNames[id - CheatMenuId]);
		return;
	}

	switch (id)
	{
	case DefaultRatesMenuId:
		Options.UpdatesPerSecond = options::DefUps;
		Options.FramesPerSecond = options::DefFps;
		UpdateFrameRate();
		return;
	case UncappedUpsMenuId:
		Options.UncappedUpdatesPerSecond ^= true;
		return;
	case PreciseSleepMenuId:
		Options.HybridSleep ^= true;
		SleepState = WelfordState{};
		SpinThreshold = 0;
		return;
	case HideCursorMenuId:
		Options.HideCursor ^= true;
		if (!Options.HideCursor && CursorHidden)
		{
			CursorHidden = false;
			SetCursor(LoadCursorA(nullptr, IDC_ARROW));
		}
		return;
	case ResetOptionsMenuId:
		options::ResetAllOptions();
		Restart();
		return;
	default:
		break;
	}

	switch (static_cast<Menu1>(id))
	{
	case Menu1::Launch_Ball:
		end_pause();
		pb::launch_ball();
		break;
	case Menu1::High_Scores:
		pause(false);
		pb::high_scores();
		break;
	case Menu1::Demo:
		end_pause();
		pb::toggle_demo();
		break;
	case Menu1::OnePlayer:
	case Menu1::TwoPlayers:
	case Menu1::ThreePlayers:
	case Menu1::FourPlayers:
		options::toggle(static_cast<Menu1>(id));
		new_game();
		break;
	case Menu1::SoundStereo:
	case Menu1::WindowUniformScale:
	case Menu1::WindowIntegerScale:
	case Menu1::Prefer3DPBGameData:
	case Menu1::MaximumResolution:
	case Menu1::R640x480:
	case Menu1::R800x600:
	case Menu1::R1024x768:
		options::toggle(static_cast<Menu1>(id));
		break;
	case Menu1::About_Pinball:
		pause(false);
		ShowAboutDialog = true;
		break;
	default:
		break;
	}
}

std::string winmain::GetExeFolder()
{
	char path[MAX_PATH];
	auto length = GetModuleFileNameA(nullptr, path, MAX_PATH);
	if (length == 0 || length >= MAX_PATH)
		return {};

	std::string folder(path, length);
	auto separator = folder.rfind('\\');
	if (separator == std::string::npos)
		return {};
	return folder.substr(0, separator + 1);
}

// The Pinball folder of Windows NT 4.0: Program Files\Windows NT\Pinball.
std::string winmain::GetPinballFolder()
{
	char subKey[] = "SOFTWARE\\Microsoft\\Windows\\CurrentVersion";
	char valueName[] = "ProgramFilesDir";
	char folder[MAX_PATH]{};
	HKEY key;
	if (RegOpenKeyExA(HKEY_LOCAL_MACHINE, subKey, 0, KEY_READ, &key) == ERROR_SUCCESS)
	{
		char value[MAX_PATH]{};
		DWORD type = 0, size = sizeof value - 1;
		if (RegQueryValueExA(key, valueName, nullptr, &type, reinterpret_cast<LPBYTE>(value), &size) == ERROR_SUCCESS)
		{
			if (type == REG_SZ)
				strcpy(folder, value);
			else if (type == REG_EXPAND_SZ)
			{
				auto length = ExpandEnvironmentStringsA(value, folder, sizeof folder);
				if (length == 0 || length > sizeof folder)
					folder[0] = 0;
			}
		}
		RegCloseKey(key);
	}

	std::string result = folder;
	if (result.empty())
	{
		char drive[MAX_PATH];
		auto length = GetEnvironmentVariableA("SystemDrive", drive, sizeof drive);
		result = length > 0 && length < sizeof drive ? drive : "C:";
		result += "\\Program Files";
	}
	if (result.back() != '\\')
		result += '\\';
	return result + "Windows NT\\Pinball\\";
}
