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
#include <wx/msgdlg.h>
#include <wx/sizer.h>
#include <wx/stattext.h>
#include <wx/textctrl.h>

#include <algorithm>
#include <cctype>
#include <map>

#include "hotkey_utils.h"
#include "theme.h"

namespace
{
	std::string ToLower(const std::string& text)
	{
		std::string copy = text;
		std::transform(copy.begin(), copy.end(), copy.begin(), [](unsigned char ch) {
			return static_cast<char>(std::tolower(ch));
		});
		return copy;
	}

	std::string BuildMenuKey(const std::string& menu, const std::string& action)
	{
		return menu + '\n' + action;
	}
}

HotkeysDialog::HotkeysDialog(wxWindow* parent, MainMenuBar& menubar) :
	wxDialog(parent, wxID_ANY, "Hotkey Configuration", wxDefaultPosition, wxSize(900, 600), wxDEFAULT_DIALOG_STYLE | wxRESIZE_BORDER),
	menu_bar(menubar),
	search_ctrl(nullptr),
	list_ctrl(nullptr),
	hotkey_ctrl(nullptr),
	info_label(nullptr),
	set_button(nullptr),
	set_mouse_button(nullptr),
	clear_button(nullptr),
	reset_button(nullptr),
	reset_all_button(nullptr),
	export_button(nullptr),
	import_button(nullptr),
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

	wxBoxSizer* searchSizer = new wxBoxSizer(wxHORIZONTAL);
	searchSizer->Add(new wxStaticText(this, wxID_ANY, "Search:"), 0, wxALIGN_CENTER_VERTICAL | wxRIGHT, 5);
	search_ctrl = new wxTextCtrl(this, wxID_ANY);
	searchSizer->Add(search_ctrl, 1, wxALIGN_CENTER_VERTICAL);
	mainSizer->Add(searchSizer, 0, wxEXPAND | wxALL, 10);

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
	column.SetText("Mouse Button");
	list_ctrl->InsertColumn(2, column);
	column.SetText("Hotkey");
	list_ctrl->InsertColumn(3, column);

	mainSizer->Add(list_ctrl, 1, wxEXPAND | wxALL, 10);

	wxBoxSizer* hotkeySizer = new wxBoxSizer(wxHORIZONTAL);
	hotkeySizer->Add(new wxStaticText(this, wxID_ANY, "Hotkey:"), 0, wxALIGN_CENTER_VERTICAL | wxRIGHT, 5);
	hotkey_ctrl = new wxTextCtrl(this, wxID_ANY, "", wxDefaultPosition, wxDefaultSize, wxTE_READONLY | wxTE_PROCESS_TAB);
	hotkeySizer->Add(hotkey_ctrl, 1, wxALIGN_CENTER_VERTICAL | wxRIGHT, 5);

	set_button = new wxButton(this, wxID_ANY, "Set Hotkey");
	set_mouse_button = new wxButton(this, wxID_ANY, "Set Mouse Button");
	clear_button = new wxButton(this, wxID_ANY, "Clear");
	reset_button = new wxButton(this, wxID_ANY, "Reset to Default");

	hotkeySizer->Add(set_button, 0, wxRIGHT, 5);
	hotkeySizer->Add(set_mouse_button, 0, wxRIGHT, 5);
	hotkeySizer->Add(clear_button, 0, wxRIGHT, 5);
	hotkeySizer->Add(reset_button, 0);

	mainSizer->Add(hotkeySizer, 0, wxEXPAND | wxLEFT | wxRIGHT, 10);

	info_label = new wxStaticText(this, wxID_ANY, "Select an action to edit its hotkey.");
	mainSizer->Add(info_label, 0, wxEXPAND | wxLEFT | wxRIGHT | wxTOP, 10);

	wxBoxSizer* buttonSizer = new wxBoxSizer(wxHORIZONTAL);
	reset_all_button = new wxButton(this, wxID_ANY, "Reset All");
	export_button = new wxButton(this, wxID_ANY, "Export...");
	import_button = new wxButton(this, wxID_ANY, "Import...");
	buttonSizer->Add(reset_all_button, 0, wxRIGHT, 5);
	buttonSizer->Add(export_button, 0, wxRIGHT, 5);
	buttonSizer->Add(import_button, 0, wxRIGHT, 5);
	buttonSizer->AddStretchSpacer();
	wxButton* saveButton = new wxButton(this, wxID_ANY, "Save");
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
	set_mouse_button->Bind(wxEVT_BUTTON, &HotkeysDialog::OnSetMouseButton, this);
	clear_button->Bind(wxEVT_BUTTON, &HotkeysDialog::OnClearHotkey, this);
	reset_button->Bind(wxEVT_BUTTON, &HotkeysDialog::OnResetHotkey, this);
	reset_all_button->Bind(wxEVT_BUTTON, &HotkeysDialog::OnResetAllHotkeys, this);
	export_button->Bind(wxEVT_BUTTON, &HotkeysDialog::OnExportHotkeys, this);
	import_button->Bind(wxEVT_BUTTON, &HotkeysDialog::OnImportHotkeys, this);
	saveButton->Bind(wxEVT_BUTTON, &HotkeysDialog::OnSave, this);
	search_ctrl->Bind(wxEVT_TEXT, &HotkeysDialog::OnSearchChanged, this);
	hotkey_ctrl->Bind(wxEVT_KEY_DOWN, &HotkeysDialog::OnHotkeyKeyDown, this);
	hotkey_ctrl->Bind(wxEVT_LEFT_DOWN, &HotkeysDialog::OnHotkeyMouseDown, this);
	hotkey_ctrl->Bind(wxEVT_RIGHT_DOWN, &HotkeysDialog::OnHotkeyMouseDown, this);
	hotkey_ctrl->Bind(wxEVT_MIDDLE_DOWN, &HotkeysDialog::OnHotkeyMouseDown, this);
#ifdef wxEVT_AUX1_DOWN
	hotkey_ctrl->Bind(wxEVT_AUX1_DOWN, &HotkeysDialog::OnHotkeyMouseDown, this);
#endif
#ifdef wxEVT_AUX2_DOWN
	hotkey_ctrl->Bind(wxEVT_AUX2_DOWN, &HotkeysDialog::OnHotkeyMouseDown, this);
#endif
	Bind(wxEVT_BUTTON, &HotkeysDialog::OnCancel, this, wxID_CANCEL);
}

void HotkeysDialog::PopulateList()
{
	const bool hadSelection = HasSelection();
	const HotkeyRowType previousType = selected_type;
	const int previousIndex = selected_index;

	list_ctrl->Freeze();
	list_ctrl->DeleteAllItems();
	row_mapping.clear();

	for(size_t i = 0; i < menu_entries.size(); ++i) {
		if(RowMatchesFilter(menu_entries[i].menu, menu_entries[i].action, "", menu_entries[i].currentHotkey))
			AddRow(menu_entries[i].menu, menu_entries[i].action, "", menu_entries[i].currentHotkey, HotkeyRowType::MenuAction, static_cast<int>(i));
	}

	for(size_t i = 0; i < mouse_entries.size(); ++i) {
		const std::string mouseBinding = MouseBindingToText(mouse_entries[i].currentBinding);
		if(RowMatchesFilter(mouse_entries[i].menu, mouse_entries[i].action, mouseBinding, mouse_entries[i].currentKeyboardHotkey)) {
			AddRow(mouse_entries[i].menu, mouse_entries[i].action, mouseBinding,
				mouse_entries[i].currentKeyboardHotkey, HotkeyRowType::MouseAction, static_cast<int>(i));
		}
	}

	list_ctrl->SetColumnWidth(0, wxLIST_AUTOSIZE_USEHEADER);
	list_ctrl->SetColumnWidth(1, wxLIST_AUTOSIZE_USEHEADER);
	list_ctrl->SetColumnWidth(2, wxLIST_AUTOSIZE_USEHEADER);
	list_ctrl->SetColumnWidth(3, wxLIST_AUTOSIZE_USEHEADER);

	list_ctrl->Thaw();

	if(hadSelection) {
		const int row = FindRowForEntry(previousType, previousIndex);
		if(row >= 0) {
			list_ctrl->SetItemState(row, wxLIST_STATE_SELECTED, wxLIST_STATE_SELECTED);
			list_ctrl->EnsureVisible(row);
			UpdateSelection(row);
			return;
		}
	}
	UpdateSelection(-1);
}

void HotkeysDialog::AddRow(const std::string& menu, const std::string& action, const std::string& mouseBinding, const std::string& hotkey, HotkeyRowType type, int index)
{
	long row = list_ctrl->InsertItem(list_ctrl->GetItemCount(), wxstr(menu));
	list_ctrl->SetItem(row, 1, wxstr(action));
	list_ctrl->SetItem(row, 2, wxstr(mouseBinding));
	list_ctrl->SetItem(row, 3, wxstr(hotkey));

	RowEntry entry;
	entry.type = type;
	entry.index = index;
	row_mapping.push_back(entry);
	list_ctrl->SetItemData(row, static_cast<long>(row_mapping.size() - 1));
}

bool HotkeysDialog::RowMatchesFilter(const std::string& menu, const std::string& action, const std::string& mouseBinding, const std::string& hotkey) const
{
	if(search_query.empty())
		return true;

	const std::string lowerMenu = ToLower(menu);
	const std::string lowerAction = ToLower(action);
	const std::string lowerMouse = ToLower(mouseBinding);
	const std::string lowerHotkey = ToLower(hotkey);

	return lowerMenu.find(search_query) != std::string::npos ||
		lowerAction.find(search_query) != std::string::npos ||
		lowerMouse.find(search_query) != std::string::npos ||
		lowerHotkey.find(search_query) != std::string::npos;
}

bool HotkeysDialog::FindHotkeyConflict(const std::string& hotkey, HotkeyRowType currentType, int currentIndex, std::string& outConflict) const
{
	if(hotkey.empty())
		return false;

	for(size_t i = 0; i < menu_entries.size(); ++i) {
		if(currentType == HotkeyRowType::MenuAction && static_cast<int>(i) == currentIndex)
			continue;
		if(menu_entries[i].currentHotkey == hotkey) {
			outConflict = menu_entries[i].menu + " / " + menu_entries[i].action;
			return true;
		}
	}

	for(size_t i = 0; i < mouse_entries.size(); ++i) {
		if(currentType == HotkeyRowType::MouseAction && static_cast<int>(i) == currentIndex)
			continue;
		if(mouse_entries[i].currentKeyboardHotkey == hotkey && !mouse_entries[i].currentKeyboardHotkey.empty()) {
			outConflict = mouse_entries[i].menu + " / " + mouse_entries[i].action;
			return true;
		}
	}

	return false;
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
		hotkey_ctrl->ChangeValue(wxstr(mouse_entries[selected_index].currentKeyboardHotkey));
		info_label->SetLabel("Use Set Hotkey for keyboard shortcuts or Set Mouse Button to change the button.");
	} else {
		hotkey_ctrl->ChangeValue(wxstr(menu_entries[selected_index].currentHotkey));
		info_label->SetLabel("Click Set Hotkey, then press the desired key combination.");
	}
	UpdateButtonStates();
}

void HotkeysDialog::StartCapture(CaptureMode mode)
{
	if(!HasSelection()) {
		info_label->SetLabel("Select an action before setting a hotkey.");
		return;
	}

	StopCapture();
	if(mode == CaptureMode::Mouse) {
		if(!IsMouseSelection()) {
			info_label->SetLabel("Mouse buttons can only be changed on mouse actions.");
			return;
		}
		capture_mode = CaptureMode::Mouse;
		info_label->SetLabel("Click the desired mouse button, or press Esc to cancel.");
		hotkey_ctrl->ChangeValue("");
		if(!hotkey_ctrl->HasCapture()) {
			hotkey_ctrl->CaptureMouse();
		}
	} else if(mode == CaptureMode::Keyboard) {
		capture_mode = CaptureMode::Keyboard;
		info_label->SetLabel("Press the new key combination, or press Esc to cancel.");
		hotkey_ctrl->ChangeValue("");
		hotkey_ctrl->SetFocus();
	}
}

void HotkeysDialog::StopCapture()
{
	if(capture_mode == CaptureMode::Mouse && hotkey_ctrl && hotkey_ctrl->HasCapture()) {
		hotkey_ctrl->ReleaseMouse();
	}
	capture_mode = CaptureMode::None;
}

void HotkeysDialog::ApplyKeyboardHotkey(const std::string& hotkey)
{
	if(!HasSelection())
		return;

	if(!hotkey.empty()) {
		std::string conflict;
		if(FindHotkeyConflict(hotkey, selected_type, selected_index, conflict)) {
			wxMessageBox(wxString::Format("The hotkey \"%s\" is already assigned to %s.\nPlease clear it first.", wxstr(hotkey), wxstr(conflict)),
				"Hotkey In Use", wxOK | wxICON_WARNING, this);
			info_label->SetLabel("Hotkey unchanged; already in use.");
			UpdateSelection(selected_row);
			return;
		}
	}

	if(selected_type == HotkeyRowType::MouseAction) {
		mouse_entries[selected_index].currentKeyboardHotkey = hotkey;
		hotkey_ctrl->ChangeValue(wxstr(hotkey));
	} else {
		menu_entries[selected_index].currentHotkey = hotkey;
		hotkey_ctrl->ChangeValue(wxstr(hotkey));
	}
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
	if(selected_row >= 0)
		hotkey_ctrl->ChangeValue(wxstr(mouse_entries[selected_index].currentKeyboardHotkey));
	UpdateRowText(selected_row);
}

void HotkeysDialog::UpdateButtonStates()
{
	const bool hasSelection = HasSelection();
	set_button->Enable(hasSelection);
	if(set_mouse_button)
		set_mouse_button->Enable(IsMouseSelection());
	clear_button->Enable(hasSelection);
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
		list_ctrl->SetItem(row, 3, wxstr(mouse_entries[info->index].currentKeyboardHotkey));
	} else {
		list_ctrl->SetItem(row, 2, wxEmptyString);
		list_ctrl->SetItem(row, 3, wxstr(menu_entries[info->index].currentHotkey));
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
	StartCapture(CaptureMode::Keyboard);
}

void HotkeysDialog::OnSetMouseButton(wxCommandEvent& WXUNUSED(event))
{
	StartCapture(CaptureMode::Mouse);
}

void HotkeysDialog::OnClearHotkey(wxCommandEvent& WXUNUSED(event))
{
	StopCapture();
	if(!HasSelection())
		return;
	if(IsMouseSelection()) {
		ApplyKeyboardHotkey("");
		info_label->SetLabel("Keyboard hotkey cleared.");
	} else {
		ApplyKeyboardHotkey("");
		info_label->SetLabel("Hotkey cleared.");
	}
}

void HotkeysDialog::OnResetHotkey(wxCommandEvent& WXUNUSED(event))
{
	StopCapture();
	if(!HasSelection())
		return;

	if(IsMouseSelection()) {
		ApplyMouseBinding(mouse_entries[selected_index].defaultBinding);
		ApplyKeyboardHotkey(mouse_entries[selected_index].defaultKeyboardHotkey);
		info_label->SetLabel("Mouse bindings reset to default.");
	} else {
		ApplyKeyboardHotkey(menu_entries[selected_index].defaultHotkey);
		info_label->SetLabel("Hotkey reset to default.");
	}
}

void HotkeysDialog::OnResetAllHotkeys(wxCommandEvent& WXUNUSED(event))
{
	StopCapture();
	if(wxMessageBox("Reset all hotkeys to their default values?", "Reset All Hotkeys",
		wxYES_NO | wxICON_WARNING, this) != wxYES) {
		return;
	}

	for(MenuHotkeyEntry& entry : menu_entries) {
		entry.currentHotkey = entry.defaultHotkey;
	}

	for(MouseHotkeyEntry& entry : mouse_entries) {
		entry.currentBinding = entry.defaultBinding;
		entry.currentKeyboardHotkey = entry.defaultKeyboardHotkey;
	}

	PopulateList();
	info_label->SetLabel("All hotkeys reset. Click Save to apply.");
}

void HotkeysDialog::OnExportHotkeys(wxCommandEvent& WXUNUSED(event))
{
	wxFileDialog dialog(this, "Export Hotkeys", "", "", "Hotkey Files (*.xml)|*.xml", wxFD_SAVE | wxFD_OVERWRITE_PROMPT);
	if(dialog.ShowModal() != wxID_OK)
		return;

	if(SaveHotkeysToFile(dialog.GetPath())) {
		info_label->SetLabel("Hotkeys exported.");
	} else {
		wxMessageBox("Failed to export hotkeys.", "Export Error", wxOK | wxICON_ERROR, this);
	}
}

void HotkeysDialog::OnImportHotkeys(wxCommandEvent& WXUNUSED(event))
{
	wxFileDialog dialog(this, "Import Hotkeys", "", "", "Hotkey Files (*.xml)|*.xml", wxFD_OPEN | wxFD_FILE_MUST_EXIST);
	if(dialog.ShowModal() != wxID_OK)
		return;

	if(LoadHotkeysFromFile(dialog.GetPath())) {
		PopulateList();
		info_label->SetLabel("Hotkeys loaded. Click Save to apply.");
	} else {
		wxMessageBox("Failed to import hotkeys.", "Import Error", wxOK | wxICON_ERROR, this);
	}
}

void HotkeysDialog::OnSave(wxCommandEvent& WXUNUSED(event))
{
	StopCapture();
	menu_bar.ApplyMenuHotkeys(menu_entries);
	ApplyMouseHotkeys(mouse_entries);
	info_label->SetLabel("Hotkeys saved.");
}

void HotkeysDialog::OnCancel(wxCommandEvent& WXUNUSED(event))
{
	StopCapture();
	EndModal(wxID_CANCEL);
}

void HotkeysDialog::OnSearchChanged(wxCommandEvent& event)
{
	search_query = ToLower(event.GetString().ToStdString());
	PopulateList();
}

bool HotkeysDialog::SaveHotkeysToFile(const wxString& path)
{
	wxCharBuffer buffer = path.ToUTF8();
	if(!buffer)
		return false;

	pugi::xml_document doc;
	pugi::xml_node root = doc.append_child("hotkeys");
	pugi::xml_node menuNode = root.append_child("menu");
	for(const MenuHotkeyEntry& entry : menu_entries) {
		pugi::xml_node node = menuNode.append_child("entry");
		node.append_attribute("menu") = entry.menu.c_str();
		node.append_attribute("action") = entry.action.c_str();
		node.append_attribute("hotkey") = entry.currentHotkey.c_str();
	}

	pugi::xml_node mouseNode = root.append_child("mouse");
	for(const MouseHotkeyEntry& entry : mouse_entries) {
		pugi::xml_node node = mouseNode.append_child("entry");
		node.append_attribute("action") = entry.action.c_str();
		node.append_attribute("button") = MouseBindingToIndex(entry.currentBinding);
		node.append_attribute("hotkey") = entry.currentKeyboardHotkey.c_str();
	}

	return doc.save_file(buffer.data(), "  ", pugi::format_default, pugi::encoding_utf8);
}

bool HotkeysDialog::LoadHotkeysFromFile(const wxString& path)
{
	wxCharBuffer buffer = path.ToUTF8();
	if(!buffer)
		return false;

	pugi::xml_document doc;
	pugi::xml_parse_result result = doc.load_file(buffer.data());
	if(!result)
		return false;

	pugi::xml_node root = doc.child("hotkeys");
	if(!root)
		return false;

	std::map<std::string, std::string> menuHotkeyMap;
	pugi::xml_node menuNode = root.child("menu");
	if(menuNode) {
		for(pugi::xml_node node : menuNode.children("entry")) {
			std::string menu = node.attribute("menu").as_string();
			std::string action = node.attribute("action").as_string();
			std::string hotkey = node.attribute("hotkey").as_string();
			if(menu.empty() || action.empty())
				continue;
			menuHotkeyMap[BuildMenuKey(menu, action)] = hotkey;
		}
	}

	bool modified = false;
	for(MenuHotkeyEntry& entry : menu_entries) {
		auto it = menuHotkeyMap.find(BuildMenuKey(entry.menu, entry.action));
		if(it != menuHotkeyMap.end()) {
			entry.currentHotkey = it->second;
			modified = true;
		}
	}

	pugi::xml_node mouseNode = root.child("mouse");
	if(mouseNode) {
		for(pugi::xml_node node : mouseNode.children("entry")) {
			std::string action = node.attribute("action").as_string();
			if(action.empty())
				continue;

			for(MouseHotkeyEntry& entry : mouse_entries) {
				if(entry.action != action)
					continue;

				if(node.attribute("button")) {
					entry.currentBinding = MouseBindingFromIndex(node.attribute("button").as_int(MouseBindingToIndex(entry.currentBinding)));
				}
				if(node.attribute("hotkey")) {
					entry.currentKeyboardHotkey = node.attribute("hotkey").as_string();
				}
				modified = true;
				break;
			}
		}
	}

	return modified;
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
	ApplyKeyboardHotkey(text);
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
