#include "pch.h"
#include "wintext.h"

std::vector<wchar_t> wintext::ToWide(const char* utf8)
{
	std::vector<wchar_t> result;
	if (!utf8)
		utf8 = "";
	result.reserve(strlen(utf8) + 1);

	auto text = reinterpret_cast<const unsigned char*>(utf8);
	while (*text)
	{
		uint32_t code = *text++;
		int extra = 0;
		if (code >= 0xF0 && code <= 0xF4)
		{
			code &= 0x07;
			extra = 3;
		}
		else if (code >= 0xE0 && code <= 0xEF)
		{
			code &= 0x0F;
			extra = 2;
		}
		else if (code >= 0xC2 && code <= 0xDF)
		{
			code &= 0x1F;
			extra = 1;
		}
		else if (code >= 0x80)
		{
			code = 0xFFFD; // Not a start byte.
		}

		for (; extra > 0; extra--)
		{
			if ((*text & 0xC0) != 0x80)
			{
				code = 0xFFFD;
				break;
			}
			code = (code << 6) | (*text++ & 0x3F);
		}

		if (code >= 0x10000)
		{
			code -= 0x10000;
			result.push_back(static_cast<wchar_t>(0xD800 + (code >> 10)));
			result.push_back(static_cast<wchar_t>(0xDC00 + (code & 0x3FF)));
		}
		else
		{
			result.push_back(static_cast<wchar_t>(code));
		}
	}

	result.push_back(0);
	return result;
}

std::string wintext::ToUtf8(const wchar_t* wide)
{
	std::string result;
	if (!wide)
		return result;

	while (*wide)
	{
		uint32_t code = static_cast<uint16_t>(*wide++);
		if (code >= 0xD800 && code <= 0xDBFF && *wide >= 0xDC00 && *wide <= 0xDFFF)
		{
			code = 0x10000 + ((code - 0xD800) << 10) + (static_cast<uint16_t>(*wide++) - 0xDC00);
		}

		if (code < 0x80)
		{
			result += static_cast<char>(code);
		}
		else if (code < 0x800)
		{
			result += static_cast<char>(0xC0 | (code >> 6));
			result += static_cast<char>(0x80 | (code & 0x3F));
		}
		else if (code < 0x10000)
		{
			result += static_cast<char>(0xE0 | (code >> 12));
			result += static_cast<char>(0x80 | ((code >> 6) & 0x3F));
			result += static_cast<char>(0x80 | (code & 0x3F));
		}
		else
		{
			result += static_cast<char>(0xF0 | (code >> 18));
			result += static_cast<char>(0x80 | ((code >> 12) & 0x3F));
			result += static_cast<char>(0x80 | ((code >> 6) & 0x3F));
			result += static_cast<char>(0x80 | (code & 0x3F));
		}
	}
	return result;
}

void wintext::SetText(HWND window, const char* utf8)
{
	SetWindowTextW(window, ToWide(utf8).data());
}

void wintext::SetItemText(HWND dialog, int id, const char* utf8)
{
	SetDlgItemTextW(dialog, id, ToWide(utf8).data());
}

std::string wintext::GetText(HWND window)
{
	auto length = GetWindowTextLengthW(window);
	if (length <= 0)
		return {};
	std::vector<wchar_t> text(static_cast<size_t>(length) + 1);
	GetWindowTextW(window, text.data(), length + 1);
	return ToUtf8(text.data());
}

std::vector<wchar_t> wintext::ToMenuText(const std::string& utf8)
{
	std::string text;
	for (auto ch : utf8)
	{
		if (ch == '&')
			text += '&';
		text += ch;
	}
	return ToWide(text);
}
