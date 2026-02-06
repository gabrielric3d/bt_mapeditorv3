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

#include "palette_npc_paths.h"
#include "gui.h"
#include "editor.h"
#include "map.h"
#include "map_display.h"
#include "npc_path_brush.h"
#include "settings.h"

#include <algorithm>

BEGIN_EVENT_TABLE(NPCPathPalettePanel, PalettePanel)
	// Path list events
	EVT_LIST_ITEM_SELECTED(PALETTE_NPC_PATH_LISTBOX, NPCPathPalettePanel::OnClickPath)
	EVT_LIST_BEGIN_LABEL_EDIT(PALETTE_NPC_PATH_LISTBOX, NPCPathPalettePanel::OnBeginEditPathLabel)
	EVT_LIST_END_LABEL_EDIT(PALETTE_NPC_PATH_LISTBOX, NPCPathPalettePanel::OnEditPathLabel)
	EVT_BUTTON(PALETTE_NPC_PATH_ADD, NPCPathPalettePanel::OnAddPath)
	EVT_BUTTON(PALETTE_NPC_PATH_REMOVE, NPCPathPalettePanel::OnRemovePath)
	EVT_CHECKBOX(PALETTE_NPC_PATH_LOOP, NPCPathPalettePanel::OnToggleLoop)
	EVT_CHECKBOX(PALETTE_NPC_PATH_ACTIVE, NPCPathPalettePanel::OnToggleActive)
	EVT_TEXT(PALETTE_NPC_PATH_NPC_NAME, NPCPathPalettePanel::OnNPCNameChanged)

	// Waypoint list events
	EVT_LIST_ITEM_SELECTED(PALETTE_NPC_WAYPOINT_LISTBOX, NPCPathPalettePanel::OnClickWaypoint)
	EVT_LIST_ITEM_DESELECTED(PALETTE_NPC_WAYPOINT_LISTBOX, NPCPathPalettePanel::OnDeselectWaypoint)
	EVT_BUTTON(PALETTE_NPC_WAYPOINT_ADD, NPCPathPalettePanel::OnAddWaypoint)
	EVT_BUTTON(PALETTE_NPC_WAYPOINT_REMOVE, NPCPathPalettePanel::OnRemoveWaypoint)
	EVT_BUTTON(PALETTE_NPC_WAYPOINT_CLEAR, NPCPathPalettePanel::OnClearWaypoints)
	EVT_BUTTON(PALETTE_NPC_WAYPOINT_UP, NPCPathPalettePanel::OnWaypointUp)
	EVT_BUTTON(PALETTE_NPC_WAYPOINT_DOWN, NPCPathPalettePanel::OnWaypointDown)
	EVT_BUTTON(PALETTE_NPC_WAYPOINT_APPLY, NPCPathPalettePanel::OnApplyWaypointProps)

	// Action list events
	EVT_LIST_ITEM_SELECTED(PALETTE_NPC_ACTION_LISTBOX, NPCPathPalettePanel::OnClickAction)
	EVT_BUTTON(PALETTE_NPC_ACTION_ADD, NPCPathPalettePanel::OnAddAction)
	EVT_BUTTON(PALETTE_NPC_ACTION_REMOVE, NPCPathPalettePanel::OnRemoveAction)
	EVT_BUTTON(PALETTE_NPC_ACTION_EDIT, NPCPathPalettePanel::OnEditAction)
	EVT_BUTTON(PALETTE_NPC_ACTION_APPLY, NPCPathPalettePanel::OnApplyAction)
	EVT_CHOICE(PALETTE_NPC_ACTION_TYPE, NPCPathPalettePanel::OnActionTypeChanged)

	// Preview and display events
	EVT_BUTTON(PALETTE_NPC_PREVIEW_PATH, NPCPathPalettePanel::OnPreviewPath)
	EVT_CHECKBOX(PALETTE_NPC_SHOW_PATHS, NPCPathPalettePanel::OnToggleShowPaths)
END_EVENT_TABLE()

NPCPathPalettePanel::NPCPathPalettePanel(wxWindow* parent, wxWindowID id) :
	PalettePanel(parent, id),
	map(nullptr)
{
	wxSizer* root = newd wxBoxSizer(wxVERTICAL);

	// ===== Path List Section =====
	wxStaticBoxSizer* pathSizer = newd wxStaticBoxSizer(wxVERTICAL, this, "NPC Paths");

	path_list = newd wxListCtrl(this, PALETTE_NPC_PATH_LISTBOX,
		wxDefaultPosition, wxSize(-1, 100),
		wxLC_REPORT | wxLC_SINGLE_SEL | wxLC_EDIT_LABELS | wxLC_NO_HEADER);
	path_list->InsertColumn(0, "Name", wxLIST_FORMAT_LEFT, 120);
	path_list->InsertColumn(1, "NPC", wxLIST_FORMAT_LEFT, 80);
	path_list->InsertColumn(2, "Active", wxLIST_FORMAT_LEFT, 45);
	pathSizer->Add(path_list, 1, wxEXPAND | wxBOTTOM, 4);

	wxSizer* pathButtons = newd wxBoxSizer(wxHORIZONTAL);
	add_path_button = newd wxButton(this, PALETTE_NPC_PATH_ADD, "Add");
	remove_path_button = newd wxButton(this, PALETTE_NPC_PATH_REMOVE, "Remove");
	pathButtons->Add(add_path_button, 1, wxEXPAND);
	pathButtons->Add(remove_path_button, 1, wxEXPAND | wxLEFT, 4);
	pathSizer->Add(pathButtons, 0, wxEXPAND | wxBOTTOM, 4);

	// Path properties
	wxFlexGridSizer* pathPropGrid = newd wxFlexGridSizer(2, 4, 6);
	pathPropGrid->AddGrowableCol(1);

	pathPropGrid->Add(newd wxStaticText(this, wxID_ANY, "NPC Name:"), 0, wxALIGN_CENTER_VERTICAL);
	npc_name_ctrl = newd wxTextCtrl(this, PALETTE_NPC_PATH_NPC_NAME, "");
	pathPropGrid->Add(npc_name_ctrl, 1, wxEXPAND);
	pathSizer->Add(pathPropGrid, 0, wxEXPAND | wxBOTTOM, 4);

	wxSizer* pathCheckboxes = newd wxBoxSizer(wxHORIZONTAL);
	loop_checkbox = newd wxCheckBox(this, PALETTE_NPC_PATH_LOOP, "Loop");
	active_checkbox = newd wxCheckBox(this, PALETTE_NPC_PATH_ACTIVE, "Active");
	active_checkbox->SetValue(true);
	pathCheckboxes->Add(loop_checkbox, 0, wxRIGHT, 10);
	pathCheckboxes->Add(active_checkbox, 0);
	pathSizer->Add(pathCheckboxes, 0, wxBOTTOM, 4);

	// Show paths checkbox
	show_paths_checkbox = newd wxCheckBox(this, PALETTE_NPC_SHOW_PATHS, "Show NPC Paths");
	show_paths_checkbox->SetValue(g_settings.getBoolean(Config::SHOW_NPC_PATHS));
	pathSizer->Add(show_paths_checkbox, 0, wxBOTTOM, 4);

	// Preview button
	preview_path_button = newd wxButton(this, PALETTE_NPC_PREVIEW_PATH, "Preview Path");
	pathSizer->Add(preview_path_button, 0, wxEXPAND);

	root->Add(pathSizer, 1, wxEXPAND | wxBOTTOM, 6);

	// ===== Waypoint List Section =====
	wxStaticBoxSizer* waypointSizer = newd wxStaticBoxSizer(wxVERTICAL, this, "Waypoints");

	waypoint_list = newd wxListCtrl(this, PALETTE_NPC_WAYPOINT_LISTBOX,
		wxDefaultPosition, wxSize(-1, 100),
		wxLC_REPORT);
	waypoint_list->InsertColumn(0, "#", wxLIST_FORMAT_LEFT, 25);
	waypoint_list->InsertColumn(1, "X", wxLIST_FORMAT_LEFT, 45);
	waypoint_list->InsertColumn(2, "Y", wxLIST_FORMAT_LEFT, 45);
	waypoint_list->InsertColumn(3, "Z", wxLIST_FORMAT_LEFT, 25);
	waypoint_list->InsertColumn(4, "Speed", wxLIST_FORMAT_LEFT, 45);
	waypoint_list->InsertColumn(5, "Acts", wxLIST_FORMAT_LEFT, 35);
	waypointSizer->Add(waypoint_list, 1, wxEXPAND | wxBOTTOM, 4);

	wxSizer* waypointButtons = newd wxBoxSizer(wxHORIZONTAL);
	add_waypoint_button = newd wxButton(this, PALETTE_NPC_WAYPOINT_ADD, "Add");
	remove_waypoint_button = newd wxButton(this, PALETTE_NPC_WAYPOINT_REMOVE, "Remove");
	clear_waypoints_button = newd wxButton(this, PALETTE_NPC_WAYPOINT_CLEAR, "Clear");
	waypoint_up_button = newd wxButton(this, PALETTE_NPC_WAYPOINT_UP, "Up");
	waypoint_down_button = newd wxButton(this, PALETTE_NPC_WAYPOINT_DOWN, "Down");
	waypointButtons->Add(add_waypoint_button, 1, wxEXPAND);
	waypointButtons->Add(remove_waypoint_button, 1, wxEXPAND | wxLEFT, 2);
	waypointButtons->Add(clear_waypoints_button, 1, wxEXPAND | wxLEFT, 2);
	waypointButtons->Add(waypoint_up_button, 0, wxLEFT, 4);
	waypointButtons->Add(waypoint_down_button, 0, wxLEFT, 2);
	waypointSizer->Add(waypointButtons, 0, wxEXPAND | wxBOTTOM, 4);

	// Waypoint properties
	wxStaticBoxSizer* waypointPropsSizer = newd wxStaticBoxSizer(wxVERTICAL, this, "Waypoint Properties");
	pos_label = newd wxStaticText(this, wxID_ANY, "Pos: - , -");
	waypointPropsSizer->Add(pos_label, 0, wxBOTTOM, 4);

	wxFlexGridSizer* waypointGrid = newd wxFlexGridSizer(2, 4, 6);
	waypointGrid->AddGrowableCol(1);

	waypointGrid->Add(newd wxStaticText(this, wxID_ANY, "Walk Speed:"), 0, wxALIGN_CENTER_VERTICAL);
	walk_speed_ctrl = newd wxTextCtrl(this, wxID_ANY, "1.0");
	waypointGrid->Add(walk_speed_ctrl, 1, wxEXPAND);

	waypointGrid->Add(newd wxStaticText(this, wxID_ANY, "Wait Before:"), 0, wxALIGN_CENTER_VERTICAL);
	wait_before_ctrl = newd wxTextCtrl(this, wxID_ANY, "0.0");
	waypointGrid->Add(wait_before_ctrl, 1, wxEXPAND);

	waypointGrid->Add(newd wxStaticText(this, wxID_ANY, "Wait After:"), 0, wxALIGN_CENTER_VERTICAL);
	wait_after_ctrl = newd wxTextCtrl(this, wxID_ANY, "0.0");
	waypointGrid->Add(wait_after_ctrl, 1, wxEXPAND);

	waypointPropsSizer->Add(waypointGrid, 0, wxEXPAND | wxBOTTOM, 4);

	apply_waypoint_props_button = newd wxButton(this, PALETTE_NPC_WAYPOINT_APPLY, "Apply to Selected");
	waypointPropsSizer->Add(apply_waypoint_props_button, 0, wxEXPAND);

	waypointSizer->Add(waypointPropsSizer, 0, wxEXPAND);

	root->Add(waypointSizer, 1, wxEXPAND | wxBOTTOM, 6);

	// ===== Action List Section =====
	wxStaticBoxSizer* actionSizer = newd wxStaticBoxSizer(wxVERTICAL, this, "Waypoint Actions");

	action_list = newd wxListCtrl(this, PALETTE_NPC_ACTION_LISTBOX,
		wxDefaultPosition, wxSize(-1, 80),
		wxLC_REPORT | wxLC_SINGLE_SEL);
	action_list->InsertColumn(0, "#", wxLIST_FORMAT_LEFT, 25);
	action_list->InsertColumn(1, "Type", wxLIST_FORMAT_LEFT, 80);
	action_list->InsertColumn(2, "Details", wxLIST_FORMAT_LEFT, 120);
	actionSizer->Add(action_list, 1, wxEXPAND | wxBOTTOM, 4);

	wxSizer* actionButtons = newd wxBoxSizer(wxHORIZONTAL);
	add_action_button = newd wxButton(this, PALETTE_NPC_ACTION_ADD, "Add");
	remove_action_button = newd wxButton(this, PALETTE_NPC_ACTION_REMOVE, "Remove");
	edit_action_button = newd wxButton(this, PALETTE_NPC_ACTION_EDIT, "Edit");
	actionButtons->Add(add_action_button, 1, wxEXPAND);
	actionButtons->Add(remove_action_button, 1, wxEXPAND | wxLEFT, 4);
	actionButtons->Add(edit_action_button, 1, wxEXPAND | wxLEFT, 4);
	actionSizer->Add(actionButtons, 0, wxEXPAND | wxBOTTOM, 4);

	// Action properties
	wxStaticBoxSizer* actionPropsSizer = newd wxStaticBoxSizer(wxVERTICAL, this, "Action Properties");

	wxFlexGridSizer* actionGrid = newd wxFlexGridSizer(2, 4, 6);
	actionGrid->AddGrowableCol(1);

	actionGrid->Add(newd wxStaticText(this, wxID_ANY, "Type:"), 0, wxALIGN_CENTER_VERTICAL);
	wxArrayString actionTypeChoices;
	actionTypeChoices.Add("None");
	actionTypeChoices.Add("Speak");
	actionTypeChoices.Add("Wait");
	actionTypeChoices.Add("Face Direction");
	actionTypeChoices.Add("Emote");
	action_type_ctrl = newd wxChoice(this, PALETTE_NPC_ACTION_TYPE, wxDefaultPosition, wxDefaultSize, actionTypeChoices);
	action_type_ctrl->SetSelection(0);
	actionGrid->Add(action_type_ctrl, 1, wxEXPAND);

	actionGrid->Add(newd wxStaticText(this, wxID_ANY, "Message:"), 0, wxALIGN_CENTER_VERTICAL);
	action_message_ctrl = newd wxTextCtrl(this, wxID_ANY, "");
	actionGrid->Add(action_message_ctrl, 1, wxEXPAND);

	actionGrid->Add(newd wxStaticText(this, wxID_ANY, "Duration:"), 0, wxALIGN_CENTER_VERTICAL);
	action_duration_ctrl = newd wxTextCtrl(this, wxID_ANY, "0.0");
	actionGrid->Add(action_duration_ctrl, 1, wxEXPAND);

	actionGrid->Add(newd wxStaticText(this, wxID_ANY, "Direction:"), 0, wxALIGN_CENTER_VERTICAL);
	wxArrayString directionChoices;
	directionChoices.Add("North");
	directionChoices.Add("East");
	directionChoices.Add("South");
	directionChoices.Add("West");
	action_direction_ctrl = newd wxChoice(this, wxID_ANY, wxDefaultPosition, wxDefaultSize, directionChoices);
	action_direction_ctrl->SetSelection(0);
	actionGrid->Add(action_direction_ctrl, 1, wxEXPAND);

	actionGrid->Add(newd wxStaticText(this, wxID_ANY, "Emote ID:"), 0, wxALIGN_CENTER_VERTICAL);
	action_emote_ctrl = newd wxTextCtrl(this, wxID_ANY, "0");
	actionGrid->Add(action_emote_ctrl, 1, wxEXPAND);

	actionPropsSizer->Add(actionGrid, 0, wxEXPAND | wxBOTTOM, 4);

	apply_action_button = newd wxButton(this, PALETTE_NPC_ACTION_APPLY, "Apply Action");
	actionPropsSizer->Add(apply_action_button, 0, wxEXPAND);

	actionSizer->Add(actionPropsSizer, 0, wxEXPAND);

	root->Add(actionSizer, 1, wxEXPAND);

	SetSizerAndFit(root);
	UpdateActionControls();
}

NPCPathPalettePanel::~NPCPathPalettePanel()
{
	////
}

wxString NPCPathPalettePanel::GetName() const
{
	return "NPC Path Palette";
}

PaletteType NPCPathPalettePanel::GetType() const
{
	return TILESET_NPC_PATH;
}

Brush* NPCPathPalettePanel::GetSelectedBrush() const
{
	return g_gui.npc_path_brush;
}

int NPCPathPalettePanel::GetSelectedBrushSize() const
{
	return 0;
}

bool NPCPathPalettePanel::SelectBrush(const Brush* whatbrush)
{
	return whatbrush == g_gui.npc_path_brush;
}

void NPCPathPalettePanel::OnUpdate()
{
	RefreshPathList();
	RefreshWaypointList();
	RefreshActionList();
	UpdateWaypointControls();
	UpdateActionControls();
}

void NPCPathPalettePanel::OnSwitchIn()
{
	PalettePanel::OnSwitchIn();
	g_gui.ActivatePalette(GetParentPalette());
	show_paths_checkbox->SetValue(g_settings.getBoolean(Config::SHOW_NPC_PATHS));
}

void NPCPathPalettePanel::OnSwitchOut()
{
	PalettePanel::OnSwitchOut();
}

void NPCPathPalettePanel::SetMap(Map* m)
{
	map = m;
	OnUpdate();
}

double NPCPathPalettePanel::GetWaypointWalkSpeed() const
{
	double value = 1.0;
	walk_speed_ctrl->GetValue().ToDouble(&value);
	return std::max(0.1, value);
}

double NPCPathPalettePanel::GetWaypointWaitBefore() const
{
	double value = 0.0;
	wait_before_ctrl->GetValue().ToDouble(&value);
	return std::max(0.0, value);
}

double NPCPathPalettePanel::GetWaypointWaitAfter() const
{
	double value = 0.0;
	wait_after_ctrl->GetValue().ToDouble(&value);
	return std::max(0.0, value);
}

void NPCPathPalettePanel::RefreshPathList()
{
	path_list->DeleteAllItems();

	if(!map) {
		path_list->Enable(false);
		add_path_button->Enable(false);
		remove_path_button->Enable(false);
		preview_path_button->Enable(false);
		loop_checkbox->Enable(false);
		active_checkbox->Enable(false);
		npc_name_ctrl->Enable(false);
		return;
	}

	path_list->Enable(true);
	add_path_button->Enable(true);
	remove_path_button->Enable(true);
	preview_path_button->Enable(true);
	loop_checkbox->Enable(true);
	active_checkbox->Enable(true);
	npc_name_ctrl->Enable(true);

	const std::vector<NPCPath>& paths = map->npc_paths.getPaths();
	for(size_t i = 0; i < paths.size(); ++i) {
		const NPCPath& path = paths[i];
		long idx = path_list->InsertItem(static_cast<long>(i), wxstr(path.name));
		path_list->SetItem(idx, 1, wxstr(path.npc_name));
		path_list->SetItem(idx, 2, path.active ? "Yes" : "No");
	}

	std::string activeName = map->npc_paths.getActivePathName();
	if(activeName.empty() && !paths.empty()) {
		activeName = paths.front().name;
		map->npc_paths.setActivePath(activeName);
	}
	for(long i = 0; i < path_list->GetItemCount(); ++i) {
		if(nstr(path_list->GetItemText(i)) == activeName) {
			path_list->SetItemState(i, wxLIST_STATE_SELECTED, wxLIST_STATE_SELECTED);
			break;
		}
	}

	// Update path property controls based on active path
	NPCPath* activePath = GetActivePath();
	if(activePath) {
		loop_checkbox->SetValue(activePath->loop);
		active_checkbox->SetValue(activePath->active);
		npc_name_ctrl->ChangeValue(wxstr(activePath->npc_name));
	}
}

void NPCPathPalettePanel::RefreshWaypointList()
{
	waypoint_list->DeleteAllItems();

	if(!map) {
		waypoint_list->Enable(false);
		add_waypoint_button->Enable(false);
		remove_waypoint_button->Enable(false);
		clear_waypoints_button->Enable(false);
		waypoint_up_button->Enable(false);
		waypoint_down_button->Enable(false);
		apply_waypoint_props_button->Enable(false);
		return;
	}

	waypoint_list->Enable(true);
	add_waypoint_button->Enable(true);
	remove_waypoint_button->Enable(true);
	clear_waypoints_button->Enable(true);
	waypoint_up_button->Enable(true);
	waypoint_down_button->Enable(true);
	apply_waypoint_props_button->Enable(true);

	NPCPath* path = GetActivePath();
	if(!path) {
		waypoint_list->Enable(false);
		remove_waypoint_button->Enable(false);
		clear_waypoints_button->Enable(false);
		waypoint_up_button->Enable(false);
		waypoint_down_button->Enable(false);
		apply_waypoint_props_button->Enable(false);
		return;
	}

	for(size_t i = 0; i < path->waypoints.size(); ++i) {
		const NPCWaypoint& wp = path->waypoints[i];
		long idx = waypoint_list->InsertItem(static_cast<long>(i), wxString::Format("%d", static_cast<int>(i + 1)));
		waypoint_list->SetItem(idx, 1, wxString::Format("%d", wp.pos.x));
		waypoint_list->SetItem(idx, 2, wxString::Format("%d", wp.pos.y));
		waypoint_list->SetItem(idx, 3, wxString::Format("%d", wp.pos.z));
		waypoint_list->SetItem(idx, 4, wxString::Format("%.1f", wp.walk_speed));
		waypoint_list->SetItem(idx, 5, wxString::Format("%zu", wp.actions.size()));
	}

	int activeIndex = map->npc_paths.getActiveWaypoint();
	if(activeIndex >= 0 && activeIndex < static_cast<int>(path->waypoints.size())) {
		waypoint_list->SetItemState(activeIndex, wxLIST_STATE_SELECTED, wxLIST_STATE_SELECTED);
	}
}

void NPCPathPalettePanel::RefreshActionList()
{
	action_list->DeleteAllItems();

	if(!map) {
		action_list->Enable(false);
		add_action_button->Enable(false);
		remove_action_button->Enable(false);
		edit_action_button->Enable(false);
		apply_action_button->Enable(false);
		return;
	}

	NPCPath* path = GetActivePath();
	int waypointIndex = GetSelectedWaypointIndex();
	if(!path || waypointIndex < 0 || waypointIndex >= static_cast<int>(path->waypoints.size())) {
		action_list->Enable(false);
		add_action_button->Enable(false);
		remove_action_button->Enable(false);
		edit_action_button->Enable(false);
		apply_action_button->Enable(false);
		return;
	}

	action_list->Enable(true);
	add_action_button->Enable(true);
	remove_action_button->Enable(true);
	edit_action_button->Enable(true);
	apply_action_button->Enable(true);

	const NPCWaypoint& wp = path->waypoints[waypointIndex];
	for(size_t i = 0; i < wp.actions.size(); ++i) {
		const NPCAction& action = wp.actions[i];
		long idx = action_list->InsertItem(static_cast<long>(i), wxString::Format("%d", static_cast<int>(i + 1)));

		wxString typeStr;
		wxString detailsStr;
		switch(action.type) {
			case NPCActionType::None:
				typeStr = "None";
				break;
			case NPCActionType::Speak:
				typeStr = "Speak";
				detailsStr = wxstr(action.message);
				if(detailsStr.length() > 30) {
					detailsStr = detailsStr.Left(27) + "...";
				}
				break;
			case NPCActionType::Wait:
				typeStr = "Wait";
				detailsStr = wxString::Format("%.1fs", action.duration);
				break;
			case NPCActionType::FaceDirection:
				typeStr = "Face";
				switch(action.direction) {
					case 0: detailsStr = "North"; break;
					case 1: detailsStr = "East"; break;
					case 2: detailsStr = "South"; break;
					case 3: detailsStr = "West"; break;
					default: detailsStr = "Unknown"; break;
				}
				break;
			case NPCActionType::Emote:
				typeStr = "Emote";
				detailsStr = wxString::Format("ID: %d", action.emote_id);
				break;
		}

		action_list->SetItem(idx, 1, typeStr);
		action_list->SetItem(idx, 2, detailsStr);
	}
}

void NPCPathPalettePanel::UpdateWaypointControls()
{
	if(!map) {
		pos_label->SetLabel("Pos: - , -");
		return;
	}

	NPCPath* path = GetActivePath();
	if(!path) {
		pos_label->SetLabel("Pos: - , -");
		return;
	}

	std::vector<int> indices = GetSelectedWaypointIndices();
	if(indices.empty()) {
		pos_label->SetLabel("Pos: - , -");
		return;
	}

	if(indices.size() > 1) {
		pos_label->SetLabel(wxString::Format("Selected: %zu waypoints", indices.size()));
		return;
	}

	int index = indices[0];
	if(index < 0 || index >= static_cast<int>(path->waypoints.size())) {
		pos_label->SetLabel("Pos: - , -");
		return;
	}

	const NPCWaypoint& wp = path->waypoints[index];
	pos_label->SetLabel(wxString::Format("Pos: %d, %d, %d", wp.pos.x, wp.pos.y, wp.pos.z));
	walk_speed_ctrl->ChangeValue(wxString::Format("%.2f", wp.walk_speed));
	wait_before_ctrl->ChangeValue(wxString::Format("%.2f", wp.wait_before));
	wait_after_ctrl->ChangeValue(wxString::Format("%.2f", wp.wait_after));
}

void NPCPathPalettePanel::UpdateActionControls()
{
	int actionIndex = GetSelectedActionIndex();
	NPCPath* path = GetActivePath();
	int waypointIndex = GetSelectedWaypointIndex();

	// Enable/disable controls based on action type
	int actionType = action_type_ctrl->GetSelection();

	// Message is only for Speak
	action_message_ctrl->Enable(actionType == 1);
	// Duration is only for Wait
	action_duration_ctrl->Enable(actionType == 2);
	// Direction is only for FaceDirection
	action_direction_ctrl->Enable(actionType == 3);
	// Emote ID is only for Emote
	action_emote_ctrl->Enable(actionType == 4);

	// If we have a selected action, populate the fields
	if(path && waypointIndex >= 0 && waypointIndex < static_cast<int>(path->waypoints.size())) {
		const NPCWaypoint& wp = path->waypoints[waypointIndex];
		if(actionIndex >= 0 && actionIndex < static_cast<int>(wp.actions.size())) {
			const NPCAction& action = wp.actions[actionIndex];
			action_type_ctrl->SetSelection(static_cast<int>(action.type));
			action_message_ctrl->ChangeValue(wxstr(action.message));
			action_duration_ctrl->ChangeValue(wxString::Format("%.2f", action.duration));
			action_direction_ctrl->SetSelection(action.direction);
			action_emote_ctrl->ChangeValue(wxString::Format("%d", action.emote_id));

			// Re-update enable states based on actual action type
			action_message_ctrl->Enable(action.type == NPCActionType::Speak);
			action_duration_ctrl->Enable(action.type == NPCActionType::Wait);
			action_direction_ctrl->Enable(action.type == NPCActionType::FaceDirection);
			action_emote_ctrl->Enable(action.type == NPCActionType::Emote);
		}
	}
}

NPCPath* NPCPathPalettePanel::GetActivePath() const
{
	if(!map) {
		return nullptr;
	}
	return map->npc_paths.getActivePath();
}

int NPCPathPalettePanel::GetSelectedWaypointIndex() const
{
	long item = waypoint_list->GetNextItem(-1, wxLIST_NEXT_ALL, wxLIST_STATE_SELECTED);
	if(item != -1) {
		return static_cast<int>(item);
	}
	return map ? map->npc_paths.getActiveWaypoint() : -1;
}

std::vector<int> NPCPathPalettePanel::GetSelectedWaypointIndices() const
{
	std::vector<int> indices;
	long item = -1;
	while((item = waypoint_list->GetNextItem(item, wxLIST_NEXT_ALL, wxLIST_STATE_SELECTED)) != -1) {
		indices.push_back(static_cast<int>(item));
	}
	return indices;
}

int NPCPathPalettePanel::GetSelectedActionIndex() const
{
	long item = action_list->GetNextItem(-1, wxLIST_NEXT_ALL, wxLIST_STATE_SELECTED);
	return item != -1 ? static_cast<int>(item) : -1;
}

Position NPCPathPalettePanel::GetCursorPosition() const
{
	MapTab* tab = g_gui.GetCurrentMapTab();
	if(!tab) {
		return Position();
	}
	MapCanvas* canvas = tab->GetCanvas();
	if(canvas) {
		Position pos = canvas->GetCursorPosition();
		if(pos.isValid()) {
			return pos;
		}
	}
	return tab->GetScreenCenterPosition();
}

// ===== Path Event Handlers =====

void NPCPathPalettePanel::OnClickPath(wxListEvent& event)
{
	if(!map) {
		return;
	}

	std::string name = nstr(event.GetText());
	map->npc_paths.setActivePath(name);
	map->npc_paths.setActiveWaypoint(-1);

	// Update path property controls
	NPCPath* path = GetActivePath();
	if(path) {
		loop_checkbox->SetValue(path->loop);
		active_checkbox->SetValue(path->active);
		npc_name_ctrl->ChangeValue(wxstr(path->npc_name));
	}

	RefreshWaypointList();
	RefreshActionList();
	UpdateWaypointControls();
	UpdateActionControls();
	g_gui.RefreshView();
}

void NPCPathPalettePanel::OnBeginEditPathLabel(wxListEvent& event)
{
	g_gui.DisableHotkeys();
}

void NPCPathPalettePanel::OnEditPathLabel(wxListEvent& event)
{
	if(!map) {
		g_gui.EnableHotkeys();
		return;
	}

	if(event.IsEditCancelled()) {
		g_gui.EnableHotkeys();
		return;
	}

	std::string newName = nstr(event.GetLabel());
	std::string oldName = nstr(path_list->GetItemText(event.GetIndex()));
	if(newName.empty()) {
		event.Veto();
		g_gui.EnableHotkeys();
		return;
	}
	if(newName == oldName) {
		g_gui.EnableHotkeys();
		return;
	}

	NPCPaths temp = map->npc_paths;
	NPCPath* path = temp.getPath(oldName);
	if(!path) {
		g_gui.EnableHotkeys();
		return;
	}

	if(temp.getPath(newName)) {
		event.Veto();
		g_gui.EnableHotkeys();
		return;
	}
	path->name = newName;
	temp.setActivePath(newName);

	Editor* editor = g_gui.GetCurrentEditor();
	if(editor) {
		editor->ApplyNPCPathsSnapshot(temp.snapshot(), ACTION_DRAW);
		editor->resetActionsTimer();
		editor->updateActions();
	}

	g_gui.EnableHotkeys();
}

void NPCPathPalettePanel::OnAddPath(wxCommandEvent& WXUNUSED(event))
{
	if(!map) {
		return;
	}

	NPCPaths temp = map->npc_paths;
	temp.addPath("NPC Path");

	Editor* editor = g_gui.GetCurrentEditor();
	if(editor) {
		editor->ApplyNPCPathsSnapshot(temp.snapshot(), ACTION_DRAW);
		editor->resetActionsTimer();
		editor->updateActions();
	}
}

void NPCPathPalettePanel::OnRemovePath(wxCommandEvent& WXUNUSED(event))
{
	if(!map) {
		return;
	}

	long item = path_list->GetNextItem(-1, wxLIST_NEXT_ALL, wxLIST_STATE_SELECTED);
	if(item == -1) {
		return;
	}

	std::string name = nstr(path_list->GetItemText(item));
	NPCPaths temp = map->npc_paths;
	if(!temp.removePath(name)) {
		return;
	}

	Editor* editor = g_gui.GetCurrentEditor();
	if(editor) {
		editor->ApplyNPCPathsSnapshot(temp.snapshot(), ACTION_DRAW);
		editor->resetActionsTimer();
		editor->updateActions();
	}
}

void NPCPathPalettePanel::OnToggleLoop(wxCommandEvent& WXUNUSED(event))
{
	if(!map) {
		return;
	}

	NPCPaths temp = map->npc_paths;
	NPCPath* path = temp.getActivePath();
	if(!path) {
		return;
	}
	path->loop = loop_checkbox->GetValue();

	Editor* editor = g_gui.GetCurrentEditor();
	if(editor) {
		editor->ApplyNPCPathsSnapshot(temp.snapshot(), ACTION_DRAW);
		editor->resetActionsTimer();
		editor->updateActions();
	}
}

void NPCPathPalettePanel::OnToggleActive(wxCommandEvent& WXUNUSED(event))
{
	if(!map) {
		return;
	}

	NPCPaths temp = map->npc_paths;
	NPCPath* path = temp.getActivePath();
	if(!path) {
		return;
	}
	path->active = active_checkbox->GetValue();

	Editor* editor = g_gui.GetCurrentEditor();
	if(editor) {
		editor->ApplyNPCPathsSnapshot(temp.snapshot(), ACTION_DRAW);
		editor->resetActionsTimer();
		editor->updateActions();
	}
}

void NPCPathPalettePanel::OnNPCNameChanged(wxCommandEvent& WXUNUSED(event))
{
	if(!map) {
		return;
	}

	NPCPaths temp = map->npc_paths;
	NPCPath* path = temp.getActivePath();
	if(!path) {
		return;
	}
	path->npc_name = nstr(npc_name_ctrl->GetValue());

	Editor* editor = g_gui.GetCurrentEditor();
	if(editor) {
		editor->ApplyNPCPathsSnapshot(temp.snapshot(), ACTION_DRAW);
		editor->resetActionsTimer();
		editor->updateActions();
	}
}

// ===== Waypoint Event Handlers =====

void NPCPathPalettePanel::OnClickWaypoint(wxListEvent& event)
{
	if(!map) {
		return;
	}

	int index = static_cast<int>(event.GetIndex());
	map->npc_paths.setActiveWaypoint(index);
	UpdateWaypointControls();
	RefreshActionList();
	UpdateActionControls();

	// Center on waypoint if single selection
	std::vector<int> indices = GetSelectedWaypointIndices();
	if(indices.size() == 1) {
		if(NPCPath* path = GetActivePath()) {
			if(index >= 0 && index < static_cast<int>(path->waypoints.size())) {
				g_gui.SetScreenCenterPosition(path->waypoints[index].pos);
			}
		}
	}
	g_gui.RefreshView();
}

void NPCPathPalettePanel::OnDeselectWaypoint(wxListEvent& WXUNUSED(event))
{
	UpdateWaypointControls();
	RefreshActionList();
	UpdateActionControls();
	g_gui.RefreshView();
}

void NPCPathPalettePanel::OnAddWaypoint(wxCommandEvent& WXUNUSED(event))
{
	if(!map) {
		return;
	}

	NPCPaths temp = map->npc_paths;
	NPCPath* path = temp.getActivePath();
	if(!path) {
		path = temp.addPath("NPC Path");
	}

	Position pos = GetCursorPosition();
	if(!pos.isValid()) {
		return;
	}

	double walkSpeed = GetWaypointWalkSpeed();
	double waitBefore = GetWaypointWaitBefore();
	double waitAfter = GetWaypointWaitAfter();

	NPCWaypoint wp;
	wp.pos = pos;
	wp.walk_speed = walkSpeed;
	wp.wait_before = waitBefore;
	wp.wait_after = waitAfter;

	int insertIndex = static_cast<int>(path->waypoints.size());
	int activeIndex = temp.getActiveWaypoint();
	if(activeIndex >= 0 && activeIndex < static_cast<int>(path->waypoints.size())) {
		insertIndex = activeIndex + 1;
	}
	path->waypoints.insert(path->waypoints.begin() + insertIndex, wp);
	temp.setActiveWaypoint(insertIndex);

	Editor* editor = g_gui.GetCurrentEditor();
	if(editor) {
		editor->ApplyNPCPathsSnapshot(temp.snapshot(), ACTION_DRAW);
		editor->resetActionsTimer();
		editor->updateActions();
	}
}

void NPCPathPalettePanel::OnRemoveWaypoint(wxCommandEvent& WXUNUSED(event))
{
	if(!map) {
		return;
	}

	NPCPaths temp = map->npc_paths;
	NPCPath* path = temp.getActivePath();
	if(!path) {
		return;
	}

	int index = GetSelectedWaypointIndex();
	if(index < 0 || index >= static_cast<int>(path->waypoints.size())) {
		return;
	}

	path->waypoints.erase(path->waypoints.begin() + index);
	if(path->waypoints.empty()) {
		temp.setActiveWaypoint(-1);
	} else {
		temp.setActiveWaypoint(0);
	}

	Editor* editor = g_gui.GetCurrentEditor();
	if(editor) {
		editor->ApplyNPCPathsSnapshot(temp.snapshot(), ACTION_DRAW);
		editor->resetActionsTimer();
		editor->updateActions();
	}
}

void NPCPathPalettePanel::OnClearWaypoints(wxCommandEvent& WXUNUSED(event))
{
	if(!map) {
		return;
	}

	NPCPaths temp = map->npc_paths;
	NPCPath* path = temp.getActivePath();
	if(!path || path->waypoints.empty()) {
		return;
	}

	path->waypoints.clear();
	temp.setActiveWaypoint(-1);

	Editor* editor = g_gui.GetCurrentEditor();
	if(editor) {
		editor->ApplyNPCPathsSnapshot(temp.snapshot(), ACTION_DRAW);
		editor->resetActionsTimer();
		editor->updateActions();
	}
}

void NPCPathPalettePanel::OnWaypointUp(wxCommandEvent& WXUNUSED(event))
{
	if(!map) {
		return;
	}

	NPCPaths temp = map->npc_paths;
	NPCPath* path = temp.getActivePath();
	if(!path) {
		return;
	}

	int index = GetSelectedWaypointIndex();
	if(index <= 0 || index >= static_cast<int>(path->waypoints.size())) {
		return;
	}

	std::swap(path->waypoints[index], path->waypoints[index - 1]);
	temp.setActiveWaypoint(index - 1);

	Editor* editor = g_gui.GetCurrentEditor();
	if(editor) {
		editor->ApplyNPCPathsSnapshot(temp.snapshot(), ACTION_DRAW);
		editor->resetActionsTimer();
		editor->updateActions();
	}
}

void NPCPathPalettePanel::OnWaypointDown(wxCommandEvent& WXUNUSED(event))
{
	if(!map) {
		return;
	}

	NPCPaths temp = map->npc_paths;
	NPCPath* path = temp.getActivePath();
	if(!path) {
		return;
	}

	int index = GetSelectedWaypointIndex();
	if(index < 0 || index >= static_cast<int>(path->waypoints.size()) - 1) {
		return;
	}

	std::swap(path->waypoints[index], path->waypoints[index + 1]);
	temp.setActiveWaypoint(index + 1);

	Editor* editor = g_gui.GetCurrentEditor();
	if(editor) {
		editor->ApplyNPCPathsSnapshot(temp.snapshot(), ACTION_DRAW);
		editor->resetActionsTimer();
		editor->updateActions();
	}
}

void NPCPathPalettePanel::OnApplyWaypointProps(wxCommandEvent& WXUNUSED(event))
{
	if(!map) {
		return;
	}

	NPCPaths temp = map->npc_paths;
	NPCPath* path = temp.getActivePath();
	if(!path) {
		return;
	}

	std::vector<int> indices = GetSelectedWaypointIndices();
	if(indices.empty()) {
		return;
	}

	double walkSpeed = GetWaypointWalkSpeed();
	double waitBefore = GetWaypointWaitBefore();
	double waitAfter = GetWaypointWaitAfter();

	for(int index : indices) {
		if(index >= 0 && index < static_cast<int>(path->waypoints.size())) {
			NPCWaypoint& wp = path->waypoints[index];
			wp.walk_speed = walkSpeed;
			wp.wait_before = waitBefore;
			wp.wait_after = waitAfter;
		}
	}

	Editor* editor = g_gui.GetCurrentEditor();
	if(editor) {
		editor->ApplyNPCPathsSnapshot(temp.snapshot(), ACTION_DRAW);
		editor->resetActionsTimer();
		editor->updateActions();
	}
}

// ===== Action Event Handlers =====

void NPCPathPalettePanel::OnClickAction(wxListEvent& event)
{
	UpdateActionControls();
}

void NPCPathPalettePanel::OnAddAction(wxCommandEvent& WXUNUSED(event))
{
	if(!map) {
		return;
	}

	NPCPaths temp = map->npc_paths;
	NPCPath* path = temp.getActivePath();
	int waypointIndex = GetSelectedWaypointIndex();
	if(!path || waypointIndex < 0 || waypointIndex >= static_cast<int>(path->waypoints.size())) {
		return;
	}

	NPCAction action;
	action.type = static_cast<NPCActionType>(action_type_ctrl->GetSelection());
	action.message = nstr(action_message_ctrl->GetValue());
	action_duration_ctrl->GetValue().ToDouble(&action.duration);
	action.direction = action_direction_ctrl->GetSelection();
	long emoteId = 0;
	action_emote_ctrl->GetValue().ToLong(&emoteId);
	action.emote_id = static_cast<int>(emoteId);

	path->waypoints[waypointIndex].actions.push_back(action);

	Editor* editor = g_gui.GetCurrentEditor();
	if(editor) {
		editor->ApplyNPCPathsSnapshot(temp.snapshot(), ACTION_DRAW);
		editor->resetActionsTimer();
		editor->updateActions();
	}
}

void NPCPathPalettePanel::OnRemoveAction(wxCommandEvent& WXUNUSED(event))
{
	if(!map) {
		return;
	}

	NPCPaths temp = map->npc_paths;
	NPCPath* path = temp.getActivePath();
	int waypointIndex = GetSelectedWaypointIndex();
	int actionIndex = GetSelectedActionIndex();
	if(!path || waypointIndex < 0 || waypointIndex >= static_cast<int>(path->waypoints.size())) {
		return;
	}
	if(actionIndex < 0 || actionIndex >= static_cast<int>(path->waypoints[waypointIndex].actions.size())) {
		return;
	}

	path->waypoints[waypointIndex].actions.erase(
		path->waypoints[waypointIndex].actions.begin() + actionIndex
	);

	Editor* editor = g_gui.GetCurrentEditor();
	if(editor) {
		editor->ApplyNPCPathsSnapshot(temp.snapshot(), ACTION_DRAW);
		editor->resetActionsTimer();
		editor->updateActions();
	}
}

void NPCPathPalettePanel::OnEditAction(wxCommandEvent& WXUNUSED(event))
{
	// Load action into controls for editing
	NPCPath* path = GetActivePath();
	int waypointIndex = GetSelectedWaypointIndex();
	int actionIndex = GetSelectedActionIndex();
	if(!path || waypointIndex < 0 || waypointIndex >= static_cast<int>(path->waypoints.size())) {
		return;
	}
	if(actionIndex < 0 || actionIndex >= static_cast<int>(path->waypoints[waypointIndex].actions.size())) {
		return;
	}

	const NPCAction& action = path->waypoints[waypointIndex].actions[actionIndex];
	action_type_ctrl->SetSelection(static_cast<int>(action.type));
	action_message_ctrl->ChangeValue(wxstr(action.message));
	action_duration_ctrl->ChangeValue(wxString::Format("%.2f", action.duration));
	action_direction_ctrl->SetSelection(action.direction);
	action_emote_ctrl->ChangeValue(wxString::Format("%d", action.emote_id));

	UpdateActionControls();
}

void NPCPathPalettePanel::OnApplyAction(wxCommandEvent& WXUNUSED(event))
{
	if(!map) {
		return;
	}

	NPCPaths temp = map->npc_paths;
	NPCPath* path = temp.getActivePath();
	int waypointIndex = GetSelectedWaypointIndex();
	int actionIndex = GetSelectedActionIndex();
	if(!path || waypointIndex < 0 || waypointIndex >= static_cast<int>(path->waypoints.size())) {
		return;
	}
	if(actionIndex < 0 || actionIndex >= static_cast<int>(path->waypoints[waypointIndex].actions.size())) {
		return;
	}

	NPCAction& action = path->waypoints[waypointIndex].actions[actionIndex];
	action.type = static_cast<NPCActionType>(action_type_ctrl->GetSelection());
	action.message = nstr(action_message_ctrl->GetValue());
	action_duration_ctrl->GetValue().ToDouble(&action.duration);
	action.direction = action_direction_ctrl->GetSelection();
	long emoteId = 0;
	action_emote_ctrl->GetValue().ToLong(&emoteId);
	action.emote_id = static_cast<int>(emoteId);

	Editor* editor = g_gui.GetCurrentEditor();
	if(editor) {
		editor->ApplyNPCPathsSnapshot(temp.snapshot(), ACTION_DRAW);
		editor->resetActionsTimer();
		editor->updateActions();
	}
}

void NPCPathPalettePanel::OnActionTypeChanged(wxCommandEvent& WXUNUSED(event))
{
	UpdateActionControls();
}

// ===== Preview and Display Event Handlers =====

void NPCPathPalettePanel::OnPreviewPath(wxCommandEvent& WXUNUSED(event))
{
	// Preview the NPC path animation (placeholder for future implementation)
	// For now, just center on the first waypoint of the active path
	NPCPath* path = GetActivePath();
	if(path && !path->waypoints.empty()) {
		g_gui.SetScreenCenterPosition(path->waypoints[0].pos);
		g_gui.RefreshView();
	}
}

void NPCPathPalettePanel::OnToggleShowPaths(wxCommandEvent& WXUNUSED(event))
{
	bool show = show_paths_checkbox->GetValue();
	g_settings.setInteger(Config::SHOW_NPC_PATHS, show ? 1 : 0);
	g_gui.UpdateMenubar();
	g_gui.RefreshView();
}
