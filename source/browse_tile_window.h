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

#ifndef RME_BROWSE_TILE_WINDOW_H_
#define RME_BROWSE_TILE_WINDOW_H_

#include "main.h"
#include "map.h"
#include "tile.h"

#include <wx/timer.h>

class BrowseTileListBox;
class Editor;

class BrowseTileWindow : public wxDialog
{
public:
	BrowseTileWindow(wxWindow* parent, Tile* tile, wxPoint position = wxDefaultPosition);
	~BrowseTileWindow();

	void OnItemSelected(wxCommandEvent&);
	void OnClickDelete(wxCommandEvent&);
	void OnClickSelectRaw(wxCommandEvent&);
	void OnClickOK(wxCommandEvent&);
	void OnClickCancel(wxCommandEvent&);
	void OnClickMoveUp(wxCommandEvent&);
	void OnClickMoveDown(wxCommandEvent&);
	void OnToggleAutoApply(wxCommandEvent&);

protected:
	void TriggerAutoApply();

	BrowseTileListBox* item_list;
	wxStaticText* item_count_txt;
	wxButton* delete_button;
	wxButton* select_raw_button;
	wxButton* moveup_button;
	wxButton* movedown_button;
	wxCheckBox* auto_apply_checkbox;
	Tile* edit_tile;
	bool auto_apply_enabled;

	DECLARE_EVENT_TABLE();
};

class BrowseTilePanel : public wxPanel
{
public:
	BrowseTilePanel(wxWindow* parent);
	~BrowseTilePanel();

	void LoadSelectionFromEditor();
	void ClearSelection();

private:
	void OnItemSelected(wxCommandEvent&);
	void OnClickDelete(wxCommandEvent&);
	void OnClickSelectRaw(wxCommandEvent&);
	void OnClickMoveUp(wxCommandEvent&);
	void OnClickMoveDown(wxCommandEvent&);
	void OnClickApply(wxCommandEvent&);
	void OnClickLoadSelection(wxCommandEvent&);
	void OnToggleAutoApply(wxCommandEvent&);
	void OnToggleAutoLoad(wxCommandEvent&);
	void OnAutoLoadTimer(wxTimerEvent&);

	void UpdateTileInfo();
	void UpdateControlStates();
	void ResetTile(Tile* new_tile);
	void RefreshFromSourceTile();
	bool HasTile() const { return working_tile != nullptr; }

	BrowseTileListBox* item_list;
	wxStaticText* item_count_txt;
	wxStaticText* position_txt;
	wxStaticText* protection_txt;
	wxStaticText* nopvp_txt;
	wxStaticText* nologout_txt;
	wxStaticText* pvpzone_txt;
	wxStaticText* worldboss_txt;
	wxStaticText* house_txt;
	wxButton* delete_button;
	wxButton* select_raw_button;
	wxButton* moveup_button;
	wxButton* movedown_button;
	wxButton* apply_button;
	wxButton* load_selection_button;
	wxCheckBox* auto_load_checkbox;
	wxCheckBox* auto_apply_checkbox;

	Tile* working_tile;
	Editor* source_editor;
	Position working_position;
	bool working_position_valid;

	wxTimer* auto_load_timer;
	bool auto_load_enabled;
	bool auto_apply_enabled;

	void TriggerAutoApply();
};

#endif

