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

#include "main.h"
#include "hotkey_window.h"

#include <wx/button.h>
#include <wx/listctrl.h>
#include <wx/sizer.h>
#include <wx/stattext.h>
#include <wx/textctrl.h>

#include "hotkey_utils.h"
#include "theme.h"

HotkeysDialog::HotkeysDialog(wxWindow* parent, MainMenuBar& menubar) :
	wxDialog(parent, wxID_ANY, "Hotkey Configuration", wxDefaultPosition, wxSize(900, 600), wxDEFAULT_DIALOG_STYLE | wxRESIZE_BORDER),
	menu_bar(menubar),
	list_ctrl(nullptr),
	hotkey_ctrl(nullptr),
	info_label(nullptr),
	set_button(nullptr),
	clear_button(nullptr),
	reset_button(nullptr),
	capturing(false),
	selected_index(-1),
	selected_row(-1)
{
	const ThemeColors& theme = Theme::Dark();
	SetBackgroundColour(theme.background);
	SetForegroundColour(theme.text);

	entries = menu_bar.GetMenuHotkeys();

	wxBoxSizer* mainSizer = new wxBoxSizer(wxVERTICAL);

	list_ctrl = new wxListCtrl(this, wxID_ANY, wxDefaultPosition, wxDefaultSize,
		wxLC_REPORT | wxLC_SINGLE_SEL | wxLC_HRULES | wxLC_VRULES);
	list_ctrl->SetTextColour(theme.text);
	list_ctrl->SetForegroundColour(theme.text);
	list_ctrl->SetBackgroundColour(theme.surface);

	wxListItem column;
	column.SetText("Menu");
	column.SetTextColour(*wxWHITE);
	list_ctrl->InsertColumn(0, column);
	column.SetText("Action");
	list_ctrl->InsertColumn(1, column);
	column.SetText("Hotkey");
	list_ctrl->InsertColumn(2, column);

	mainSizer->Add(list_ctrl, 1, wxEXPAND | wxALL, 10);

	wxBoxSizer* hotkeySizer = new wxBoxSizer(wxHORIZONTAL);
	hotkeySizer->Add(new wxStaticText(this, wxID_ANY, "Hotkey:"), 0, wxALIGN_CENTER_VERTICAL | wxRIGHT, 5);
	hotkey_ctrl = new wxTextCtrl(this, wxID_ANY, "", wxDefaultPosition, wxDefaultSize, wxTE_READONLY | wxTE_PROCESS_TAB);
	hotkeySizer->Add(hotkey_ctrl, 1, wxALIGN_CENTER_VERTICAL | wxRIGHT, 5);

	set_button = new wxButton(this, wxID_ANY, "Set");
	clear_button = new wxButton(this, wxID_ANY, "Clear");
	reset_button = new wxButton(this, wxID_ANY, "Reset to Default");

	hotkeySizer->Add(set_button, 0, wxRIGHT, 5);
	hotkeySizer->Add(clear_button, 0, wxRIGHT, 5);
	hotkeySizer->Add(reset_button, 0);

	mainSizer->Add(hotkeySizer, 0, wxEXPAND | wxLEFT | wxRIGHT, 10);

	info_label = new wxStaticText(this, wxID_ANY, "Select an action to edit its hotkey.");
	mainSizer->Add(info_label, 0, wxEXPAND | wxLEFT | wxRIGHT | wxTOP, 10);

	wxBoxSizer* buttonSizer = new wxBoxSizer(wxHORIZONTAL);
	buttonSizer->AddStretchSpacer();
	wxButton* saveButton = new wxButton(this, wxID_OK, "Save");
	wxButton* cancelButton = new wxButton(this, wxID_CANCEL, "Cancel");
	buttonSizer->Add(saveButton, 0, wxRIGHT, 5);
	buttonSizer->Add(cancelButton, 0);
	mainSizer->Add(buttonSizer, 0, wxEXPAND | wxALL, 10);

	SetSizerAndFit(mainSizer);
	SetMinSize(wxSize(900, 600));
	CentreOnParent();

	PopulateList();
	UpdateButtonStates();

	list_ctrl->Bind(wxEVT_LIST_ITEM_SELECTED, &HotkeysDialog::OnItemSelected, this);
	list_ctrl->Bind(wxEVT_LIST_ITEM_DESELECTED, &HotkeysDialog::OnItemDeselected, this);
	set_button->Bind(wxEVT_BUTTON, &HotkeysDialog::OnSetHotkey, this);
	clear_button->Bind(wxEVT_BUTTON, &HotkeysDialog::OnClearHotkey, this);
	reset_button->Bind(wxEVT_BUTTON, &HotkeysDialog::OnResetHotkey, this);
	hotkey_ctrl->Bind(wxEVT_KEY_DOWN, &HotkeysDialog::OnHotkeyKeyDown, this);
	Bind(wxEVT_BUTTON, &HotkeysDialog::OnSave, this, wxID_OK);
	Bind(wxEVT_BUTTON, &HotkeysDialog::OnCancel, this, wxID_CANCEL);
}

void HotkeysDialog::PopulateList()
{
	list_ctrl->Freeze();
	list_ctrl->DeleteAllItems();

	for(size_t i = 0; i < entries.size(); ++i) {
		long row = list_ctrl->InsertItem(list_ctrl->GetItemCount(), wxstr(entries[i].menu));
		list_ctrl->SetItem(row, 1, wxstr(entries[i].action));
		list_ctrl->SetItem(row, 2, wxstr(entries[i].currentHotkey));
		list_ctrl->SetItemData(row, static_cast<long>(i));
	}

	list_ctrl->SetColumnWidth(0, wxLIST_AUTOSIZE_USEHEADER);
	list_ctrl->SetColumnWidth(1, wxLIST_AUTOSIZE_USEHEADER);
	list_ctrl->SetColumnWidth(2, wxLIST_AUTOSIZE_USEHEADER);

	list_ctrl->Thaw();
}

void HotkeysDialog::UpdateSelection(int row)
{
	if(row < 0) {
		selected_index = -1;
		selected_row = -1;
		hotkey_ctrl->ChangeValue("");
		info_label->SetLabel("Select an action to edit its hotkey.");
		UpdateButtonStates();
		return;
	}

	long data = list_ctrl->GetItemData(row);
	if(data < 0 || static_cast<size_t>(data) >= entries.size()) {
		selected_index = -1;
		selected_row = -1;
		UpdateButtonStates();
		return;
	}

	selected_index = static_cast<int>(data);
	selected_row = row;
	hotkey_ctrl->ChangeValue(wxstr(entries[selected_index].currentHotkey));
	info_label->SetLabel("Click Set, then press the desired key combination.");
	UpdateButtonStates();
}

void HotkeysDialog::StartCapture()
{
	if(selected_index < 0) {
		info_label->SetLabel("Select an action before setting a hotkey.");
		return;
	}

	capturing = true;
	info_label->SetLabel("Press the new key combination, or press Esc to cancel.");
	hotkey_ctrl->ChangeValue("");
	hotkey_ctrl->SetFocus();
}

void HotkeysDialog::ApplyHotkeyToSelection(const std::string& hotkey)
{
	if(selected_index < 0)
		return;

	entries[selected_index].currentHotkey = hotkey;
	hotkey_ctrl->ChangeValue(wxstr(hotkey));
	if(selected_row >= 0) {
		list_ctrl->SetItem(selected_row, 2, wxstr(hotkey));
	}
}

void HotkeysDialog::UpdateButtonStates()
{
	const bool hasSelection = selected_index >= 0;
	set_button->Enable(hasSelection);
	clear_button->Enable(hasSelection);
	reset_button->Enable(hasSelection);
}

void HotkeysDialog::OnItemSelected(wxListEvent& event)
{
	capturing = false;
	UpdateSelection(event.GetIndex());
}

void HotkeysDialog::OnItemDeselected(wxListEvent& WXUNUSED(event))
{
	capturing = false;
	UpdateSelection(-1);
}

void HotkeysDialog::OnSetHotkey(wxCommandEvent& WXUNUSED(event))
{
	StartCapture();
}

void HotkeysDialog::OnClearHotkey(wxCommandEvent& WXUNUSED(event))
{
	capturing = false;
	if(selected_index < 0)
		return;

	ApplyHotkeyToSelection("");
	info_label->SetLabel("Hotkey cleared.");
}

void HotkeysDialog::OnResetHotkey(wxCommandEvent& WXUNUSED(event))
{
	capturing = false;
	if(selected_index < 0)
		return;

	ApplyHotkeyToSelection(entries[selected_index].defaultHotkey);
	info_label->SetLabel("Hotkey reset to default.");
}

void HotkeysDialog::OnSave(wxCommandEvent& WXUNUSED(event))
{
	menu_bar.ApplyMenuHotkeys(entries);
	EndModal(wxID_OK);
}

void HotkeysDialog::OnCancel(wxCommandEvent& WXUNUSED(event))
{
	EndModal(wxID_CANCEL);
}

void HotkeysDialog::OnHotkeyKeyDown(wxKeyEvent& event)
{
	if(!capturing) {
		event.Skip();
		return;
	}

	if(event.GetKeyCode() == WXK_ESCAPE) {
		capturing = false;
		info_label->SetLabel("Hotkey capture cancelled.");
		UpdateSelection(selected_row);
		event.Skip(false);
		return;
	}

	HotkeyData data;
	if(!EventToHotkey(event, data)) {
		event.Skip();
		return;
	}

	std::string text = HotkeyToText(data);
	if(text.empty()) {
		event.Skip();
		return;
	}

	capturing = false;
	ApplyHotkeyToSelection(text);
	info_label->SetLabel("Hotkey updated.");
	event.Skip(false);
}
