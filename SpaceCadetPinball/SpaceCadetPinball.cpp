#include "pch.h"
#include "winmain.h"

// Windows subsystem main
int WINAPI WinMain(HINSTANCE hInstance, HINSTANCE hPrevInstance, LPSTR lpCmdLine, int nShowCmd)
{
	return winmain::WinMain(hInstance, lpCmdLine);
}
