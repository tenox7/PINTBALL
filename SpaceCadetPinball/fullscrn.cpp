#include "pch.h"
#include "fullscrn.h"


#include "options.h"
#include "pb.h"
#include "render.h"
#include "winmain.h"


int fullscrn::screen_mode;
int fullscrn::display_changed;

int fullscrn::resolution = 0;
const resolution_info fullscrn::resolution_array[3] =
{
	{640, 480, 600, 416, 501},
	{800, 600, 752, 520, 502},
	{1024, 768, 960, 666, 503},
};
float fullscrn::ScaleX = 1;
float fullscrn::ScaleY = 1;
int fullscrn::OffsetX = 0;
int fullscrn::OffsetY = 0;
WINDOWPLACEMENT fullscrn::WindowedPlacement{};

void fullscrn::init()
{
	window_size_changed();
}

void fullscrn::shutdown()
{
	if (display_changed)
		set_screen_mode(0);
}

int fullscrn::set_screen_mode(int isFullscreen)
{
	int result = isFullscreen;
	if (isFullscreen == screen_mode)
		return result;
	screen_mode = isFullscreen;
	if (isFullscreen)
	{
		enableFullscreen();
		result = 1;
	}
	else
	{
		disableFullscreen();
		result = 1;
	}
	return result;
}

// Full screen on Windows NT 4.0: a pop-up window that covers the screen. The
// display mode does not change.
int fullscrn::enableFullscreen()
{
	auto window = winmain::MainWindow;
	if (!display_changed && window)
	{
		WindowedPlacement.length = sizeof WindowedPlacement;
		GetWindowPlacement(window, &WindowedPlacement);

		auto style = GetWindowLongA(window, GWL_STYLE);
		SetWindowLongA(window, GWL_STYLE, (style & ~WS_OVERLAPPEDWINDOW) | WS_POPUP);
		SetWindowPos(window, HWND_TOP, 0, 0, GetSystemMetrics(SM_CXSCREEN), GetSystemMetrics(SM_CYSCREEN),
		             SWP_FRAMECHANGED | SWP_SHOWWINDOW);
		display_changed = 1;
		return 1;
	}
	return 0;
}

int fullscrn::disableFullscreen()
{
	auto window = winmain::MainWindow;
	if (display_changed && window)
	{
		auto style = GetWindowLongA(window, GWL_STYLE);
		SetWindowLongA(window, GWL_STYLE, (style & ~WS_POPUP) | WS_OVERLAPPEDWINDOW);
		SetWindowPlacement(window, &WindowedPlacement);
		SetWindowPos(window, nullptr, 0, 0, 0, 0,
		             SWP_NOMOVE | SWP_NOSIZE | SWP_NOZORDER | SWP_FRAMECHANGED | SWP_SHOWWINDOW);
		display_changed = 0;
	}

	return 0;
}

void fullscrn::activate(int flag)
{
	if (screen_mode)
	{
		if (!flag)
		{
			set_screen_mode(0);
		}
	}
}

int fullscrn::GetResolution()
{
	return resolution;
}

void fullscrn::SetResolution(int value)
{
	if (!pb::FullTiltMode || pb::FullTiltDemoMode)
		value = 0;
	assertm(value >= 0 && value <= 2, "Resolution value out of bounds");
	resolution = value;
}

int fullscrn::GetMaxResolution()
{
	return pb::FullTiltMode && !pb::FullTiltDemoMode ? 2 : 0;
}

void fullscrn::window_size_changed()
{
	if (!winmain::MainWindow)
		return;

	// The menu bar is not in the client area, so the full client area is for the table.
	RECT client{};
	GetClientRect(winmain::MainWindow, &client);
	int width = client.right - client.left;
	int height = client.bottom - client.top;
	if (width <= 0 || height <= 0)
		return; // The window is minimized.

	auto res = &resolution_array[resolution];
	ScaleX = static_cast<float>(width) / res->TableWidth;
	ScaleY = static_cast<float>(height) / res->TableHeight;
	OffsetX = OffsetY = 0;
	auto offset2X = 0, offset2Y = 0;

	if (options::Options.IntegerScaling)
	{
		ScaleX = ScaleX < 1 ? ScaleX : std::floor(ScaleX);
		ScaleY = ScaleY < 1 ? ScaleY : std::floor(ScaleY);
	}

	if (options::Options.UniformScaling)
	{
		ScaleY = ScaleX = std::min(ScaleX, ScaleY);
	}

	offset2X = static_cast<int>(floor(width - res->TableWidth * ScaleX));
	offset2Y = static_cast<int>(floor(height - res->TableHeight * ScaleY));
	OffsetX = offset2X / 2;
	OffsetY = offset2Y / 2;

	render::SetDestination(OffsetX, OffsetY, width - offset2X, height - offset2Y);
}
