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
#include "hotkey_utils.h"

#include <vector>

class wxListCtrl;
class wxTextCtrl;
class wxStaticText;
class wxButton;
class wxListEvent;

enum class HotkeyRowType
{
	MenuAction,
	MouseAction,
};

struct RowEntry
{
	HotkeyRowType type = HotkeyRowType::MenuAction;
	int index = -1;
};

class HotkeysDialog : public wxDialog
{
public:
	HotkeysDialog(wxWindow* parent, MainMenuBar& menubar);

private:
	void PopulateList();
	void UpdateSelection(int index);
	void StartCapture();
	void StopCapture();
	void ApplyMenuHotkey(const std::string& hotkey);
	void ApplyMouseBinding(MouseButtonBinding binding);
	void AddRow(const std::string& menu, const std::string& action, const std::string& hotkey, HotkeyRowType type, int index);
	void UpdateButtonStates();
	bool HasSelection() const;
	bool IsMouseSelection() const;
	void UpdateRowText(int row);
	const RowEntry* GetRowEntry(int row) const;
	int FindRowForEntry(HotkeyRowType type, int index) const;

	void OnItemSelected(wxListEvent& event);
	void OnItemDeselected(wxListEvent& event);
	void OnSetHotkey(wxCommandEvent& event);
	void OnClearHotkey(wxCommandEvent& event);
	void OnResetHotkey(wxCommandEvent& event);
	void OnSave(wxCommandEvent& event);
	void OnCancel(wxCommandEvent& event);
	void OnHotkeyKeyDown(wxKeyEvent& event);
	void OnHotkeyMouseDown(wxMouseEvent& event);

	enum class CaptureMode
	{
		None,
		Keyboard,
		Mouse,
	};

	MainMenuBar& menu_bar;
	std::vector<MenuHotkeyEntry> menu_entries;
	std::vector<MouseHotkeyEntry> mouse_entries;
	std::vector<RowEntry> row_mapping;
	wxListCtrl* list_ctrl;
	wxTextCtrl* hotkey_ctrl;
	wxStaticText* info_label;
	wxButton* set_button;
	wxButton* clear_button;
	wxButton* reset_button;
	CaptureMode capture_mode;
	HotkeyRowType selected_type;
	int selected_index;
	int selected_row;
};

#endif
