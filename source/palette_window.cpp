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
#include "gui.h"
#include "brush.h"
#include "map_display.h"

#include "palette_window.h"
#include "palette_brushlist.h"
#include "palette_house.h"
#include "palette_creature.h"
#include "palette_waypoints.h"

#include "house_brush.h"
#include "map.h"
#include "theme.h"

#include <wx/dcbuffer.h>
#include <wx/dcmemory.h>

// ---------------------------------------------------------------------------
// Custom button used by the palette selector. It keeps the painting/local
// events encapsulated so PaletteWindow only worries about layout + wiring.

class PaletteCategoryButton : public wxPanel
{
public:
	PaletteCategoryButton(
		wxWindow* parent,
		PaletteType type,
		const wxString& label,
		const wxBitmap& icon_bitmap) :
		wxPanel(parent, wxID_ANY, wxDefaultPosition, wxSize(120, 36)),
		palette_type(type),
		text(label),
		icon(icon_bitmap),
		hovered(false),
		selected(false)
	{
		SetBackgroundStyle(wxBG_STYLE_PAINT);
		SetMinSize(wxSize(120, 32));
		SetCursor(wxCursor(wxCURSOR_HAND));
		SetFont(wxFont(
			9,
			wxFONTFAMILY_SWISS,
			wxFONTSTYLE_NORMAL,
			wxFONTWEIGHT_NORMAL));
		SetDoubleBuffered(true);
		const ThemeColors& theme = Theme::Dark();
		SetBackgroundColour(theme.surfaceAlt);
		SetForegroundColour(theme.text);

		Bind(wxEVT_PAINT, &PaletteCategoryButton::OnPaint, this);
		Bind(wxEVT_ENTER_WINDOW, &PaletteCategoryButton::OnEnter, this);
		Bind(wxEVT_LEAVE_WINDOW, &PaletteCategoryButton::OnLeave, this);
		Bind(wxEVT_LEFT_UP, &PaletteCategoryButton::OnClick, this);
	}

	void SetSelected(bool value)
	{
		if(selected == value) {
			return;
		}
		selected = value;
		Refresh();
	}

	PaletteType GetPaletteType() const { return palette_type; }

private:
	void OnPaint(wxPaintEvent&)
	{
		wxAutoBufferedPaintDC dc(this);
		const ThemeColors& theme = Theme::Dark();
		dc.SetBackground(wxBrush(theme.surfaceAlt));
		dc.Clear();

		wxRect bounds = GetClientRect();
		bounds.Deflate(4, 2);

		const wxColour fill = selected ? theme.controlActive : (hovered ? theme.controlHover : theme.controlBase);
		const wxColour border = selected ? theme.accent : theme.border;

		dc.SetPen(wxPen(border, 1));
		dc.SetBrush(wxBrush(fill));
		dc.DrawRoundedRectangle(bounds, 5);

		if(selected) {
			dc.SetPen(wxPen(theme.accent, 2));
			dc.DrawLine(bounds.GetLeft() + 4, bounds.GetBottom() - 2, bounds.GetRight() - 4, bounds.GetBottom() - 2);
		}

		dc.SetTextForeground(theme.text);
		int textWidth = 0;
		int textHeight = 0;
		dc.GetTextExtent(text, &textWidth, &textHeight);

		int textX = bounds.GetLeft() + 12;
		if(icon.IsOk()) {
			wxSize iconSize = icon.GetSize();
			const int iconX = bounds.GetLeft() + 8;
			const int iconY = bounds.GetY() + (bounds.GetHeight() - iconSize.GetHeight()) / 2;
			dc.DrawBitmap(icon, iconX, iconY, true);
			textX = iconX + iconSize.GetWidth() + 6;
		}

		const int textY = bounds.GetY() + (bounds.GetHeight() - textHeight) / 2;
		dc.DrawText(text, textX, textY);
	}

	void OnEnter(wxMouseEvent& event)
	{
		hovered = true;
		Refresh();
		event.Skip();
	}

	void OnLeave(wxMouseEvent& event)
	{
		hovered = false;
		Refresh();
		event.Skip();
	}

	void OnClick(wxMouseEvent& event)
	{
		wxCommandEvent click(wxEVT_BUTTON, GetId());
		click.SetEventObject(this);
		click.SetInt(static_cast<int>(palette_type));
		wxPostEvent(this, click);
		event.Skip();
	}

	PaletteType palette_type;
	wxString text;
	wxBitmap icon;
	bool hovered;
	bool selected;
};

// ============================================================================
// Palette window

BEGIN_EVENT_TABLE(PaletteWindow, wxPanel)
	EVT_CHOICEBOOK_PAGE_CHANGING(PALETTE_CHOICEBOOK, PaletteWindow::OnSwitchingPage)
	EVT_CHOICEBOOK_PAGE_CHANGED(PALETTE_CHOICEBOOK, PaletteWindow::OnPageChanged)
	EVT_CLOSE(PaletteWindow::OnClose)

	EVT_KEY_DOWN(PaletteWindow::OnKey)
END_EVENT_TABLE()

PaletteWindow::PaletteWindow(wxWindow* parent, const TilesetContainer& tilesets) :
	wxPanel(parent, wxID_ANY, wxDefaultPosition, wxSize(230, 250)),
	choicebook(nullptr),
	palette_selector_panel(nullptr),
	current_palette(TILESET_UNKNOWN),
	terrain_palette(nullptr),
	doodad_palette(nullptr),
	item_palette(nullptr),
	creature_palette(nullptr),
	house_palette(nullptr),
	waypoint_palette(nullptr),
	raw_palette(nullptr)
{
	SetMinSize(wxSize(225, 250));
	const ThemeColors& theme = Theme::Dark();
	SetBackgroundColour(theme.background);
	SetForegroundColour(theme.text);

	// Create the hidden book control that still hosts all palette pages.
	choicebook = newd wxChoicebook(this, PALETTE_CHOICEBOOK, wxDefaultPosition, wxSize(230, 250));
	choicebook->SetMinSize(wxSize(225, 300));
	if(choicebook->GetChoiceCtrl()) {
		// Hide the original ComboBox so that all palette switching runs through our custom buttons.
		choicebook->GetChoiceCtrl()->Hide();
		choicebook->GetChoiceCtrl()->SetMinSize(wxSize(0, 0));
	}

	// Create the left-side selector panel that mimics a FlowLayout (vertical stack).
	choicebook->SetBackgroundColour(theme.surface);
	choicebook->SetForegroundColour(theme.text);

	// Create the left-side selector panel that mimics a FlowLayout (vertical stack).
	palette_selector_panel = newd wxPanel(this, wxID_ANY, wxDefaultPosition, wxSize(140, -1));
	palette_selector_panel->SetBackgroundColour(theme.surfaceAlt);
	palette_selector_panel->SetMinSize(wxSize(140, -1));

	terrain_palette = static_cast<BrushPalettePanel*>(CreateTerrainPalette(choicebook, tilesets));
	choicebook->AddPage(terrain_palette, terrain_palette->GetName());

	doodad_palette = static_cast<BrushPalettePanel*>(CreateDoodadPalette(choicebook, tilesets));
	choicebook->AddPage(doodad_palette, doodad_palette->GetName());

	item_palette = static_cast<BrushPalettePanel*>(CreateItemPalette(choicebook, tilesets));
	choicebook->AddPage(item_palette, item_palette->GetName());

	house_palette = static_cast<HousePalettePanel*>(CreateHousePalette(choicebook, tilesets));
	choicebook->AddPage(house_palette, house_palette->GetName());

	waypoint_palette = static_cast<WaypointPalettePanel*>(CreateWaypointPalette(choicebook, tilesets));
	choicebook->AddPage(waypoint_palette, waypoint_palette->GetName());

	creature_palette = static_cast<CreaturePalettePanel*>(CreateCreaturePalette(choicebook, tilesets));
	choicebook->AddPage(creature_palette, creature_palette->GetName());

	raw_palette = static_cast<BrushPalettePanel*>(CreateRAWPalette(choicebook, tilesets));
	choicebook->AddPage(raw_palette, raw_palette->GetName());

	BuildPaletteSelector();

	// Setup layout: palette selector on the left, content book on the right.
	wxSizer* sizer = newd wxBoxSizer(wxHORIZONTAL);
	sizer->Add(palette_selector_panel, 0, wxEXPAND | wxRIGHT, 6);
	sizer->Add(choicebook, 1, wxEXPAND);
	SetSizer(sizer);

	// Load first page
	LoadCurrentContents();
	UpdateButtonStates(GetSelectedPage());

	Fit();
}

PaletteWindow::~PaletteWindow()
{
	////
}

PalettePanel* PaletteWindow::CreateTerrainPalette(wxWindow *parent, const TilesetContainer& tilesets)
{
	BrushPalettePanel* panel = newd BrushPalettePanel(parent, tilesets, TILESET_TERRAIN);
	panel->SetListType(wxstr(g_settings.getString(Config::PALETTE_TERRAIN_STYLE)));

	BrushToolPanel* tool_panel = newd BrushToolPanel(panel);
	tool_panel->SetToolbarIconSize(g_settings.getBoolean(Config::USE_LARGE_TERRAIN_TOOLBAR));
	panel->AddToolPanel(tool_panel);

	BrushSizePanel* size_panel = newd BrushSizePanel(panel);
	size_panel->SetToolbarIconSize(g_settings.getBoolean(Config::USE_LARGE_TERRAIN_TOOLBAR));
	panel->AddToolPanel(size_panel);

	return panel;
}

PalettePanel* PaletteWindow::CreateDoodadPalette(wxWindow *parent, const TilesetContainer& tilesets)
{
	BrushPalettePanel* panel = newd BrushPalettePanel(parent, tilesets, TILESET_DOODAD);
	panel->SetListType(wxstr(g_settings.getString(Config::PALETTE_DOODAD_STYLE)));

	panel->AddToolPanel(newd BrushThicknessPanel(panel));

	BrushSizePanel* size_panel = newd BrushSizePanel(panel);
	size_panel->SetToolbarIconSize(g_settings.getBoolean(Config::USE_LARGE_DOODAD_SIZEBAR));
	panel->AddToolPanel(size_panel);

	return panel;
}

PalettePanel* PaletteWindow::CreateItemPalette(wxWindow *parent, const TilesetContainer& tilesets)
{
	BrushPalettePanel* panel = newd BrushPalettePanel(parent, tilesets, TILESET_ITEM);
	panel->SetListType(wxstr(g_settings.getString(Config::PALETTE_ITEM_STYLE)));

	BrushSizePanel* size_panel = newd BrushSizePanel(panel);
	size_panel->SetToolbarIconSize(g_settings.getBoolean(Config::USE_LARGE_ITEM_SIZEBAR));
	panel->AddToolPanel(size_panel);
	return panel;
}

PalettePanel* PaletteWindow::CreateHousePalette(wxWindow *parent, const TilesetContainer& tilesets)
{
	HousePalettePanel* panel = newd HousePalettePanel(parent);

	BrushSizePanel* size_panel = newd BrushSizePanel(panel);
	size_panel->SetToolbarIconSize(g_settings.getBoolean(Config::USE_LARGE_HOUSE_SIZEBAR));
	panel->AddToolPanel(size_panel);
	return panel;
}

PalettePanel* PaletteWindow::CreateWaypointPalette(wxWindow *parent, const TilesetContainer& tilesets)
{
	WaypointPalettePanel* panel = newd WaypointPalettePanel(parent);
	return panel;
}

PalettePanel* PaletteWindow::CreateCreaturePalette(wxWindow *parent, const TilesetContainer& tilesets)
{
	CreaturePalettePanel* panel = newd CreaturePalettePanel(parent);
	return panel;
}

PalettePanel* PaletteWindow::CreateRAWPalette(wxWindow *parent, const TilesetContainer& tilesets)
{
	BrushPalettePanel* panel = newd BrushPalettePanel(parent, tilesets, TILESET_RAW);
	panel->SetListType(wxstr(g_settings.getString(Config::PALETTE_RAW_STYLE)));

	BrushSizePanel* size_panel = newd BrushSizePanel(panel);
	size_panel->SetToolbarIconSize(g_settings.getBoolean(Config::USE_LARGE_RAW_SIZEBAR));
	panel->AddToolPanel(size_panel);

	return panel;
}

void PaletteWindow::ReloadSettings(Map* map)
{
	if(terrain_palette) {
		terrain_palette->SetListType(wxstr(g_settings.getString(Config::PALETTE_TERRAIN_STYLE)));
		terrain_palette->SetToolbarIconSize(g_settings.getBoolean(Config::USE_LARGE_TERRAIN_TOOLBAR));
	}
	if(doodad_palette) {
		doodad_palette->SetListType(wxstr(g_settings.getString(Config::PALETTE_DOODAD_STYLE)));
		doodad_palette->SetToolbarIconSize(g_settings.getBoolean(Config::USE_LARGE_DOODAD_SIZEBAR));
	}
	if(house_palette) {
		house_palette->SetMap(map);
		house_palette->SetToolbarIconSize(g_settings.getBoolean(Config::USE_LARGE_HOUSE_SIZEBAR));
	}
	if(waypoint_palette) {
		waypoint_palette->SetMap(map);
	}
	if(item_palette) {
		item_palette->SetListType(wxstr(g_settings.getString(Config::PALETTE_ITEM_STYLE)));
		item_palette->SetToolbarIconSize(g_settings.getBoolean(Config::USE_LARGE_ITEM_SIZEBAR));
	}
	if(raw_palette) {
		raw_palette->SetListType(wxstr(g_settings.getString(Config::PALETTE_RAW_STYLE)));
		raw_palette->SetToolbarIconSize(g_settings.getBoolean(Config::USE_LARGE_RAW_SIZEBAR));
	}
	InvalidateContents();
}

void PaletteWindow::LoadCurrentContents()
{
	if(!choicebook) return;
	PalettePanel* panel = dynamic_cast<PalettePanel*>(choicebook->GetCurrentPage());
	panel->LoadCurrentContents();
	Fit();
	Refresh();
	Update();
}

void PaletteWindow::InvalidateContents()
{
	if(!choicebook) return;
	for(size_t iz = 0; iz < choicebook->GetPageCount(); ++iz) {
		PalettePanel* panel = dynamic_cast<PalettePanel*>(choicebook->GetPage(iz));
		panel->InvalidateContents();
	}
	LoadCurrentContents();
	if(creature_palette) {
		creature_palette->OnUpdate();
	}
	if(house_palette) {
		house_palette->OnUpdate();
	}
	if(waypoint_palette) {
		waypoint_palette->OnUpdate();
	}
}

void PaletteWindow::SelectPage(PaletteType id)
{
	ChangePalette(id);
}

Brush* PaletteWindow::GetSelectedBrush() const
{
	if(!choicebook) return nullptr;
	PalettePanel* panel = dynamic_cast<PalettePanel*>(choicebook->GetCurrentPage());
	return panel->GetSelectedBrush();
}

int PaletteWindow::GetSelectedBrushSize() const
{
	if(!choicebook) return 0;
	PalettePanel* panel = dynamic_cast<PalettePanel*>(choicebook->GetCurrentPage());
	return panel->GetSelectedBrushSize();
}

PaletteType PaletteWindow::GetSelectedPage() const
{
	if(!choicebook) return TILESET_UNKNOWN;
	PalettePanel* panel = dynamic_cast<PalettePanel*>(choicebook->GetCurrentPage());
	ASSERT(panel);
	return panel->GetType();
}

bool PaletteWindow::OnSelectBrush(const Brush* whatbrush, PaletteType primary)
{
	if(!choicebook || !whatbrush)
		return false;

	if(whatbrush->isHouse() && house_palette) {
		house_palette->SelectBrush(whatbrush);
		SelectPage(TILESET_HOUSE);
		return true;
	}

	switch(primary) {
		case TILESET_TERRAIN: {
			// This is already searched first
			break;
		}
		case TILESET_DOODAD: {
			// Ok, search doodad before terrain
			if(doodad_palette && doodad_palette->SelectBrush(whatbrush)) {
				SelectPage(TILESET_DOODAD);
				return true;
			}
			break;
		}
		case TILESET_ITEM: {
			if(item_palette && item_palette->SelectBrush(whatbrush)) {
				SelectPage(TILESET_ITEM);
				return true;
			}
			break;
		}
		case TILESET_CREATURE: {
			if(creature_palette && creature_palette->SelectBrush(whatbrush)) {
				SelectPage(TILESET_CREATURE);
				return true;
			}
			break;
		}
		case TILESET_RAW: {
			if(raw_palette && raw_palette->SelectBrush(whatbrush)) {
				SelectPage(TILESET_RAW);
				return true;
			}
			break;
		}
		default:
			break;
	}

	// Test if it's a terrain brush
	if(terrain_palette && terrain_palette->SelectBrush(whatbrush)) {
		SelectPage(TILESET_TERRAIN);
		return true;
	}

	// Test if it's a doodad brush
	if(primary != TILESET_DOODAD) {
		if(doodad_palette && doodad_palette->SelectBrush(whatbrush)) {
			SelectPage(TILESET_DOODAD);
			return true;
		}
	}

	// Test if it's an item brush
	if(primary != TILESET_ITEM) {
		if(item_palette && item_palette->SelectBrush(whatbrush)) {
			SelectPage(TILESET_ITEM);
			return true;
		}
	}

	// Test if it's a creature brush
	if(primary != TILESET_CREATURE) {
		if(creature_palette && creature_palette->SelectBrush(whatbrush)) {
			SelectPage(TILESET_CREATURE);
			return true;
		}
	}

	// Test if it's a raw brush
	if(primary != TILESET_RAW) {
		if(raw_palette && raw_palette->SelectBrush(whatbrush)) {
			SelectPage(TILESET_RAW);
			return true;
		}
	}

	return false;
}

void PaletteWindow::OnSwitchingPage(wxChoicebookEvent& event)
{
	event.Skip();
	if(!choicebook) return;

	wxWindow* old_page = choicebook->GetPage(choicebook->GetSelection());
	PalettePanel* old_panel = dynamic_cast<PalettePanel*>(old_page);
	if(old_panel) {
		old_panel->OnSwitchOut();
	}

	wxWindow* page = choicebook->GetPage(event.GetSelection());
	PalettePanel* panel = dynamic_cast<PalettePanel*>(page);
	if(panel) {
		panel->OnSwitchIn();
	}
}

void PaletteWindow::OnPageChanged(wxChoicebookEvent& event)
{
	if(!choicebook) return;
	UpdateButtonStates(GetSelectedPage());
	g_gui.SelectBrush();
}

void PaletteWindow::OnUpdateBrushSize(BrushShape shape, int size)
{
	if(!choicebook) return;
	PalettePanel* page = dynamic_cast<PalettePanel*>(choicebook->GetCurrentPage());
	ASSERT(page);
	page->OnUpdateBrushSize(shape, size);
}

void PaletteWindow::OnUpdate(Map* map)
{
	if(creature_palette) {
		creature_palette->OnUpdate();
	}
	if(house_palette) {
		house_palette->SetMap(map);
	}
	if(waypoint_palette) {
		waypoint_palette->SetMap(map);
		waypoint_palette->OnUpdate();
	}
}

void PaletteWindow::OnKey(wxKeyEvent& event)
{
	if(g_gui.GetCurrentTab() != nullptr) {
		g_gui.GetCurrentMapTab()->GetEventHandler()->AddPendingEvent(event);
	}
}

void PaletteWindow::OnClose(wxCloseEvent& event)
{
	if(!event.CanVeto()) {
		// We can't do anything! This sucks!
		// (application is closed, we have to destroy ourselves)
		Destroy();
	} else {
		Show(false);
		event.Veto(true);
	}
}

void PaletteWindow::ChangePalette(PaletteType id)
{
	if(!choicebook || id == TILESET_UNKNOWN)
		return;

	if(id == current_palette) {
		UpdateButtonStates(id);
		return;
	}

	for(size_t iz = 0; iz < choicebook->GetPageCount(); ++iz) {
		PalettePanel* panel = dynamic_cast<PalettePanel*>(choicebook->GetPage(iz));
		if(panel && panel->GetType() == id) {
			choicebook->SetSelection(iz);
			UpdateButtonStates(id);
			break;
		}
	}
}

void PaletteWindow::BuildPaletteSelector()
{
	if(!palette_selector_panel)
		return;

	wxBoxSizer* selectorSizer = newd wxBoxSizer(wxVERTICAL);
	selectorSizer->AddSpacer(6);
	palette_selector_panel->SetSizer(selectorSizer);

	struct ButtonInfo
	{
		PaletteType type;
		const char* label;
	};

	const ButtonInfo buttons[] = {
		{TILESET_TERRAIN, "Terrain"},
		{TILESET_DOODAD, "Doodad"},
		{TILESET_ITEM, "Item"},
		{TILESET_HOUSE, "House"},
		{TILESET_CREATURE, "Creature"},
		{TILESET_RAW, "RAW"}
	};

	for(const ButtonInfo& info : buttons) {
		PaletteCategoryButton* button = newd PaletteCategoryButton(
			palette_selector_panel,
			info.type,
			wxString(info.label),
			wxBitmap());
		button->Bind(wxEVT_BUTTON, &PaletteWindow::OnPaletteCategoryClicked, this);
		palette_buttons[info.type] = button;
		selectorSizer->Add(button, 0, wxEXPAND | wxBOTTOM, 6);
	}

	selectorSizer->AddStretchSpacer(1);
}

void PaletteWindow::UpdateButtonStates(PaletteType palette)
{
	if(current_palette == palette && !palette_buttons.empty()) {
		// Already highlighted, nothing else to do.
		return;
	}

	current_palette = palette;
	for(auto& entry : palette_buttons) {
		entry.second->SetSelected(entry.first == palette);
	}
}

void PaletteWindow::OnPaletteCategoryClicked(wxCommandEvent& event)
{
	const PaletteType type = static_cast<PaletteType>(event.GetInt());
	ChangePalette(type);
}
