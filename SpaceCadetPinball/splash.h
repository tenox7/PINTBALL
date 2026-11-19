#pragma once

// The load steps. Each step fills its own part of the progress bar.
enum class LoadStep
{
	Start,
	Data,
	Table,
	Finish,
};

// The load window: a small window with a progress bar, which shows while the
// game loads its data. It has its own thread with its own message loop. Thus it
// paints itself while the main thread loads, and the main thread does not
// process messages before the game is ready.
class splash
{
public:
	// Opens the window and waits until it shows. It does nothing when the
	// window is open.
	static void Show();
	// Closes the window and waits for its thread.
	static void Close();
	// Starts a load step. The detail (a file name) shows after the step text.
	static void SetStep(LoadStep step, const char* detail = nullptr);
	// The progress in the current step.
	static void SetProgress(int done, int total);
	static void SetDetail(const char* detail);
};
