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


#ifndef RME_TILESET_CREATURE_H_
#define RME_TILESET_CREATURE_H_

#include "palette_common.h"
#include <string>
#include <vector>

class CreaturePaletteListBox;

class CreaturePalettePanel : public PalettePanel {
public:
	CreaturePalettePanel(wxWindow* parent, wxWindowID id = wxID_ANY);
	virtual ~CreaturePalettePanel();

	PaletteType GetType() const;

	// Select the first brush
	void SelectFirstBrush();
	// Returns the currently selected brush (first brush if panel is not loaded)
	Brush* GetSelectedBrush() const;
	// Returns the currently selected brush size
	int GetSelectedBrushSize() const;
	// Select the brush in the parameter, this only changes the look of the panel
	bool SelectBrush(const Brush* whatbrush);

	// Updates the palette window to use the current brush size
	void OnUpdateBrushSize(BrushShape shape, int size);
	// Called when this page is displayed
	void OnSwitchIn();
	// Called sometimes?
	void OnUpdate();

protected:
	void SelectTileset(size_t index);
	void SelectCreature(size_t index);
	void SelectCreature(std::string name);
public:
	// Event handling
	void OnChangeSpawnTime(wxSpinEvent& event);
	void OnChangeSpawnSize(wxSpinEvent& event);

	void OnTilesetChange(wxCommandEvent& event);
	void OnFilterTextChange(wxCommandEvent& event);
	void OnListBoxChange(wxCommandEvent& event);
	void OnClickCreatureBrushButton(wxCommandEvent& event);
	void OnClickSpawnBrushButton(wxCommandEvent& event);
	void OnClickGroupAdd(wxCommandEvent& event);
	void OnClickGroupRemove(wxCommandEvent& event);
	void OnClickGroupClear(wxCommandEvent& event);
	void OnGroupListChange(wxCommandEvent& event);
	void OnGroupListDoubleClick(wxCommandEvent& event);
	void OnFavoriteListChange(wxCommandEvent& event);
	void OnFavoriteListDoubleClick(wxCommandEvent& event);
	void OnClickFavoriteAdd(wxCommandEvent& event);
	void OnClickFavoriteRemove(wxCommandEvent& event);
	void OnClickPreviousPage(wxCommandEvent& event);
	void OnClickNextPage(wxCommandEvent& event);
	void OnToggleCreaturePreview(wxCommandEvent& event);
protected:
	void SelectCreatureBrush();
	void SelectSpawnBrush();
	void SyncSpawnGroupToGUI();
	void UpdateSpawnGroupList();
	void ApplyCreatureFilter(bool preserve_selection);
	void UpdateCreaturePage(const std::string& preferred_selection = std::string());
	void UpdateCreaturePaginationControls();
	Brush* GetSelectedCreatureBrush() const;
	size_t GetCreaturePageCount() const;
	void LoadFavoriteCreaturesFromSettings();
	void SaveFavoriteCreaturesToSettings() const;
	void UpdateFavoriteList(const std::string& preferred_selection = std::string());
	void AddFavoriteCreature(const std::string& name);

	struct SpawnGroupEntry {
		std::string name;
		int count;
	};

	wxChoice* tileset_choice;
	KeyForwardingTextCtrl* creature_filter_text;
	CreaturePaletteListBox* creature_list;
	wxCheckBox* creature_preview_checkbox;
	wxButton* creature_page_previous_button;
	wxButton* creature_page_next_button;
	wxStaticText* creature_page_label;
	wxToggleButton* creature_brush_button;
	wxToggleButton* spawn_brush_button;
	wxSpinCtrl* creature_spawntime_spin;
	wxSpinCtrl* spawn_size_spin;
	wxListBox* favorite_creature_list;
	wxButton* favorite_creature_add_button;
	wxButton* favorite_creature_remove_button;
	wxListBox* spawn_group_list;
	wxSpinCtrl* spawn_group_count_spin;
	wxButton* spawn_group_add_button;
	wxButton* spawn_group_remove_button;
	wxButton* spawn_group_clear_button;

	bool handling_event;
	bool prefer_favorite_for_group_add;
	std::vector<SpawnGroupEntry> spawn_group;
	std::vector<Brush*> creature_brushes;
	std::vector<Brush*> filtered_creature_brushes;
	std::vector<std::string> favorite_creatures;
	size_t current_creature_page;

	static const size_t CREATURES_PER_PAGE = 64;

	DECLARE_EVENT_TABLE();
};

#endif
