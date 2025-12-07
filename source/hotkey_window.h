//////////////////////////////////////////////////////////////////////
// This file is part of Remere's Map Editor
//////////////////////////////////////////////////////////////////////
// Remere's Map Editor is free software: you can redistribute it and/or modify
// it under the terms of the GNU General Public License as published by
// the Free Software Foundation, either version 3 of the License, or
// (at your option) any later version.
//
// Remere's Map Editor is distributed in the hope that it will be useful,
// but WITHOUT ANY WARRANTY; without even the implied warranty of
// MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE. See the
// GNU General Public License for more details.
//
// You should have received a copy of the GNU General Public License
// along with this program. If not, see <http://www.gnu.org/licenses/>.
//////////////////////////////////////////////////////////////////////

#ifndef RME_HOTKEY_WINDOW_H_
#define RME_HOTKEY_WINDOW_H_

#include "main.h"
#include "main_menubar.h"

#include <vector>

class wxListCtrl;
class wxTextCtrl;
class wxStaticText;
class wxButton;
class wxListEvent;

class HotkeysDialog : public wxDialog
{
public:
	HotkeysDialog(wxWindow* parent, MainMenuBar& menubar);

private:
	void PopulateList();
	void UpdateSelection(int index);
	void StartCapture();
	void ApplyHotkeyToSelection(const std::string& hotkey);
	void UpdateButtonStates();

	void OnItemSelected(wxListEvent& event);
	void OnItemDeselected(wxListEvent& event);
	void OnSetHotkey(wxCommandEvent& event);
	void OnClearHotkey(wxCommandEvent& event);
	void OnResetHotkey(wxCommandEvent& event);
	void OnSave(wxCommandEvent& event);
	void OnCancel(wxCommandEvent& event);
	void OnHotkeyKeyDown(wxKeyEvent& event);

	MainMenuBar& menu_bar;
	std::vector<MenuHotkeyEntry> entries;
	wxListCtrl* list_ctrl;
	wxTextCtrl* hotkey_ctrl;
	wxStaticText* info_label;
	wxButton* set_button;
	wxButton* clear_button;
	wxButton* reset_button;
	bool capturing;
	int selected_index;
	int selected_row;
};

#endif
