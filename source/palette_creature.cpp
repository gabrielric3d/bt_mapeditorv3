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

#include "settings.h"
#include "brush.h"
#include "gui.h"
#include "palette_creature.h"
#include "creature_brush.h"
#include "spawn_brush.h"
#include "materials.h"
#include "creatures.h"
#include <algorithm>
#include <sstream>
#include <wx/numdlg.h>
#include <wx/settings.h>
#include <wx/vlbox.h>

class CreaturePaletteListBox : public wxVListBox {
public:
	CreaturePaletteListBox(wxWindow* parent, wxWindowID id) :
		wxVListBox(parent, id, wxDefaultPosition, wxDefaultSize, wxLB_SINGLE),
		show_preview(false)
	{
		SetItemCount(0);
	}

	void SetBrushes(const std::vector<Brush*>& list)
	{
		brushes = list;
		SetItemCount(brushes.size());
		if(GetSelection() >= static_cast<int>(brushes.size())) {
			SetSelection(wxNOT_FOUND);
		}
		RefreshAll();
	}

	size_t GetCount() const
	{
		return brushes.size();
	}

	Brush* GetBrush(size_t index) const
	{
		if(index >= brushes.size()) {
			return nullptr;
		}
		return brushes[index];
	}

	Brush* GetSelectedBrush() const
	{
		const int selection = GetSelection();
		if(selection == wxNOT_FOUND) {
			return nullptr;
		}
		return GetBrush(static_cast<size_t>(selection));
	}

	bool SetSelectionByName(const std::string& name)
	{
		for(size_t i = 0; i < brushes.size(); ++i) {
			if(brushes[i] && brushes[i]->getName() == name) {
				SetSelection(static_cast<int>(i));
				return true;
			}
		}
		return false;
	}

	void SetShowPreview(bool enabled)
	{
		if(show_preview == enabled) {
			return;
		}
		show_preview = enabled;
		SetItemCount(brushes.size());
		RefreshAll();
	}

	bool IsShowingPreview() const
	{
		return show_preview;
	}

protected:
	void OnDrawItem(wxDC& dc, const wxRect& rect, size_t n) const override
	{
		if(n >= brushes.size()) {
			return;
		}

		Brush* brush = brushes[n];
		if(!brush) {
			return;
		}

		const wxColour text_colour = IsSelected(n) ? wxSystemSettings::GetColour(wxSYS_COLOUR_HIGHLIGHTTEXT)
		                                           : GetForegroundColour();
		dc.SetTextForeground(text_colour);

		if(!show_preview) {
			dc.DrawText(wxstr(brush->getName()), rect.GetX() + 4, rect.GetY() + 2);
			return;
		}

		const int icon_padding = 4;
		const int icon_size = std::min(rect.GetHeight(), 32);
		const int icon_x = rect.GetX() + icon_padding;
		const int icon_y = rect.GetY() + (rect.GetHeight() - icon_size) / 2;
		const wxRect icon_rect(icon_x, icon_y, icon_size, icon_size);

		Sprite* sprite = g_gui.gfx.getSprite(brush->getLookID());
		if(sprite) {
			sprite->DrawTo(&dc, SPRITE_SIZE_32x32, icon_rect.GetX(), icon_rect.GetY(), icon_rect.GetWidth(), icon_rect.GetHeight());
		} else {
			CreatureType* creature_type = g_creatures[brush->getName()];
			if(creature_type) {
				if(auto creature_sprite = g_gui.gfx.getCreatureSprite(creature_type->outfit.lookType)) {
					creature_sprite->DrawTo(&dc, icon_rect, creature_type->outfit);
				}
			}
		}

		const int text_x = icon_rect.GetRight() + icon_padding;
		dc.DrawText(wxstr(brush->getName()), text_x, rect.GetY() + 6);
	}

	wxCoord OnMeasureItem(size_t WXUNUSED(n)) const override
	{
		return show_preview ? 32 : 20;
	}

private:
	bool show_preview;
	std::vector<Brush*> brushes;
};

namespace {

bool CreatureNameLess(const Brush* lhs, const Brush* rhs)
{
	if(!lhs || !rhs) {
		return lhs < rhs;
	}
	return as_lower_str(lhs->getName()) < as_lower_str(rhs->getName());
}

} // namespace

// ============================================================================
// Creature palette

BEGIN_EVENT_TABLE(CreaturePalettePanel, PalettePanel)
	EVT_CHOICE(PALETTE_CREATURE_TILESET_CHOICE, CreaturePalettePanel::OnTilesetChange)

	EVT_TEXT(PALETTE_CREATURE_FILTER, CreaturePalettePanel::OnFilterTextChange)
	EVT_LISTBOX(PALETTE_CREATURE_LISTBOX, CreaturePalettePanel::OnListBoxChange)
	EVT_LISTBOX(PALETTE_CREATURE_FAVORITES_LIST, CreaturePalettePanel::OnFavoriteListChange)
	EVT_LISTBOX_DCLICK(PALETTE_CREATURE_FAVORITES_LIST, CreaturePalettePanel::OnFavoriteListDoubleClick)
	EVT_LISTBOX(PALETTE_CREATURE_GROUP_LIST, CreaturePalettePanel::OnGroupListChange)
	EVT_LISTBOX_DCLICK(PALETTE_CREATURE_GROUP_LIST, CreaturePalettePanel::OnGroupListDoubleClick)
	EVT_BUTTON(PALETTE_CREATURE_PAGE_PREVIOUS, CreaturePalettePanel::OnClickPreviousPage)
	EVT_BUTTON(PALETTE_CREATURE_PAGE_NEXT, CreaturePalettePanel::OnClickNextPage)
	EVT_CHECKBOX(PALETTE_CREATURE_PREVIEW_CHECKBOX, CreaturePalettePanel::OnToggleCreaturePreview)

	EVT_TOGGLEBUTTON(PALETTE_CREATURE_BRUSH_BUTTON, CreaturePalettePanel::OnClickCreatureBrushButton)
	EVT_TOGGLEBUTTON(PALETTE_SPAWN_BRUSH_BUTTON, CreaturePalettePanel::OnClickSpawnBrushButton)

	EVT_SPINCTRL(PALETTE_CREATURE_SPAWN_TIME, CreaturePalettePanel::OnChangeSpawnTime)
	EVT_SPINCTRL(PALETTE_CREATURE_SPAWN_SIZE, CreaturePalettePanel::OnChangeSpawnSize)

	EVT_BUTTON(PALETTE_CREATURE_FAVORITES_ADD, CreaturePalettePanel::OnClickFavoriteAdd)
	EVT_BUTTON(PALETTE_CREATURE_FAVORITES_REMOVE, CreaturePalettePanel::OnClickFavoriteRemove)
	EVT_BUTTON(PALETTE_CREATURE_GROUP_ADD, CreaturePalettePanel::OnClickGroupAdd)
	EVT_BUTTON(PALETTE_CREATURE_GROUP_REMOVE, CreaturePalettePanel::OnClickGroupRemove)
	EVT_BUTTON(PALETTE_CREATURE_GROUP_CLEAR, CreaturePalettePanel::OnClickGroupClear)
END_EVENT_TABLE()

CreaturePalettePanel::CreaturePalettePanel(wxWindow* parent, wxWindowID id) :
	PalettePanel(parent, id),
	handling_event(false),
	prefer_favorite_for_group_add(false),
	current_creature_page(0)
{
	wxSizer* topsizer = newd wxBoxSizer(wxVERTICAL);

	wxSizer* sidesizer = newd wxStaticBoxSizer(wxVERTICAL, this, "Creatures");
	tileset_choice = newd wxChoice(this, PALETTE_CREATURE_TILESET_CHOICE, wxDefaultPosition, wxDefaultSize, (int)0, (const wxString*)nullptr);
	sidesizer->Add(tileset_choice, 0, wxEXPAND);

	creature_filter_text = newd KeyForwardingTextCtrl(this, PALETTE_CREATURE_FILTER, "", wxDefaultPosition, wxDefaultSize);
	creature_filter_text->SetHint("Search...");
	sidesizer->Add(creature_filter_text, 0, wxEXPAND | wxTOP, 4);

	wxSizer* creature_controls = newd wxBoxSizer(wxHORIZONTAL);
	creature_preview_checkbox = newd wxCheckBox(this, PALETTE_CREATURE_PREVIEW_CHECKBOX, "Preview 32x32");
	creature_controls->Add(creature_preview_checkbox, 0, wxALIGN_CENTER_VERTICAL | wxRIGHT, 6);
	creature_controls->AddStretchSpacer(1);
	creature_page_previous_button = newd wxButton(this, PALETTE_CREATURE_PAGE_PREVIOUS, "<", wxDefaultPosition, wxSize(24, -1));
	creature_controls->Add(creature_page_previous_button, 0, wxALIGN_CENTER_VERTICAL | wxRIGHT, 4);
	creature_page_label = newd wxStaticText(this, wxID_ANY, "0/0");
	creature_controls->Add(creature_page_label, 0, wxALIGN_CENTER_VERTICAL | wxRIGHT, 4);
	creature_page_next_button = newd wxButton(this, PALETTE_CREATURE_PAGE_NEXT, ">", wxDefaultPosition, wxSize(24, -1));
	creature_controls->Add(creature_page_next_button, 0, wxALIGN_CENTER_VERTICAL);
	sidesizer->Add(creature_controls, 0, wxEXPAND | wxTOP | wxBOTTOM, 4);

	creature_list = newd CreaturePaletteListBox(this, PALETTE_CREATURE_LISTBOX);
	sidesizer->Add(creature_list, 1, wxEXPAND);
	topsizer->Add(sidesizer, 1, wxEXPAND);

	// Favorites setup
	wxSizer* favorites_sizer = newd wxStaticBoxSizer(wxVERTICAL, this, "Favorite Creatures");
	favorite_creature_list = newd wxListBox(this, PALETTE_CREATURE_FAVORITES_LIST, wxDefaultPosition, wxSize(-1, 95), 0, nullptr, wxLB_SINGLE);
	favorites_sizer->Add(favorite_creature_list, 1, wxEXPAND | wxBOTTOM, 5);

	wxSizer* favorites_buttons = newd wxBoxSizer(wxHORIZONTAL);
	favorite_creature_add_button = newd wxButton(this, PALETTE_CREATURE_FAVORITES_ADD, "Add Selected");
	favorite_creature_remove_button = newd wxButton(this, PALETTE_CREATURE_FAVORITES_REMOVE, "Remove");
	favorites_buttons->Add(favorite_creature_add_button, 1, wxRIGHT, 5);
	favorites_buttons->Add(favorite_creature_remove_button, 1);
	favorites_sizer->Add(favorites_buttons, 0, wxEXPAND);
	topsizer->Add(favorites_sizer, 0, wxEXPAND | wxLEFT | wxRIGHT | wxBOTTOM, 5);

	// Spawn group setup
	wxSizer* group_sizer = newd wxStaticBoxSizer(wxVERTICAL, this, "Spawn Group");
	wxSizer* group_controls = newd wxBoxSizer(wxHORIZONTAL);
	group_controls->Add(newd wxStaticText(this, wxID_ANY, "Count"), 0, wxALIGN_CENTER_VERTICAL | wxRIGHT, 5);
	spawn_group_count_spin = newd wxSpinCtrl(this, PALETTE_CREATURE_GROUP_COUNT, i2ws(1), wxDefaultPosition, wxSize(50, 20), wxSP_ARROW_KEYS, 1, 100, 1);
	group_controls->Add(spawn_group_count_spin, 0, wxRIGHT, 5);
	spawn_group_add_button = newd wxButton(this, PALETTE_CREATURE_GROUP_ADD, "Add");
	group_controls->Add(spawn_group_add_button, 0);
	group_sizer->Add(group_controls, 0, wxEXPAND | wxBOTTOM, 5);

	spawn_group_list = newd wxListBox(this, PALETTE_CREATURE_GROUP_LIST, wxDefaultPosition, wxSize(-1, 90), 0, nullptr, wxLB_SINGLE);
	group_sizer->Add(spawn_group_list, 1, wxEXPAND | wxBOTTOM, 5);

	wxSizer* group_buttons = newd wxBoxSizer(wxHORIZONTAL);
	spawn_group_remove_button = newd wxButton(this, PALETTE_CREATURE_GROUP_REMOVE, "Remove");
	spawn_group_clear_button = newd wxButton(this, PALETTE_CREATURE_GROUP_CLEAR, "Clear");
	group_buttons->Add(spawn_group_remove_button, 0, wxRIGHT, 5);
	group_buttons->Add(spawn_group_clear_button, 0);
	group_sizer->Add(group_buttons, 0, wxEXPAND);

	topsizer->Add(group_sizer, 0, wxEXPAND | wxLEFT | wxRIGHT | wxBOTTOM, 5);

	// Brush selection
	sidesizer = newd wxStaticBoxSizer(newd wxStaticBox(this, wxID_ANY, "Brushes", wxDefaultPosition, wxSize(150, 200)), wxVERTICAL);

	//sidesizer->Add(180, 1, wxEXPAND);

	wxFlexGridSizer* grid = newd wxFlexGridSizer(3, 10, 10);
	grid->AddGrowableCol(1);

	grid->Add(newd wxStaticText(this, wxID_ANY, "Spawntime"));
	creature_spawntime_spin = newd wxSpinCtrl(this, PALETTE_CREATURE_SPAWN_TIME, i2ws(g_settings.getInteger(Config::DEFAULT_SPAWNTIME)), wxDefaultPosition, wxSize(50, 20), wxSP_ARROW_KEYS, 0, 3600, g_settings.getInteger(Config::DEFAULT_SPAWNTIME));
	grid->Add(creature_spawntime_spin, 0, wxEXPAND);
	creature_brush_button = newd wxToggleButton(this, PALETTE_CREATURE_BRUSH_BUTTON, "Place Creature");
	grid->Add(creature_brush_button, 0, wxEXPAND);

	grid->Add(newd wxStaticText(this, wxID_ANY, "Spawn size"));
	spawn_size_spin = newd wxSpinCtrl(this, PALETTE_CREATURE_SPAWN_SIZE, i2ws(5), wxDefaultPosition, wxSize(50, 20), wxSP_ARROW_KEYS, 1, g_settings.getInteger(Config::MAX_SPAWN_RADIUS), g_settings.getInteger(Config::CURRENT_SPAWN_RADIUS));
	grid->Add(spawn_size_spin, 0, wxEXPAND);
	spawn_brush_button = newd wxToggleButton(this, PALETTE_SPAWN_BRUSH_BUTTON, "Place Spawn");
	grid->Add(spawn_brush_button, 0, wxEXPAND);

	sidesizer->Add(grid, 0, wxEXPAND);
	topsizer->Add(sidesizer, 0, wxEXPAND);
	SetSizerAndFit(topsizer);

	OnUpdate();
	{
		const auto& group = g_gui.GetSpawnCreatureGroup();
		spawn_group.clear();
		spawn_group.reserve(group.size());
		for(const auto& entry : group) {
			spawn_group.push_back({ entry.name, entry.count });
		}
	}
	UpdateSpawnGroupList();
	LoadFavoriteCreaturesFromSettings();
	UpdateFavoriteList();
	UpdateCreaturePaginationControls();
}

CreaturePalettePanel::~CreaturePalettePanel()
{
	////
}

PaletteType CreaturePalettePanel::GetType() const
{
	return TILESET_CREATURE;
}

void CreaturePalettePanel::SelectFirstBrush()
{
	SelectCreatureBrush();
}

Brush* CreaturePalettePanel::GetSelectedBrush() const
{
	if(creature_brush_button->GetValue()) {
		Brush* brush = GetSelectedCreatureBrush();
		if(brush && brush->isCreature()) {
			g_gui.SetSpawnTime(creature_spawntime_spin->GetValue());
			return brush;
		}
	} else if(spawn_brush_button->GetValue()) {
		g_settings.setInteger(Config::CURRENT_SPAWN_RADIUS, spawn_size_spin->GetValue());
		g_settings.setInteger(Config::DEFAULT_SPAWNTIME, creature_spawntime_spin->GetValue());
		return g_gui.spawn_brush;
	}
	return nullptr;
}

bool CreaturePalettePanel::SelectBrush(const Brush* whatbrush)
{
	if(!whatbrush)
		return false;

	if(whatbrush->isCreature()) {
		int current_index = tileset_choice->GetSelection();
		if(current_index != wxNOT_FOUND) {
			const TilesetCategory* tsc = reinterpret_cast<const TilesetCategory*>(tileset_choice->GetClientData(current_index));
			// Select first house
			for(BrushVector::const_iterator iter = tsc->brushlist.begin(); iter != tsc->brushlist.end(); ++iter) {
				if(*iter == whatbrush) {
					SelectCreature(whatbrush->getName());
					return true;
				}
			}
		}
		// Not in the current display, search the hidden one's
		for(size_t i = 0; i < tileset_choice->GetCount(); ++i) {
			if(current_index != (int)i) {
				const TilesetCategory* tsc = reinterpret_cast<const TilesetCategory*>(tileset_choice->GetClientData(i));
				for(BrushVector::const_iterator iter = tsc->brushlist.begin();
						iter != tsc->brushlist.end();
						++iter)
				{
					if(*iter == whatbrush) {
						SelectTileset(i);
						SelectCreature(whatbrush->getName());
						return true;
					}
				}
			}
		}
	} else if(whatbrush->isSpawn()) {
		SelectSpawnBrush();
		return true;
	}
	return false;
}

int CreaturePalettePanel::GetSelectedBrushSize() const
{
	return spawn_size_spin->GetValue();
}

void CreaturePalettePanel::OnUpdate()
{
	tileset_choice->Clear();
	g_materials.createOtherTileset();

	for(TilesetContainer::const_iterator iter = g_materials.tilesets.begin(); iter != g_materials.tilesets.end(); ++iter) {
		const TilesetCategory* tsc = iter->second->getCategory(TILESET_CREATURE);
		if(tsc && tsc->size() > 0) {
			tileset_choice->Append(wxstr(iter->second->name), const_cast<TilesetCategory*>(tsc));
		} else if(iter->second->name == "NPCs" || iter->second->name == "Others") {
			Tileset* ts = const_cast<Tileset*>(iter->second);
			TilesetCategory* rtsc = ts->getCategory(TILESET_CREATURE);
			tileset_choice->Append(wxstr(ts->name), rtsc);
		}
	}
	SelectTileset(0);
}

void CreaturePalettePanel::OnUpdateBrushSize(BrushShape shape, int size)
{
	return spawn_size_spin->SetValue(size);
}

void CreaturePalettePanel::OnSwitchIn()
{
	g_gui.ActivatePalette(GetParentPalette());
	g_gui.SetBrushSize(spawn_size_spin->GetValue());
}

void CreaturePalettePanel::SelectTileset(size_t index)
{
	ASSERT(tileset_choice->GetCount() >= index);

	current_creature_page = 0;
	if(tileset_choice->GetCount() == 0) {
		// No tilesets :(
		creature_brushes.clear();
		filtered_creature_brushes.clear();
		creature_list->SetBrushes(filtered_creature_brushes);
		UpdateCreaturePaginationControls();
		creature_brush_button->Enable(false);
	} else {
		const TilesetCategory* tsc = reinterpret_cast<const TilesetCategory*>(tileset_choice->GetClientData(index));
		creature_brushes.clear();
		for(BrushVector::const_iterator iter = tsc->brushlist.begin();
				iter != tsc->brushlist.end();
				++iter)
		{
			creature_brushes.push_back(*iter);
		}
		ApplyCreatureFilter(false);

		tileset_choice->SetSelection(index);
	}
}

void CreaturePalettePanel::SelectCreature(size_t index)
{
	if(creature_list->GetCount() > 0) {
		if(index < creature_list->GetCount()) {
			creature_list->SetSelection(static_cast<int>(index));
		} else {
			creature_list->SetSelection(0);
		}
	}

	SelectCreatureBrush();
}

void CreaturePalettePanel::SelectCreature(std::string name)
{
	if(filtered_creature_brushes.empty()) {
		creature_list->SetSelection(wxNOT_FOUND);
		SelectCreatureBrush();
		return;
	}

	auto it = std::find_if(filtered_creature_brushes.begin(), filtered_creature_brushes.end(), [&](const Brush* brush) {
		return brush && brush->getName() == name;
	});
	if(it != filtered_creature_brushes.end()) {
		const size_t index = static_cast<size_t>(std::distance(filtered_creature_brushes.begin(), it));
		current_creature_page = index / CREATURES_PER_PAGE;
		UpdateCreaturePage(name);
	} else {
		UpdateCreaturePage();
	}
	if(creature_list->GetCount() > 0 && creature_list->GetSelection() == wxNOT_FOUND) {
		creature_list->SetSelection(0);
	}
	SelectCreatureBrush();
}

void CreaturePalettePanel::SelectCreatureBrush()
{
	if(creature_list->GetCount() > 0) {
		creature_brush_button->Enable(true);
		creature_brush_button->SetValue(true);
		spawn_brush_button->SetValue(false);
	} else {
		creature_brush_button->Enable(false);
		SelectSpawnBrush();
	}
}

void CreaturePalettePanel::SelectSpawnBrush()
{
	//g_gui.house_exit_brush->setHouse(house);
	creature_brush_button->SetValue(false);
	spawn_brush_button->SetValue(true);
}

void CreaturePalettePanel::OnTilesetChange(wxCommandEvent& event)
{
	SelectTileset(event.GetSelection());
	g_gui.ActivatePalette(GetParentPalette());
	g_gui.SelectBrush();
}

void CreaturePalettePanel::OnFilterTextChange(wxCommandEvent& WXUNUSED(event))
{
	ApplyCreatureFilter(true);
	g_gui.ActivatePalette(GetParentPalette());
	g_gui.SelectBrush();
}

void CreaturePalettePanel::OnListBoxChange(wxCommandEvent& event)
{
	const int selection = event.GetSelection();
	if(selection != wxNOT_FOUND) {
		prefer_favorite_for_group_add = false;
		SelectCreature(static_cast<size_t>(selection));
	}
	g_gui.ActivatePalette(GetParentPalette());
	g_gui.SelectBrush();
}

void CreaturePalettePanel::OnClickCreatureBrushButton(wxCommandEvent& event)
{
	SelectCreatureBrush();
	g_gui.ActivatePalette(GetParentPalette());
	g_gui.SelectBrush();
}

void CreaturePalettePanel::OnClickSpawnBrushButton(wxCommandEvent& event)
{
	SelectSpawnBrush();
	g_gui.ActivatePalette(GetParentPalette());
	g_gui.SelectBrush();
}

void CreaturePalettePanel::OnClickGroupAdd(wxCommandEvent& WXUNUSED(event))
{
	std::string name;
	if(prefer_favorite_for_group_add) {
		const int selection = favorite_creature_list->GetSelection();
		if(selection >= 0 && selection < static_cast<int>(favorite_creatures.size())) {
			name = favorite_creatures[selection];
		}
	}

	if(name.empty()) {
		Brush* brush = GetSelectedCreatureBrush();
		if(!brush || !brush->isCreature()) {
			return;
		}
		name = brush->getName();
	}

	const int count = spawn_group_count_spin->GetValue();
	if(count <= 0) {
		return;
	}

	auto it = std::find_if(spawn_group.begin(), spawn_group.end(), [&](const SpawnGroupEntry& entry) {
		return entry.name == name;
	});
	if(it != spawn_group.end()) {
		it->count += count;
	} else {
		spawn_group.push_back({ name, count });
	}

	SyncSpawnGroupToGUI();
	UpdateSpawnGroupList();
}

void CreaturePalettePanel::OnClickGroupRemove(wxCommandEvent& WXUNUSED(event))
{
	const int selection = spawn_group_list->GetSelection();
	if(selection == wxNOT_FOUND) {
		return;
	}

	if(selection >= 0 && selection < static_cast<int>(spawn_group.size())) {
		spawn_group.erase(spawn_group.begin() + selection);
	}

	SyncSpawnGroupToGUI();
	UpdateSpawnGroupList();
}

void CreaturePalettePanel::OnClickGroupClear(wxCommandEvent& WXUNUSED(event))
{
	if(spawn_group.empty()) {
		return;
	}

	spawn_group.clear();
	SyncSpawnGroupToGUI();
	UpdateSpawnGroupList();
}

void CreaturePalettePanel::OnGroupListChange(wxCommandEvent& WXUNUSED(event))
{
	spawn_group_remove_button->Enable(spawn_group_list->GetSelection() != wxNOT_FOUND);
}

void CreaturePalettePanel::OnGroupListDoubleClick(wxCommandEvent& event)
{
	int selection = event.GetSelection();
	if(selection == wxNOT_FOUND) {
		selection = spawn_group_list->GetSelection();
	}
	if(selection == wxNOT_FOUND) {
		return;
	}

	if(selection < 0 || selection >= static_cast<int>(spawn_group.size())) {
		return;
	}

	SpawnGroupEntry& entry = spawn_group[selection];
	wxString message;
	message << "Set creature count for \"" << wxstr(entry.name) << "\"";
	wxNumberEntryDialog dialog(
		this,
		message,
		"Count",
		"Edit Spawn Group Count",
		entry.count,
		1,
		1000000);

	if(dialog.ShowModal() != wxID_OK) {
		return;
	}

	entry.count = static_cast<int>(dialog.GetValue());
	SyncSpawnGroupToGUI();
	UpdateSpawnGroupList();
	spawn_group_list->SetSelection(selection);
	spawn_group_remove_button->Enable(true);
}

void CreaturePalettePanel::OnFavoriteListChange(wxCommandEvent& WXUNUSED(event))
{
	const int selection = favorite_creature_list->GetSelection();
	prefer_favorite_for_group_add = (selection != wxNOT_FOUND);
	favorite_creature_remove_button->Enable(selection != wxNOT_FOUND);
}

void CreaturePalettePanel::OnFavoriteListDoubleClick(wxCommandEvent& event)
{
	int selection = event.GetSelection();
	if(selection == wxNOT_FOUND) {
		selection = favorite_creature_list->GetSelection();
	}
	if(selection == wxNOT_FOUND) {
		return;
	}
	if(selection < 0 || selection >= static_cast<int>(favorite_creatures.size())) {
		return;
	}

	prefer_favorite_for_group_add = true;
	const std::string& name = favorite_creatures[selection];
	CreatureType* creature_type = g_creatures[name];
	if(creature_type && creature_type->brush) {
		SelectBrush(creature_type->brush);
	} else {
		SelectCreature(name);
	}

	g_gui.ActivatePalette(GetParentPalette());
	g_gui.SelectBrush();
}

void CreaturePalettePanel::OnClickFavoriteAdd(wxCommandEvent& WXUNUSED(event))
{
	Brush* brush = GetSelectedCreatureBrush();
	if(!brush || !brush->isCreature()) {
		return;
	}

	AddFavoriteCreature(brush->getName());
}

void CreaturePalettePanel::OnClickFavoriteRemove(wxCommandEvent& WXUNUSED(event))
{
	const int selection = favorite_creature_list->GetSelection();
	if(selection == wxNOT_FOUND) {
		return;
	}
	if(selection < 0 || selection >= static_cast<int>(favorite_creatures.size())) {
		return;
	}

	favorite_creatures.erase(favorite_creatures.begin() + selection);
	SaveFavoriteCreaturesToSettings();
	std::string preferred;
	if(!favorite_creatures.empty()) {
		const size_t next_index = std::min(static_cast<size_t>(selection), favorite_creatures.size() - 1);
		preferred = favorite_creatures[next_index];
	}
	UpdateFavoriteList(preferred);
}

void CreaturePalettePanel::OnClickPreviousPage(wxCommandEvent& WXUNUSED(event))
{
	if(current_creature_page == 0) {
		return;
	}

	--current_creature_page;
	UpdateCreaturePage();
	g_gui.ActivatePalette(GetParentPalette());
	g_gui.SelectBrush();
}

void CreaturePalettePanel::OnClickNextPage(wxCommandEvent& WXUNUSED(event))
{
	const size_t page_count = GetCreaturePageCount();
	if(page_count == 0 || current_creature_page + 1 >= page_count) {
		return;
	}

	++current_creature_page;
	UpdateCreaturePage();
	g_gui.ActivatePalette(GetParentPalette());
	g_gui.SelectBrush();
}

void CreaturePalettePanel::OnToggleCreaturePreview(wxCommandEvent& WXUNUSED(event))
{
	creature_list->SetShowPreview(creature_preview_checkbox->GetValue());
}

void CreaturePalettePanel::OnChangeSpawnTime(wxSpinEvent& event)
{
	g_gui.ActivatePalette(GetParentPalette());
	g_gui.SetSpawnTime(event.GetPosition());
}

void CreaturePalettePanel::OnChangeSpawnSize(wxSpinEvent& event)
{
	if(!handling_event) {
		handling_event = true;
		g_gui.ActivatePalette(GetParentPalette());
		g_gui.SetBrushSize(event.GetPosition());
		handling_event = false;
	}
}

void CreaturePalettePanel::SyncSpawnGroupToGUI()
{
	std::vector<GUI::SpawnCreatureEntry> group;
	group.reserve(spawn_group.size());
	for(const auto& entry : spawn_group) {
		group.push_back({ entry.name, entry.count });
	}
	g_gui.SetSpawnCreatureGroup(group);
}

void CreaturePalettePanel::UpdateSpawnGroupList()
{
	spawn_group_list->Clear();
	for(const auto& entry : spawn_group) {
		wxString label;
		label << wxstr(entry.name) << " x" << entry.count;
		spawn_group_list->Append(label);
	}

	spawn_group_remove_button->Enable(spawn_group_list->GetSelection() != wxNOT_FOUND);
	spawn_group_clear_button->Enable(!spawn_group.empty());
}

void CreaturePalettePanel::LoadFavoriteCreaturesFromSettings()
{
	favorite_creatures.clear();
	std::stringstream stream(g_settings.getString(Config::CREATURE_FAVORITES));
	std::string entry;
	while(std::getline(stream, entry)) {
		trim(entry);
		if(entry.empty()) {
			continue;
		}

		const std::string normalized = as_lower_str(entry);
		auto it = std::find_if(favorite_creatures.begin(), favorite_creatures.end(), [&](const std::string& existing) {
			return as_lower_str(existing) == normalized;
		});
		if(it == favorite_creatures.end()) {
			favorite_creatures.push_back(entry);
		}
	}

	std::sort(favorite_creatures.begin(), favorite_creatures.end(), [](const std::string& lhs, const std::string& rhs) {
		return as_lower_str(lhs) < as_lower_str(rhs);
	});
}

void CreaturePalettePanel::SaveFavoriteCreaturesToSettings() const
{
	std::ostringstream stream;
	for(size_t i = 0; i < favorite_creatures.size(); ++i) {
		if(i != 0) {
			stream << '\n';
		}
		stream << favorite_creatures[i];
	}
	g_settings.setString(Config::CREATURE_FAVORITES, stream.str());
}

void CreaturePalettePanel::UpdateFavoriteList(const std::string& preferred_selection)
{
	favorite_creature_list->Clear();
	int selection = wxNOT_FOUND;
	for(size_t i = 0; i < favorite_creatures.size(); ++i) {
		favorite_creature_list->Append(wxstr(favorite_creatures[i]));
		if(!preferred_selection.empty() && favorite_creatures[i] == preferred_selection) {
			selection = static_cast<int>(i);
		}
	}

	if(selection != wxNOT_FOUND) {
		favorite_creature_list->SetSelection(selection);
	}

	favorite_creature_remove_button->Enable(favorite_creature_list->GetSelection() != wxNOT_FOUND);
}

void CreaturePalettePanel::AddFavoriteCreature(const std::string& name)
{
	if(name.empty()) {
		return;
	}

	const std::string normalized = as_lower_str(name);
	for(size_t i = 0; i < favorite_creatures.size(); ++i) {
		if(as_lower_str(favorite_creatures[i]) == normalized) {
			UpdateFavoriteList(favorite_creatures[i]);
			return;
		}
	}

	favorite_creatures.push_back(name);
	std::sort(favorite_creatures.begin(), favorite_creatures.end(), [](const std::string& lhs, const std::string& rhs) {
		return as_lower_str(lhs) < as_lower_str(rhs);
	});
	SaveFavoriteCreaturesToSettings();
	UpdateFavoriteList(name);
}

Brush* CreaturePalettePanel::GetSelectedCreatureBrush() const
{
	return creature_list->GetSelectedBrush();
}

size_t CreaturePalettePanel::GetCreaturePageCount() const
{
	if(filtered_creature_brushes.empty()) {
		return 0;
	}
	return (filtered_creature_brushes.size() + CREATURES_PER_PAGE - 1) / CREATURES_PER_PAGE;
}

void CreaturePalettePanel::UpdateCreaturePaginationControls()
{
	const size_t page_count = GetCreaturePageCount();
	if(page_count == 0) {
		creature_page_label->SetLabel("0/0");
		creature_page_previous_button->Enable(false);
		creature_page_next_button->Enable(false);
		Layout();
		return;
	}

	wxString label;
	label << static_cast<int>(current_creature_page + 1) << "/" << static_cast<int>(page_count);
	creature_page_label->SetLabel(label);
	creature_page_previous_button->Enable(current_creature_page > 0);
	creature_page_next_button->Enable(current_creature_page + 1 < page_count);
	Layout();
}

void CreaturePalettePanel::UpdateCreaturePage(const std::string& preferred_selection)
{
	const size_t page_count = GetCreaturePageCount();
	if(page_count == 0) {
		current_creature_page = 0;
		creature_list->SetBrushes(filtered_creature_brushes);
		creature_list->SetSelection(wxNOT_FOUND);
		UpdateCreaturePaginationControls();
		creature_brush_button->Enable(false);
		SelectSpawnBrush();
		return;
	}

	if(current_creature_page >= page_count) {
		current_creature_page = page_count - 1;
	}

	const size_t page_begin = current_creature_page * CREATURES_PER_PAGE;
	const size_t page_end = std::min(page_begin + CREATURES_PER_PAGE, filtered_creature_brushes.size());
	std::vector<Brush*> page_items(filtered_creature_brushes.begin() + page_begin, filtered_creature_brushes.begin() + page_end);
	creature_list->SetBrushes(page_items);

	bool selected = false;
	if(!preferred_selection.empty()) {
		selected = creature_list->SetSelectionByName(preferred_selection);
	}
	if(!selected && creature_list->GetCount() > 0) {
		creature_list->SetSelection(0);
	}

	UpdateCreaturePaginationControls();
	SelectCreatureBrush();
}

void CreaturePalettePanel::ApplyCreatureFilter(bool preserve_selection)
{
	std::string previous;
	if(preserve_selection) {
		if(Brush* selected = GetSelectedCreatureBrush()) {
			previous = selected->getName();
		}
	}

	const std::string filter = as_lower_str(nstr(creature_filter_text->GetValue()));
	filtered_creature_brushes.clear();
	filtered_creature_brushes.reserve(creature_brushes.size());
	for(Brush* brush : creature_brushes) {
		if(filter.empty()) {
			filtered_creature_brushes.push_back(brush);
			continue;
		}

		if(as_lower_str(brush->getName()).find(filter) != std::string::npos) {
			filtered_creature_brushes.push_back(brush);
		}
	}

	std::sort(filtered_creature_brushes.begin(), filtered_creature_brushes.end(), CreatureNameLess);

	if(preserve_selection && !previous.empty()) {
		auto it = std::find_if(filtered_creature_brushes.begin(), filtered_creature_brushes.end(), [&](const Brush* brush) {
			return brush && brush->getName() == previous;
		});
		if(it != filtered_creature_brushes.end()) {
			const size_t index = static_cast<size_t>(std::distance(filtered_creature_brushes.begin(), it));
			current_creature_page = index / CREATURES_PER_PAGE;
		}
	}

	const size_t page_count = GetCreaturePageCount();
	if(page_count == 0) {
		current_creature_page = 0;
	} else if(current_creature_page >= page_count) {
		current_creature_page = page_count - 1;
	}

	UpdateCreaturePage(previous);
}
