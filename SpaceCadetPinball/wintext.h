#pragma once

// The texts of the game are UTF-8. The Unicode (W) functions of Win32 use
// UTF-16. This code does the conversion itself, so it does not depend on the
// code pages that the system has.
class wintext
{
public:
	// Gives a UTF-16 text that ends with a zero.
	static std::vector<wchar_t> ToWide(const char* utf8);
	static std::vector<wchar_t> ToWide(const std::string& utf8) { return ToWide(utf8.c_str()); }
	static std::string ToUtf8(const wchar_t* wide);

	// Window and dialog item texts from UTF-8.
	static void SetText(HWND window, const char* utf8);
	static void SetItemText(HWND dialog, int id, const char* utf8);
	static std::string GetText(HWND window);

	// Menu texts: '&' must be "&&", or it marks the next letter.
	static std::vector<wchar_t> ToMenuText(const std::string& utf8);
};
