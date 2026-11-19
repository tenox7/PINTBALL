#pragma once
#include "gdrv.h"

enum class GameBindings;

struct WelfordState
{
	double mean;
	double M2;
	int64_t count;

	WelfordState() : mean(0.005), M2(0), count(1)
	{
	}

	void Advance(double newValue)
	{
		++count;
		auto delta = newValue - mean;
		mean += delta / count;
		M2 += delta * (newValue - mean); //M2n = M2n-1 + (Xn - AvgXn-1) * (Xn - AvgXn)
	}

	double GetStdDev() const
	{
		return std::sqrt(M2 / (count - 1)); // Sn^2 = M2n / (n - 1)
	}
};

// The main window, its menu and the main loop, with plain Win32.
class winmain
{
public:
	static constexpr const char* Version = "2.1.1 PINT";
	static bool single_step;
	static HINSTANCE Instance;
	static HWND MainWindow;
	static bool LaunchBallEnabled;
	static bool HighScoresEnabled;
	static bool DemoActive;

	static int WinMain(HINSTANCE instance, LPCSTR lpCmdLine);
	[[ noreturn ]] static void memalloc_failure();
	static int ProcessWindowMessages();
	static void a_dialog();
	static void end_pause();
	static void new_game();
	static void pause(bool toggle = true);
	static void Restart();
	static void UpdateFrameRate();
	static void HandleGameBinding(GameBindings binding, bool shortcut);
	static void UpdateMenu();
	static void UpdateMenuVisibility();
private:
	static int return_value;
	static int mouse_down, last_mouse_x, last_mouse_y;
	static bool no_time_loss, activated, bQuit, has_focus, DispFrameRate;
	static bool restart;
	static bool ShowAboutDialog;
	static bool ShowExitPopup;
	static double UpdateToFrameRatio;
	static double TargetFrameTime; // milliseconds
	static struct optionsStruct& Options;
	static double SpinThreshold; // milliseconds
	static WelfordState SleepState;
	static int CursorIdleCounter;
	static bool CursorHidden;
	static POINT LastCursorPos;
	static HMENU MainMenu;
	static double CounterToMs;

	static LRESULT CALLBACK WindowProc(HWND window, UINT message, WPARAM wParam, LPARAM lParam);
	static int __cdecl NewHandler(size_t size);
	static double Now();
	static void HybridSleep(double sleepTarget);
	static void MainLoop();
	static bool RunDialogs();
	static void RequestQuit();
	static void FocusGained();
	static void FocusLost();
	static bool OnKey(UINT message, WPARAM wParam, LPARAM lParam);
	static void OnMouseButton(UINT message, WPARAM wParam, LPARAM lParam);
	static void OnMenuCommand(int id);
	static void InitMenuStates(HMENU menu);
	static void AppendBindingItem(HMENU menu, GameBindings binding);
	static std::string GetExeFolder();
	static std::string GetPinballFolder();
};
