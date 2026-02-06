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

#ifndef RME_PALETTE_NPC_PATHS_H_
#define RME_PALETTE_NPC_PATHS_H_

#include <wx/listctrl.h>
#include <wx/textctrl.h>
#include <wx/checkbox.h>
#include <wx/choice.h>

#include <vector>

#include "palette_common.h"
#include "npc_path.h"

class NPCPathPalettePanel : public PalettePanel {
public:
	NPCPathPalettePanel(wxWindow* parent, wxWindowID id = wxID_ANY);
	~NPCPathPalettePanel();

	wxString GetName() const;
	PaletteType GetType() const;

	Brush* GetSelectedBrush() const;
	int GetSelectedBrushSize() const;
	bool SelectBrush(const Brush* whatbrush);

	void OnUpdate();
	void OnSwitchIn();
	void OnSwitchOut();

	void SetMap(Map* map);

	// Getters for waypoint properties from UI fields
	double GetWaypointWalkSpeed() const;
	double GetWaypointWaitBefore() const;
	double GetWaypointWaitAfter() const;

	// Path event handlers
	void OnClickPath(wxListEvent& event);
	void OnBeginEditPathLabel(wxListEvent& event);
	void OnEditPathLabel(wxListEvent& event);
	void OnAddPath(wxCommandEvent& event);
	void OnRemovePath(wxCommandEvent& event);
	void OnToggleLoop(wxCommandEvent& event);
	void OnToggleActive(wxCommandEvent& event);
	void OnNPCNameChanged(wxCommandEvent& event);

	// Waypoint event handlers
	void OnClickWaypoint(wxListEvent& event);
	void OnDeselectWaypoint(wxListEvent& event);
	void OnAddWaypoint(wxCommandEvent& event);
	void OnRemoveWaypoint(wxCommandEvent& event);
	void OnClearWaypoints(wxCommandEvent& event);
	void OnWaypointUp(wxCommandEvent& event);
	void OnWaypointDown(wxCommandEvent& event);
	void OnApplyWaypointProps(wxCommandEvent& event);

	// Action event handlers
	void OnClickAction(wxListEvent& event);
	void OnAddAction(wxCommandEvent& event);
	void OnRemoveAction(wxCommandEvent& event);
	void OnEditAction(wxCommandEvent& event);
	void OnApplyAction(wxCommandEvent& event);
	void OnActionTypeChanged(wxCommandEvent& event);

	// Preview and display event handlers
	void OnPreviewPath(wxCommandEvent& event);
	void OnToggleShowPaths(wxCommandEvent& event);

protected:
	void RefreshPathList();
	void RefreshWaypointList();
	void RefreshActionList();
	void UpdateWaypointControls();
	void UpdateActionControls();
	NPCPath* GetActivePath() const;
	int GetSelectedWaypointIndex() const;
	std::vector<int> GetSelectedWaypointIndices() const;
	int GetSelectedActionIndex() const;
	Position GetCursorPosition() const;

	Map* map;

	// Path list controls
	wxListCtrl* path_list;
	wxButton* add_path_button;
	wxButton* remove_path_button;
	wxCheckBox* loop_checkbox;
	wxCheckBox* active_checkbox;
	wxTextCtrl* npc_name_ctrl;

	// Waypoint list controls
	wxListCtrl* waypoint_list;
	wxButton* add_waypoint_button;
	wxButton* remove_waypoint_button;
	wxButton* clear_waypoints_button;
	wxButton* waypoint_up_button;
	wxButton* waypoint_down_button;
	wxButton* apply_waypoint_props_button;

	// Waypoint property controls
	wxStaticText* pos_label;
	wxTextCtrl* walk_speed_ctrl;
	wxTextCtrl* wait_before_ctrl;
	wxTextCtrl* wait_after_ctrl;

	// Action list controls
	wxListCtrl* action_list;
	wxButton* add_action_button;
	wxButton* remove_action_button;
	wxButton* edit_action_button;
	wxButton* apply_action_button;

	// Action property controls
	wxChoice* action_type_ctrl;
	wxTextCtrl* action_message_ctrl;
	wxTextCtrl* action_duration_ctrl;
	wxChoice* action_direction_ctrl;
	wxTextCtrl* action_emote_ctrl;

	// Preview controls
	wxButton* preview_path_button;
	wxCheckBox* show_paths_checkbox;

	DECLARE_EVENT_TABLE()
};

#endif
