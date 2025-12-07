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
#include <wx/mousestate.h>
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
	capture_mode(CaptureMode::None),
	selected_type(HotkeyRowType::MenuAction),
	selected_index(-1),
	selected_row(-1)
{
	const ThemeColors& theme = Theme::Dark();
	SetBackgroundColour(theme.background);
	SetForegroundColour(theme.text);

	menu_entries = menu_bar.GetMenuHotkeys();
	mouse_entries = GetMouseHotkeyEntries();

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
	hotkey_ctrl->Bind(wxEVT_LEFT_DOWN, &HotkeysDialog::OnHotkeyMouseDown, this);
	hotkey_ctrl->Bind(wxEVT_RIGHT_DOWN, &HotkeysDialog::OnHotkeyMouseDown, this);
	hotkey_ctrl->Bind(wxEVT_MIDDLE_DOWN, &HotkeysDialog::OnHotkeyMouseDown, this);
	Bind(wxEVT_BUTTON, &HotkeysDialog::OnSave, this, wxID_OK);
	Bind(wxEVT_BUTTON, &HotkeysDialog::OnCancel, this, wxID_CANCEL);
}

void HotkeysDialog::PopulateList()
{
	list_ctrl->Freeze();
	list_ctrl->DeleteAllItems();
	row_mapping.clear();

	for(size_t i = 0; i < menu_entries.size(); ++i) {
		AddRow(menu_entries[i].menu, menu_entries[i].action, menu_entries[i].currentHotkey, HotkeyRowType::MenuAction, static_cast<int>(i));
	}

	for(size_t i = 0; i < mouse_entries.size(); ++i) {
		AddRow(mouse_entries[i].menu, mouse_entries[i].action, MouseBindingToText(mouse_entries[i].currentBinding), HotkeyRowType::MouseAction, static_cast<int>(i));
	}

	list_ctrl->SetColumnWidth(0, wxLIST_AUTOSIZE_USEHEADER);
	list_ctrl->SetColumnWidth(1, wxLIST_AUTOSIZE_USEHEADER);
	list_ctrl->SetColumnWidth(2, wxLIST_AUTOSIZE_USEHEADER);

	list_ctrl->Thaw();
}

void HotkeysDialog::AddRow(const std::string& menu, const std::string& action, const std::string& hotkey, HotkeyRowType type, int index)
{
	long row = list_ctrl->InsertItem(list_ctrl->GetItemCount(), wxstr(menu));
	list_ctrl->SetItem(row, 1, wxstr(action));
	list_ctrl->SetItem(row, 2, wxstr(hotkey));

	RowEntry entry;
	entry.type = type;
	entry.index = index;
	row_mapping.push_back(entry);
	list_ctrl->SetItemData(row, static_cast<long>(row_mapping.size() - 1));
}

void HotkeysDialog::UpdateSelection(int row)
{
	StopCapture();
	if(row < 0) {
		selected_type = HotkeyRowType::MenuAction;
		selected_index = -1;
		selected_row = -1;
		hotkey_ctrl->ChangeValue("");
		info_label->SetLabel("Select an action to edit its hotkey.");
		UpdateButtonStates();
		return;
	}

	const RowEntry* info = GetRowEntry(row);
	if(!info || info->index < 0) {
		selected_type = HotkeyRowType::MenuAction;
		selected_index = -1;
		selected_row = -1;
		UpdateButtonStates();
		return;
	}

	selected_type = info->type;
	selected_index = info->index;
	selected_row = row;
	if(info->type == HotkeyRowType::MouseAction) {
		hotkey_ctrl->ChangeValue(wxstr(MouseBindingToText(mouse_entries[selected_index].currentBinding)));
		info_label->SetLabel("Click Set, then press the desired mouse button.");
	} else {
		hotkey_ctrl->ChangeValue(wxstr(menu_entries[selected_index].currentHotkey));
		info_label->SetLabel("Click Set, then press the desired key combination.");
	}
	UpdateButtonStates();
}

void HotkeysDialog::StartCapture()
{
	if(!HasSelection()) {
		info_label->SetLabel("Select an action before setting a hotkey.");
		return;
	}

	StopCapture();
	capture_mode = IsMouseSelection() ? CaptureMode::Mouse : CaptureMode::Keyboard;
	if(capture_mode == CaptureMode::Mouse) {
		info_label->SetLabel("Click the desired mouse button, or press Esc to cancel.");
		if(!hotkey_ctrl->HasCapture()) {
			hotkey_ctrl->CaptureMouse();
		}
	} else {
		info_label->SetLabel("Press the new key combination, or press Esc to cancel.");
	}
	hotkey_ctrl->ChangeValue("");
	hotkey_ctrl->SetFocus();
}

void HotkeysDialog::StopCapture()
{
	if(capture_mode == CaptureMode::Mouse && hotkey_ctrl && hotkey_ctrl->HasCapture()) {
		hotkey_ctrl->ReleaseMouse();
	}
	capture_mode = CaptureMode::None;
}

void HotkeysDialog::ApplyMenuHotkey(const std::string& hotkey)
{
	if(!HasSelection() || selected_type != HotkeyRowType::MenuAction)
		return;

	menu_entries[selected_index].currentHotkey = hotkey;
	hotkey_ctrl->ChangeValue(wxstr(hotkey));
	UpdateRowText(selected_row);
}

void HotkeysDialog::ApplyMouseBinding(MouseButtonBinding binding)
{
	if(!HasSelection() || selected_type != HotkeyRowType::MouseAction)
		return;

	MouseHotkeyEntry& entry = mouse_entries[selected_index];
	if(entry.currentBinding == binding) {
		UpdateRowText(selected_row);
		return;
	}

	for(size_t i = 0; i < mouse_entries.size(); ++i) {
		if(static_cast<int>(i) == selected_index)
			continue;
		if(mouse_entries[i].currentBinding == binding) {
			mouse_entries[i].currentBinding = entry.currentBinding;
			int row = FindRowForEntry(HotkeyRowType::MouseAction, static_cast<int>(i));
			if(row >= 0)
				list_ctrl->SetItem(row, 2, wxstr(MouseBindingToText(mouse_entries[i].currentBinding)));
			break;
		}
	}

	entry.currentBinding = binding;
	hotkey_ctrl->ChangeValue(wxstr(MouseBindingToText(binding)));
	UpdateRowText(selected_row);
}

void HotkeysDialog::UpdateButtonStates()
{
	const bool hasSelection = HasSelection();
	set_button->Enable(hasSelection);
	clear_button->Enable(hasSelection && !IsMouseSelection());
	reset_button->Enable(hasSelection);
}

bool HotkeysDialog::HasSelection() const
{
	return selected_index >= 0 && selected_row >= 0;
}

bool HotkeysDialog::IsMouseSelection() const
{
	return HasSelection() && selected_type == HotkeyRowType::MouseAction;
}

void HotkeysDialog::UpdateRowText(int row)
{
	if(row < 0)
		return;
	const RowEntry* info = GetRowEntry(row);
	if(!info || info->index < 0)
		return;

	if(info->type == HotkeyRowType::MouseAction) {
		list_ctrl->SetItem(row, 2, wxstr(MouseBindingToText(mouse_entries[info->index].currentBinding)));
	} else {
		list_ctrl->SetItem(row, 2, wxstr(menu_entries[info->index].currentHotkey));
	}
}

const RowEntry* HotkeysDialog::GetRowEntry(int row) const
{
	if(row < 0)
		return nullptr;
	long data = list_ctrl->GetItemData(row);
	if(data < 0 || static_cast<size_t>(data) >= row_mapping.size())
		return nullptr;
	return &row_mapping[data];
}

int HotkeysDialog::FindRowForEntry(HotkeyRowType type, int index) const
{
	for(size_t i = 0; i < row_mapping.size(); ++i) {
		if(row_mapping[i].type == type && row_mapping[i].index == index)
			return static_cast<int>(i);
	}
	return -1;
}

void HotkeysDialog::OnItemSelected(wxListEvent& event)
{
	StopCapture();
	UpdateSelection(event.GetIndex());
}

void HotkeysDialog::OnItemDeselected(wxListEvent& WXUNUSED(event))
{
	StopCapture();
	UpdateSelection(-1);
}

void HotkeysDialog::OnSetHotkey(wxCommandEvent& WXUNUSED(event))
{
	StartCapture();
}

void HotkeysDialog::OnClearHotkey(wxCommandEvent& WXUNUSED(event))
{
	StopCapture();
	if(!HasSelection())
		return;
	if(IsMouseSelection()) {
		info_label->SetLabel("Mouse actions require a button assignment.");
		return;
	}

	ApplyMenuHotkey("");
	info_label->SetLabel("Hotkey cleared.");
}

void HotkeysDialog::OnResetHotkey(wxCommandEvent& WXUNUSED(event))
{
	StopCapture();
	if(!HasSelection())
		return;

	if(IsMouseSelection()) {
		ApplyMouseBinding(mouse_entries[selected_index].defaultBinding);
		info_label->SetLabel("Mouse binding reset to default.");
	} else {
		ApplyMenuHotkey(menu_entries[selected_index].defaultHotkey);
		info_label->SetLabel("Hotkey reset to default.");
	}
}

void HotkeysDialog::OnSave(wxCommandEvent& WXUNUSED(event))
{
	StopCapture();
	menu_bar.ApplyMenuHotkeys(menu_entries);
	ApplyMouseHotkeys(mouse_entries);
	EndModal(wxID_OK);
}

void HotkeysDialog::OnCancel(wxCommandEvent& WXUNUSED(event))
{
	StopCapture();
	EndModal(wxID_CANCEL);
}

void HotkeysDialog::OnHotkeyKeyDown(wxKeyEvent& event)
{
	if(capture_mode == CaptureMode::None) {
		event.Skip();
		return;
	}

	if(capture_mode == CaptureMode::Mouse) {
		if(event.GetKeyCode() == WXK_ESCAPE) {
			StopCapture();
			info_label->SetLabel("Mouse capture cancelled.");
			UpdateSelection(selected_row);
			event.Skip(false);
		} else {
			event.Skip();
		}
		return;
	}

	if(event.GetKeyCode() == WXK_ESCAPE) {
		StopCapture();
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

	StopCapture();
	ApplyMenuHotkey(text);
	info_label->SetLabel("Hotkey updated.");
	event.Skip(false);
}

void HotkeysDialog::OnHotkeyMouseDown(wxMouseEvent& event)
{
	if(capture_mode != CaptureMode::Mouse) {
		event.Skip();
		return;
	}

	MouseButtonBinding binding;
	switch(event.GetButton()) {
		case wxMOUSE_BTN_LEFT:
			binding = MouseButtonBinding::Left;
			break;
		case wxMOUSE_BTN_MIDDLE:
			binding = MouseButtonBinding::Middle;
			break;
		case wxMOUSE_BTN_RIGHT:
			binding = MouseButtonBinding::Right;
			break;
#ifdef wxMOUSE_BTN_AUX1
		case wxMOUSE_BTN_AUX1:
			binding = MouseButtonBinding::Button4;
			break;
#endif
#ifdef wxMOUSE_BTN_AUX2
		case wxMOUSE_BTN_AUX2:
			binding = MouseButtonBinding::Button5;
			break;
#endif
		default:
			event.Skip();
			return;
	}

	StopCapture();
	ApplyMouseBinding(binding);
	info_label->SetLabel("Mouse binding updated.");
	event.Skip(false);
}
