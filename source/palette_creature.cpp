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
#include <algorithm>

// ============================================================================
// Creature palette

BEGIN_EVENT_TABLE(CreaturePalettePanel, PalettePanel)
	EVT_CHOICE(PALETTE_CREATURE_TILESET_CHOICE, CreaturePalettePanel::OnTilesetChange)

	EVT_TEXT(PALETTE_CREATURE_FILTER, CreaturePalettePanel::OnFilterTextChange)
	EVT_LISTBOX(PALETTE_CREATURE_LISTBOX, CreaturePalettePanel::OnListBoxChange)
	EVT_LISTBOX(PALETTE_CREATURE_GROUP_LIST, CreaturePalettePanel::OnGroupListChange)

	EVT_TOGGLEBUTTON(PALETTE_CREATURE_BRUSH_BUTTON, CreaturePalettePanel::OnClickCreatureBrushButton)
	EVT_TOGGLEBUTTON(PALETTE_SPAWN_BRUSH_BUTTON, CreaturePalettePanel::OnClickSpawnBrushButton)

	EVT_SPINCTRL(PALETTE_CREATURE_SPAWN_TIME, CreaturePalettePanel::OnChangeSpawnTime)
	EVT_SPINCTRL(PALETTE_CREATURE_SPAWN_SIZE, CreaturePalettePanel::OnChangeSpawnSize)

	EVT_BUTTON(PALETTE_CREATURE_GROUP_ADD, CreaturePalettePanel::OnClickGroupAdd)
	EVT_BUTTON(PALETTE_CREATURE_GROUP_REMOVE, CreaturePalettePanel::OnClickGroupRemove)
	EVT_BUTTON(PALETTE_CREATURE_GROUP_CLEAR, CreaturePalettePanel::OnClickGroupClear)
END_EVENT_TABLE()

CreaturePalettePanel::CreaturePalettePanel(wxWindow* parent, wxWindowID id) :
	PalettePanel(parent, id),
	handling_event(false)
{
	wxSizer* topsizer = newd wxBoxSizer(wxVERTICAL);

	wxSizer* sidesizer = newd wxStaticBoxSizer(wxVERTICAL, this, "Creatures");
	tileset_choice = newd wxChoice(this, PALETTE_CREATURE_TILESET_CHOICE, wxDefaultPosition, wxDefaultSize, (int)0, (const wxString*)nullptr);
	sidesizer->Add(tileset_choice, 0, wxEXPAND);

	creature_filter_text = newd KeyForwardingTextCtrl(this, PALETTE_CREATURE_FILTER, "", wxDefaultPosition, wxDefaultSize);
	creature_filter_text->SetHint("Search...");
	sidesizer->Add(creature_filter_text, 0, wxEXPAND | wxTOP, 4);

	creature_list = newd SortableListBox(this, PALETTE_CREATURE_LISTBOX);
	sidesizer->Add(creature_list, 1, wxEXPAND);
	topsizer->Add(sidesizer, 1, wxEXPAND);

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
		if(creature_list->GetCount() == 0) {
			return nullptr;
		}
		Brush* brush = reinterpret_cast<Brush*>(creature_list->GetClientData(creature_list->GetSelection()));
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

	creature_list->Clear();
	if(tileset_choice->GetCount() == 0) {
		// No tilesets :(
		creature_brushes.clear();
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
	// Save the old g_settings
	ASSERT(creature_list->GetCount() >= index);

	if(creature_list->GetCount() > 0) {
		creature_list->SetSelection(index);
	}

	SelectCreatureBrush();
}

void CreaturePalettePanel::SelectCreature(std::string name)
{
	if(creature_list->GetCount() > 0) {
		if(!creature_list->SetStringSelection(wxstr(name))) {
			creature_list->SetSelection(0);
		}
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
	SelectCreature(event.GetSelection());
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
	if(creature_list->GetCount() == 0) {
		return;
	}

	const int selection = creature_list->GetSelection();
	if(selection == wxNOT_FOUND) {
		return;
	}

	Brush* brush = reinterpret_cast<Brush*>(creature_list->GetClientData(selection));
	if(!brush || !brush->isCreature()) {
		return;
	}

	const int count = spawn_group_count_spin->GetValue();
	if(count <= 0) {
		return;
	}

	const std::string name = brush->getName();
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

void CreaturePalettePanel::ApplyCreatureFilter(bool preserve_selection)
{
	wxString previous;
	if(preserve_selection && creature_list->GetSelection() != wxNOT_FOUND) {
		previous = creature_list->GetStringSelection();
	}

	creature_list->Clear();
	const std::string filter = as_lower_str(nstr(creature_filter_text->GetValue()));
	for(Brush* brush : creature_brushes) {
		if(filter.empty()) {
			creature_list->Append(wxstr(brush->getName()), brush);
			continue;
		}

		if(as_lower_str(brush->getName()).find(filter) != std::string::npos) {
			creature_list->Append(wxstr(brush->getName()), brush);
		}
	}

	creature_list->Sort();

	if(creature_list->GetCount() > 0) {
		if(preserve_selection && !previous.empty()) {
			if(!creature_list->SetStringSelection(previous)) {
				creature_list->SetSelection(0);
			}
		} else {
			creature_list->SetSelection(0);
		}
		SelectCreatureBrush();
	} else {
		creature_brush_button->Enable(false);
		SelectSpawnBrush();
	}
}
