#include "pch.h"
#include "high_score.h"

#include "options.h"
#include "pb.h"
#include "resource.h"
#include "score.h"
#include "translations.h"
#include "winmain.h"
#include "wintext.h"

bool high_score::dlg_enter_name;
bool high_score::ShowDialog = false;
high_score_entry high_score::DlgData;
std::vector<high_score_entry> high_score::ScoreQueue;
high_score_struct high_score::highscore_table[5];

int high_score::read()
{
	char Buffer[20];

	int checkSum = 0;
	clear_table();
	for (auto position = 0; position < 5; ++position)
	{
		auto& tablePtr = highscore_table[position];

		snprintf(Buffer, sizeof Buffer, "%d", position);
		strcat(Buffer, ".Name");
		auto name = options::GetSetting(Buffer, "");
		strncpy(tablePtr.Name, name.c_str(), sizeof tablePtr.Name);

		snprintf(Buffer, sizeof Buffer, "%d", position);
		strcat(Buffer, ".Score");
		tablePtr.Score = options::get_int(Buffer, tablePtr.Score);

		for (int i = static_cast<int>(strlen(tablePtr.Name)); --i >= 0; checkSum += tablePtr.Name[i])
		{
		}
		checkSum += tablePtr.Score;
	}

	auto verification = options::get_int("Verification", 7);
	if (checkSum != verification)
		clear_table();
	return 0;
}

int high_score::write()
{
	char Buffer[20];

	int checkSum = 0;
	for (auto position = 0; position < 5; ++position)
	{
		auto& tablePtr = highscore_table[position];

		snprintf(Buffer, sizeof Buffer, "%d", position);
		strcat(Buffer, ".Name");
		options::SetSetting(Buffer, tablePtr.Name);

		snprintf(Buffer, sizeof Buffer, "%d", position);
		strcat(Buffer, ".Score");
		options::set_int(Buffer, tablePtr.Score);

		for (int i = static_cast<int>(strlen(tablePtr.Name)); --i >= 0; checkSum += tablePtr.Name[i])
		{
		}
		checkSum += tablePtr.Score;
	}

	options::set_int("Verification", checkSum);
	return 0;
}

void high_score::clear_table()
{
	for (auto& table : highscore_table)
	{
		table.Score = -999;
		table.Name[0] = 0;
	}
}

int high_score::get_score_position(int score)
{
	if (score <= 0)
		return -1;

	for (int position = 0; position < 5; position++)
	{
		if (highscore_table[position].Score < score)
			return position;
	}
	return -1;
}

void high_score::place_new_score_into(high_score_entry data)
{
	if (data.Position >= 0 && data.Position < 5)
	{
		for (int i = 4; i > data.Position; i--)
		{
			highscore_table[i] = highscore_table[i - 1];
		}

		data.Entry.Name[31] = 0;
		highscore_table[data.Position] = data.Entry;
	}
}

void high_score::show_high_score_dialog()
{
	ShowDialog = true;
}

void high_score::show_and_set_high_score_dialog(high_score_entry score)
{
	ScoreQueue.insert(ScoreQueue.begin(), score);
	ShowDialog = true;
}

// Shows the high score dialog, if show_high_score_dialog asked for it. A new
// score gets an edit box for the name of the player. Returns true when the
// dialog was shown.
bool high_score::RenderHighScoreDialog()
{
	if (!ShowDialog)
		return false;
	ShowDialog = false;

	do
	{
		dlg_enter_name = false;
		while (!ScoreQueue.empty())
		{
			DlgData = ScoreQueue.back();
			ScoreQueue.pop_back();
			if (DlgData.Position < 0 || DlgData.Position > 4)
			{
				DlgData.Position = get_score_position(DlgData.Entry.Score);
			}

			if (DlgData.Position != -1)
			{
				dlg_enter_name = true;
				break;
			}
		}

		DialogBoxParamW(winmain::Instance, MAKEINTRESOURCEW(IDD_HIGHSCORES), winmain::MainWindow, DialogProc, 0);
	}
	// Show the dialog again for the next score in the queue
	while (!ScoreQueue.empty());

	return true;
}

// The table, with the new score at its position.
void high_score::FillRows(HWND dialog)
{
	char buf[36];
	for (int offset = 0, row = 0; row < 5; row++)
	{
		snprintf(buf, sizeof buf, "%d", row + 1);
		wintext::SetItemText(dialog, IDC_HS_RANK0 + row, buf);

		int score;
		if (dlg_enter_name && DlgData.Position == row)
		{
			offset = -1;
			score = DlgData.Entry.Score;
			wintext::SetItemText(dialog, IDC_HS_NAME0 + row, "");
		}
		else
		{
			auto currentRow = &highscore_table[row + offset];
			score = currentRow->Score;
			wintext::SetItemText(dialog, IDC_HS_NAME0 + row, currentRow->Name);
		}

		score::string_format(score, buf);
		wintext::SetItemText(dialog, IDC_HS_SCORE0 + row, buf);
	}
}

BOOL CALLBACK high_score::DialogProc(HWND dialog, UINT message, WPARAM wParam, LPARAM lParam)
{
	switch (message)
	{
	case WM_INITDIALOG:
		{
			wintext::SetText(dialog, pb::get_rc_string(Msg::HIGHSCORES_Caption));
			wintext::SetItemText(dialog, IDC_HS_RANK_HEADER, pb::get_rc_string(Msg::HIGHSCORES_Rank));
			wintext::SetItemText(dialog, IDC_HS_NAME_HEADER, pb::get_rc_string(Msg::HIGHSCORES_Name));
			wintext::SetItemText(dialog, IDC_HS_SCORE_HEADER, pb::get_rc_string(Msg::HIGHSCORES_Score));
			wintext::SetItemText(dialog, IDOK, pb::get_rc_string(Msg::GenericOk));
			wintext::SetItemText(dialog, IDCANCEL, pb::get_rc_string(Msg::GenericCancel));
			wintext::SetItemText(dialog, IDC_HS_CLEAR, pb::get_rc_string(Msg::HIGHSCORES_Clear));
			FillRows(dialog);

			if (!dlg_enter_name)
				return TRUE;

			// Put the edit box on the name of the new score.
			auto nameText = GetDlgItem(dialog, IDC_HS_NAME0 + DlgData.Position);
			auto edit = GetDlgItem(dialog, IDC_HS_EDIT);
			RECT nameRect, editRect;
			GetWindowRect(nameText, &nameRect);
			GetWindowRect(edit, &editRect);
			MapWindowPoints(nullptr, dialog, reinterpret_cast<POINT*>(&nameRect), 2);
			auto editHeight = editRect.bottom - editRect.top;
			auto top = nameRect.top - (editHeight - (nameRect.bottom - nameRect.top)) / 2;
			SetWindowPos(edit, nullptr, nameRect.left - 2, top, nameRect.right - nameRect.left + 2, editHeight,
			             SWP_NOZORDER | SWP_NOACTIVATE);
			ShowWindow(nameText, SW_HIDE);

			DlgData.Entry.Name[sizeof DlgData.Entry.Name - 1] = 0;
			wintext::SetText(edit, DlgData.Entry.Name);
			SendMessageW(edit, EM_LIMITTEXT, sizeof DlgData.Entry.Name - 1, 0);
			ShowWindow(edit, SW_SHOW);
			SendMessageW(edit, EM_SETSEL, 0, -1);
			SetFocus(edit);
			return FALSE;
		}
	case WM_COMMAND:
		switch (LOWORD(wParam))
		{
		case IDOK:
			if (dlg_enter_name)
			{
				// The name is UTF-8 in 31 bytes: cut it at the start of a character.
				auto name = wintext::GetText(GetDlgItem(dialog, IDC_HS_EDIT));
				size_t length = name.size();
				if (length > sizeof DlgData.Entry.Name - 1)
				{
					length = sizeof DlgData.Entry.Name - 1;
					while (length > 0 && (static_cast<unsigned char>(name[length]) & 0xC0) == 0x80)
						length--;
				}
				memcpy(DlgData.Entry.Name, name.c_str(), length);
				DlgData.Entry.Name[length] = 0;
				place_new_score_into(DlgData);
			}
			EndDialog(dialog, IDOK);
			return TRUE;
		case IDCANCEL:
			EndDialog(dialog, IDCANCEL);
			return TRUE;
		case IDC_HS_CLEAR:
			if (MessageBoxW(dialog, wintext::ToWide(pb::get_rc_string(Msg::STRING141)).data(),
			                wintext::ToWide(pb::get_rc_string(Msg::HIGHSCORES_Caption)).data(),
			                MB_OKCANCEL | MB_ICONQUESTION) == IDOK)
			{
				clear_table();
				FillRows(dialog);
			}
			return TRUE;
		default:
			break;
		}
		break;
	default:
		break;
	}
	return FALSE;
}
