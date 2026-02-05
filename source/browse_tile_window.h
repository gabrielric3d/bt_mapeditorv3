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
#include <wx/notebook.h>
#include <wx/srchctrl.h>
#include <set>

class BrowseTileListBox;
class SelectionItemListBox;
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

	void SetTile(Tile* tile, Editor* editor);
	void ClearSelection();

private:
	void OnItemSelected(wxCommandEvent&);
	void OnClickDelete(wxCommandEvent&);
	void OnClickSelectRaw(wxCommandEvent&);
	void OnClickMoveUp(wxCommandEvent&);
	void OnClickMoveDown(wxCommandEvent&);
	void OnClickApply(wxCommandEvent&);
	void OnToggleAutoApply(wxCommandEvent&);

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
	wxCheckBox* auto_apply_checkbox;

	Tile* working_tile;
	Editor* source_editor;
	Position working_position;
	bool working_position_valid;

	bool auto_apply_enabled;

	void TriggerAutoApply();
};

// ============================================================================
// SelectionItemListBox - Custom list box for displaying items to manipulate in selection
class SelectionItemListBox : public wxVListBox
{
public:
	SelectionItemListBox(wxWindow* parent, wxWindowID id);
	~SelectionItemListBox();

	void OnDrawItem(wxDC& dc, const wxRect& rect, size_t index) const;
	wxCoord OnMeasureItem(size_t index) const;

	void AddItem(uint16_t itemId);
	void RemoveSelected();
	void MoveSelectedUp();
	void MoveSelectedDown();
	void Clear();

	uint16_t GetSelectedItemId() const;
	const std::vector<uint16_t>& GetItemIds() const { return item_ids; }

protected:
	void UpdateList();

	std::vector<uint16_t> item_ids;
};

// ============================================================================
// ApplySelectionPanel - Panel for manipulating items across multiple tiles
class ApplySelectionPanel : public wxPanel
{
public:
	ApplySelectionPanel(wxWindow* parent);
	~ApplySelectionPanel();

	void RefreshSelectionInfo();
	void LoadItemsFromSelection();

private:
	void OnSearchChanged(wxCommandEvent& event);
	void OnSearchResultSelected(wxCommandEvent& event);
	void OnAddItem(wxCommandEvent& event);
	void OnAddRange(wxCommandEvent& event);
	void OnRemoveItem(wxCommandEvent& event);
	void OnMoveUp(wxCommandEvent& event);
	void OnMoveDown(wxCommandEvent& event);
	void OnClear(wxCommandEvent& event);
	void OnLoadSelection(wxCommandEvent& event);
	void OnItemListSelected(wxCommandEvent& event);

	void PopulateSearchResults();
	void UpdateButtonStates();
	void MoveItemsInSelection(bool moveUp);

	// Search panel
	wxSearchCtrl* search_ctrl;
	wxListBox* search_results;
	wxButton* add_button;

	// Range input
	wxTextCtrl* from_id_ctrl;
	wxTextCtrl* to_id_ctrl;
	wxButton* add_range_button;

	// Item manipulation list
	SelectionItemListBox* item_list;
	wxButton* remove_button;
	wxButton* moveup_button;
	wxButton* movedown_button;

	// Action buttons
	wxButton* clear_button;
	wxCheckBox* load_from_selection_checkbox;
	wxButton* load_selection_button;

	// Status
	wxStaticText* status_label;
	wxStaticText* selection_info_label;

	// Search state
	std::vector<uint16_t> filtered_items;
};

// ============================================================================
// BrowseFieldNotebook - Container with tabs for Browse Tile and Apply Selection
class BrowseFieldNotebook : public wxPanel
{
public:
	BrowseFieldNotebook(wxWindow* parent);
	~BrowseFieldNotebook();

	BrowseTilePanel* GetBrowseTilePanel() { return browse_tile_panel; }
	ApplySelectionPanel* GetApplySelectionPanel() { return apply_selection_panel; }

	void OnPageChanged(wxBookCtrlEvent& event);

private:
	wxNotebook* notebook;
	BrowseTilePanel* browse_tile_panel;
	ApplySelectionPanel* apply_selection_panel;
};

#endif

