#include "pch.h"
#include "splash.h"

#include "winmain.h"
#include "wintext.h"

namespace
{
	struct StepInfo
	{
		const char* Text;
		// The part of the bar, in 1/1000.
		int From, To;
	};

	// The parts are estimates of the time of each step.
	const StepInfo Steps[] =
	{
		{"Starting", 0, 30},
		{"Loading game data", 30, 500},
		{"Building the table", 500, 950},
		{"Starting the game", 950, 1000},
	};

	// A splash window: no caption, not in the task bar, and it never becomes the
	// active window. Thus its close never moves the activation to the main thread,
	// and Close can wait for its thread.
	const wchar_t ClassName[] = L"PINTBALLLoad";
	constexpr DWORD WindowStyle = WS_POPUP | WS_DLGFRAME;
	constexpr DWORD WindowExStyle = WS_EX_TOOLWINDOW;
	constexpr UINT CloseMessage = WM_APP + 1;
	constexpr int Margin = 12;
	constexpr int ClientWidth = 360;

	HANDLE Thread = nullptr;
	DWORD ThreadId = 0;
	HANDLE ReadyEvent = nullptr;
	CRITICAL_SECTION Lock;
	bool LockReady = false;

	// The two threads use these under Lock.
	HWND Window = nullptr;
	bool CloseRequested = false;
	LoadStep Step = LoadStep::Start;
	int Permille = 0;
	int RequestedPercent = -1;
	char Detail[64] = "";

	// Set by the window thread before the window shows.
	HFONT Font = nullptr, TitleFont = nullptr;
	HICON Icon = nullptr;
	int IconSize = 32, TextLeft = 56;
	int TitleHeight = 19, StatusTop = 35, TextHeight = 13, BarTop = 54, BarHeight = 18, PercentWidth = 40;

	void CopyDetail(const char* detail)
	{
		strncpy(Detail, detail ? detail : "", sizeof Detail - 1);
		Detail[sizeof Detail - 1] = 0;
	}

	// Asks the window thread to paint again. The call does not wait.
	void Invalidate()
	{
		EnterCriticalSection(&Lock);
		auto window = Window;
		LeaveCriticalSection(&Lock);
		if (window)
			InvalidateRect(window, nullptr, FALSE);
	}

	HBRUSH SystemColorBrush(int index)
	{
		return reinterpret_cast<HBRUSH>(static_cast<size_t>(index + 1));
	}

	// The height of a font, from the screen.
	int FontHeight(HFONT font, int* averageWidth)
	{
		auto height = 0;
		auto screenDC = GetDC(nullptr);
		if (!screenDC)
			return height;
		auto oldFont = font ? SelectObject(screenDC, font) : nullptr;
		TEXTMETRICW metrics;
		if (GetTextMetricsW(screenDC, &metrics))
		{
			height = metrics.tmHeight;
			if (averageWidth)
				*averageWidth = metrics.tmAveCharWidth;
		}
		if (oldFont)
			SelectObject(screenDC, oldFont);
		ReleaseDC(nullptr, screenDC);
		return height;
	}

	void SetLayout()
	{
		IconSize = GetSystemMetrics(SM_CXICON);
		Font = static_cast<HFONT>(GetStockObject(DEFAULT_GUI_FONT));

		// The title: the same font, bold and 1.5 times as high.
		LOGFONTW logFont;
		if (Font && GetObjectW(Font, sizeof logFont, &logFont) == sizeof logFont)
		{
			logFont.lfWeight = FW_BOLD;
			logFont.lfHeight = logFont.lfHeight * 3 / 2;
			TitleFont = CreateFontIndirectW(&logFont);
		}

		auto averageWidth = 6;
		auto height = FontHeight(Font, &averageWidth);
		if (height > 0)
			TextHeight = height;
		PercentWidth = averageWidth * 5;
		height = FontHeight(TitleFont ? TitleFont : Font, nullptr);
		TitleHeight = height > 0 ? height : TextHeight;

		TextLeft = Margin + IconSize + Margin;
		StatusTop = Margin + TitleHeight + 4;
		BarTop = StatusTop + TextHeight + 6;
		BarHeight = TextHeight + 5;
	}

	void Paint(HWND window)
	{
		PAINTSTRUCT paint;
		auto dc = BeginPaint(window, &paint);
		if (!dc)
			return;
		RECT client;
		GetClientRect(window, &client);

		char detail[sizeof Detail];
		EnterCriticalSection(&Lock);
		auto step = Step;
		auto permille = Permille;
		memcpy(detail, Detail, sizeof detail);
		LeaveCriticalSection(&Lock);

		std::string text = Steps[static_cast<int>(step)].Text;
		if (detail[0])
		{
			text += " (";
			text += detail;
			text += ")";
		}
		auto percent = std::to_string(permille / 10);
		percent += "%";

		// Paint into memory first, then copy: the window does not flicker.
		auto memoryDC = CreateCompatibleDC(dc);
		auto bitmap = memoryDC ? CreateCompatibleBitmap(dc, client.right, client.bottom) : nullptr;
		if (memoryDC && !bitmap)
		{
			DeleteDC(memoryDC);
			memoryDC = nullptr;
		}
		auto drawDC = memoryDC ? memoryDC : dc;
		auto oldBitmap = memoryDC ? SelectObject(memoryDC, bitmap) : nullptr;
		auto oldFont = SelectObject(drawDC, TitleFont ? TitleFont : Font);

		FillRect(drawDC, &client, SystemColorBrush(COLOR_BTNFACE));
		if (Icon)
			DrawIcon(drawDC, Margin, Margin, Icon);

		SetBkMode(drawDC, TRANSPARENT);
		SetTextColor(drawDC, GetSysColor(COLOR_BTNTEXT));
		RECT titleRect{TextLeft, Margin, client.right - Margin, Margin + TitleHeight};
		DrawTextW(drawDC, L"PINTBALL", -1, &titleRect, DT_LEFT | DT_SINGLELINE | DT_VCENTER | DT_NOPREFIX);

		SelectObject(drawDC, Font);
		RECT percentRect{client.right - Margin - PercentWidth, StatusTop, client.right - Margin, StatusTop + TextHeight};
		RECT textRect{TextLeft, StatusTop, percentRect.left - 4, StatusTop + TextHeight};
		DrawTextW(drawDC, wintext::ToWide(text).data(), -1, &textRect,
		          DT_LEFT | DT_SINGLELINE | DT_VCENTER | DT_NOPREFIX | DT_END_ELLIPSIS);
		DrawTextW(drawDC, wintext::ToWide(percent).data(), -1, &percentRect,
		          DT_RIGHT | DT_SINGLELINE | DT_VCENTER | DT_NOPREFIX);

		// A progress bar with blocks, as the NT 4.0 progress control has.
		RECT bar{TextLeft, BarTop, client.right - Margin, BarTop + BarHeight};
		DrawEdge(drawDC, &bar, BDR_SUNKENOUTER, BF_RECT | BF_ADJUST);
		bar.left += 1;
		bar.top += 1;
		bar.right -= 1;
		bar.bottom -= 1;
		auto filled = (bar.right - bar.left) * permille / 1000;
		auto blockWidth = (bar.bottom - bar.top) * 2 / 3;
		if (blockWidth < 2)
			blockWidth = 2;
		for (auto x = bar.left; x < bar.left + filled; x += blockWidth + 2)
		{
			RECT block{x, bar.top, x + blockWidth, bar.bottom};
			if (block.right > bar.right)
				block.right = bar.right;
			FillRect(drawDC, &block, SystemColorBrush(COLOR_HIGHLIGHT));
		}

		if (oldFont)
			SelectObject(drawDC, oldFont);
		if (memoryDC)
		{
			BitBlt(dc, 0, 0, client.right, client.bottom, memoryDC, 0, 0, SRCCOPY);
			SelectObject(memoryDC, oldBitmap);
			DeleteObject(bitmap);
			DeleteDC(memoryDC);
		}
		EndPaint(window, &paint);
	}

	LRESULT CALLBACK WindowProc(HWND window, UINT message, WPARAM wParam, LPARAM lParam)
	{
		switch (message)
		{
		case WM_PAINT:
			Paint(window);
			return 0;
		case WM_ERASEBKGND:
			// Paint fills all of the window.
			return 1;
		case WM_MOUSEACTIVATE:
			return MA_NOACTIVATE;
		case WM_CLOSE:
			// The player can not close it. The game closes it after the load.
			return 0;
		case CloseMessage:
			DestroyWindow(window);
			return 0;
		case WM_DESTROY:
			EnterCriticalSection(&Lock);
			Window = nullptr;
			LeaveCriticalSection(&Lock);
			PostQuitMessage(0);
			return 0;
		default:
			break;
		}
		return DefWindowProcW(window, message, wParam, lParam);
	}

	DWORD WINAPI ThreadProc(LPVOID)
	{
		WNDCLASSW windowClass{};
		windowClass.lpfnWndProc = WindowProc;
		windowClass.hInstance = winmain::Instance;
		windowClass.hCursor = LoadCursorA(nullptr, IDC_APPSTARTING);
		windowClass.lpszClassName = ClassName;
		// After a restart the class is registered already, and this fails.
		RegisterClassW(&windowClass);

		Icon = LoadIconW(winmain::Instance, L"ICON_1");
		SetLayout();
		auto clientHeight = BarTop + BarHeight + Margin;
		if (clientHeight < Margin + IconSize + Margin)
			clientHeight = Margin + IconSize + Margin;

		RECT rect{0, 0, ClientWidth, clientHeight};
		AdjustWindowRect(&rect, WindowStyle, FALSE);
		auto width = rect.right - rect.left, height = rect.bottom - rect.top;
		auto window = CreateWindowExW(WindowExStyle, ClassName, L"PINTBALL", WindowStyle,
		                              (GetSystemMetrics(SM_CXSCREEN) - width) / 2,
		                              (GetSystemMetrics(SM_CYSCREEN) - height) / 2,
		                              width, height, nullptr, nullptr, winmain::Instance, nullptr);
		if (window)
		{
			EnterCriticalSection(&Lock);
			Window = window;
			auto closeNow = CloseRequested;
			LeaveCriticalSection(&Lock);
			if (closeNow)
			{
				DestroyWindow(window);
			}
			else
			{
				ShowWindow(window, SW_SHOWNOACTIVATE);
				UpdateWindow(window);
			}
		}
		SetEvent(ReadyEvent);

		if (window)
		{
			MSG message;
			while (GetMessageW(&message, nullptr, 0, 0) > 0)
			{
				TranslateMessage(&message);
				DispatchMessageW(&message);
			}
		}
		if (TitleFont)
		{
			DeleteObject(TitleFont);
			TitleFont = nullptr;
		}
		return 0;
	}
}

void splash::Show()
{
	if (Thread)
		return;
	if (!LockReady)
	{
		InitializeCriticalSection(&Lock);
		LockReady = true;
	}

	EnterCriticalSection(&Lock);
	Window = nullptr;
	CloseRequested = false;
	Step = LoadStep::Start;
	Permille = 0;
	RequestedPercent = -1;
	Detail[0] = 0;
	LeaveCriticalSection(&Lock);

	ReadyEvent = CreateEventA(nullptr, TRUE, FALSE, nullptr);
	if (!ReadyEvent)
		return;
	Thread = CreateThread(nullptr, 0, ThreadProc, nullptr, 0, &ThreadId);
	if (!Thread)
	{
		CloseHandle(ReadyEvent);
		ReadyEvent = nullptr;
		return;
	}
	// The window paints at once, also while the main thread loads.
	SetThreadPriority(Thread, THREAD_PRIORITY_ABOVE_NORMAL);
	WaitForSingleObject(ReadyEvent, 3000);
}

void splash::Close()
{
	if (!Thread)
		return;

	EnterCriticalSection(&Lock);
	CloseRequested = true;
	auto window = Window;
	LeaveCriticalSection(&Lock);
	if (window)
		PostMessageW(window, CloseMessage, 0, 0);

	// The window thread can not wait for itself (memalloc_failure can run there).
	if (GetCurrentThreadId() != ThreadId)
		WaitForSingleObject(Thread, 5000);
	CloseHandle(Thread);
	Thread = nullptr;
	ThreadId = 0;
	CloseHandle(ReadyEvent);
	ReadyEvent = nullptr;
}

void splash::SetStep(LoadStep step, const char* detail)
{
	if (!LockReady)
		return;
	EnterCriticalSection(&Lock);
	Step = step;
	auto from = Steps[static_cast<int>(step)].From;
	if (Permille < from)
		Permille = from;
	RequestedPercent = Permille / 10;
	CopyDetail(detail);
	LeaveCriticalSection(&Lock);
	Invalidate();
}

void splash::SetProgress(int done, int total)
{
	if (!LockReady || total <= 0)
		return;
	if (done < 0)
		done = 0;
	if (done > total)
		done = total;

	EnterCriticalSection(&Lock);
	const auto& info = Steps[static_cast<int>(Step)];
	auto permille = info.From + (info.To - info.From) * done / total;
	if (permille > Permille)
		Permille = permille;
	// Paint again only when the percent changes: the load does not wait for paints.
	auto changed = Permille / 10 != RequestedPercent;
	if (changed)
		RequestedPercent = Permille / 10;
	LeaveCriticalSection(&Lock);
	if (changed)
		Invalidate();
}

void splash::SetDetail(const char* detail)
{
	if (!LockReady)
		return;
	EnterCriticalSection(&Lock);
	CopyDetail(detail);
	LeaveCriticalSection(&Lock);
	Invalidate();
}
