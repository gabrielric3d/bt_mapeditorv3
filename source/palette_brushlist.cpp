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

#include "palette_brushlist.h"
#include "gui.h"
#include "brush.h"
#include "raw_brush.h"
#include "map_window.h"
#include "map_tab.h"
#include "border_editor_window.h"
#include <wx/dnd.h>
#include <wx/menu.h>

namespace {

constexpr int kMinListIconSize = 16;
constexpr int kMaxListIconSize = 128;
const wxColour kPaletteListBackgroundColour(0x0C, 0x14, 0x2A);
const wxColour kPaletteListSelectionColour(0x16, 0x24, 0x43);
const wxColour kPaletteListTextColour(0xE0, 0xE6, 0xFF);
constexpr int kCreateBorderButtonId = wxID_HIGHEST + 4100;

int GetConfiguredListIconSize()
{
	int size = g_settings.getInteger(Config::PALETTE_LIST_ICON_SIZE);
	if(size < kMinListIconSize) {
		return kMinListIconSize;
	}
	if(size > kMaxListIconSize) {
		return kMaxListIconSize;
	}
	return size;
}

}

// ============================================================================
// Brush Palette Panel
// A common class for terrain/doodad/item/raw palette

BEGIN_EVENT_TABLE(BrushPalettePanel, PalettePanel)
	EVT_CHOICEBOOK_PAGE_CHANGING(wxID_ANY, BrushPalettePanel::OnSwitchingPage)
	EVT_CHOICEBOOK_PAGE_CHANGED(wxID_ANY, BrushPalettePanel::OnPageChanged)
	EVT_BUTTON(kCreateBorderButtonId, BrushPalettePanel::OnClickCreateBorder)
END_EVENT_TABLE()

BrushPalettePanel::BrushPalettePanel(wxWindow* parent, const TilesetContainer& tilesets, TilesetCategoryType category, wxWindowID id) :
	PalettePanel(parent, id),
	palette_type(category),
	choicebook(nullptr),
	size_panel(nullptr)
{
	wxSizer* topsizer = newd wxBoxSizer(wxVERTICAL);

	// Create the tileset panel
	wxSizer* ts_sizer = newd wxStaticBoxSizer(wxVERTICAL, this, "Tileset");
	wxChoicebook* tmp_choicebook = newd wxChoicebook(this, wxID_ANY, wxDefaultPosition, wxSize(180,250));
	ts_sizer->Add(tmp_choicebook, 1, wxEXPAND);
	topsizer->Add(ts_sizer, 1, wxEXPAND);

	if(palette_type == TILESET_TERRAIN) {
		wxButton* createBorderButton = newd wxButton(this, kCreateBorderButtonId, "Create Border");
		createBorderButton->SetToolTip("Open the Border Editor to create or edit auto-borders");
		topsizer->Add(createBorderButton, 0, wxEXPAND | wxALL, 5);
	}

	for(TilesetContainer::const_iterator iter = tilesets.begin(); iter != tilesets.end(); ++iter) {
		const TilesetCategory* tcg = iter->second->getCategory(category);
		if(tcg && tcg->size() > 0) {
			BrushPanel* panel = newd BrushPanel(tmp_choicebook);
			panel->AssignTileset(tcg);
			tmp_choicebook->AddPage(panel, wxstr(iter->second->name));
		}
	}

	SetSizerAndFit(topsizer);

	choicebook = tmp_choicebook;
}

BrushPalettePanel::~BrushPalettePanel()
{
	////
}

void BrushPalettePanel::InvalidateContents()
{
	for(size_t iz = 0; iz < choicebook->GetPageCount(); ++iz) {
		BrushPanel* panel = dynamic_cast<BrushPanel*>(choicebook->GetPage(iz));
		panel->InvalidateContents();
	}
	PalettePanel::InvalidateContents();
}

void BrushPalettePanel::LoadCurrentContents()
{
	wxWindow* page = choicebook->GetCurrentPage();
	BrushPanel* panel = dynamic_cast<BrushPanel*>(page);
	if(panel) {
		panel->OnSwitchIn();
	}
	PalettePanel::LoadCurrentContents();
}

void BrushPalettePanel::LoadAllContents()
{
	for(size_t iz = 0; iz < choicebook->GetPageCount(); ++iz) {
		BrushPanel* panel = dynamic_cast<BrushPanel*>(choicebook->GetPage(iz));
		panel->LoadContents();
	}
	PalettePanel::LoadAllContents();
}

PaletteType BrushPalettePanel::GetType() const
{
	return palette_type;
}

void BrushPalettePanel::SetListType(BrushListType ltype)
{
	if(!choicebook) return;
	for(size_t iz = 0; iz < choicebook->GetPageCount(); ++iz) {
		BrushPanel* panel = dynamic_cast<BrushPanel*>(choicebook->GetPage(iz));
		panel->SetListType(ltype);
	}
}

void BrushPalettePanel::SetListType(wxString ltype)
{
	if(!choicebook) return;
	for(size_t iz = 0; iz < choicebook->GetPageCount(); ++iz) {
		BrushPanel* panel = dynamic_cast<BrushPanel*>(choicebook->GetPage(iz));
		panel->SetListType(ltype);
	}
}

Brush* BrushPalettePanel::GetSelectedBrush() const
{
	if(!choicebook) return nullptr;
	wxWindow* page = choicebook->GetCurrentPage();
	BrushPanel* panel = dynamic_cast<BrushPanel*>(page);
	Brush* res = nullptr;
	if(panel) {
		for(ToolBarList::const_iterator iter = tool_bars.begin(); iter != tool_bars.end(); ++iter) {
			res = (*iter)->GetSelectedBrush();
			if(res) return res;
		}
		res = panel->GetSelectedBrush();
	}
	return res;
}

void BrushPalettePanel::SelectFirstBrush()
{
	if(!choicebook) return;
	wxWindow* page = choicebook->GetCurrentPage();
	BrushPanel* panel = dynamic_cast<BrushPanel*>(page);
	panel->SelectFirstBrush();
}

bool BrushPalettePanel::SelectBrush(const Brush* whatbrush)
{
	if(!choicebook) {
		return false;
	}

	BrushPanel* panel = dynamic_cast<BrushPanel*>(choicebook->GetCurrentPage());
	if(!panel) {
		return false;
	}

	for(PalettePanel* toolBar : tool_bars) {
		if(toolBar->SelectBrush(whatbrush)) {
			panel->SelectBrush(nullptr);
			return true;
		}
	}

	if(panel->SelectBrush(whatbrush)) {
		for(PalettePanel* toolBar : tool_bars) {
			toolBar->SelectBrush(nullptr);
		}
		return true;
	}

	for(size_t iz = 0; iz < choicebook->GetPageCount(); ++iz) {
		if((int)iz == choicebook->GetSelection()) {
			continue;
		}

		panel = dynamic_cast<BrushPanel*>(choicebook->GetPage(iz));
		if(panel && panel->SelectBrush(whatbrush)) {
			choicebook->ChangeSelection(iz);
			for(PalettePanel* toolBar : tool_bars) {
				toolBar->SelectBrush(nullptr);
			}
			return true;
		}
	}
	return false;
}

void BrushPalettePanel::OnSwitchingPage(wxChoicebookEvent& event)
{
	event.Skip();
	if(!choicebook) {
		return;
	}
	BrushPanel* old_panel = dynamic_cast<BrushPanel*>(choicebook->GetCurrentPage());
	if(old_panel) {
		old_panel->OnSwitchOut();
		for(ToolBarList::iterator iter = tool_bars.begin(); iter != tool_bars.end(); ++iter) {
			Brush* tmp = (*iter)->GetSelectedBrush();
			if(tmp) {
				remembered_brushes[old_panel] = tmp;
			}
		}
	}

	wxWindow* page = choicebook->GetPage(event.GetSelection());
	BrushPanel* panel = dynamic_cast<BrushPanel*>(page);
	if(panel) {
		panel->OnSwitchIn();
		for(ToolBarList::iterator iter = tool_bars.begin(); iter != tool_bars.end(); ++iter) {
			(*iter)->SelectBrush(remembered_brushes[panel]);
		}
	}
}

void BrushPalettePanel::OnPageChanged(wxChoicebookEvent& event)
{
	if(!choicebook) {
		return;
	}
	g_gui.ActivatePalette(GetParentPalette());
	g_gui.SelectBrush();
}

void BrushPalettePanel::OnClickCreateBorder(wxCommandEvent& WXUNUSED(event))
{
	BorderEditorDialog* dialog = new BorderEditorDialog(g_gui.root, "Auto Border Editor");
	dialog->Show();
	g_gui.RefreshView();
}

void BrushPalettePanel::OnSwitchIn() {
	LoadCurrentContents();
	g_gui.ActivatePalette(GetParentPalette());
	g_gui.SetBrushSizeInternal(last_brush_size);
	OnUpdateBrushSize(g_gui.GetBrushShape(), last_brush_size);
}

// ============================================================================
// Brush Panel
// A container of brush buttons

BEGIN_EVENT_TABLE(BrushPanel, wxPanel)
	// Listbox style
	EVT_LISTBOX(wxID_ANY, BrushPanel::OnClickListBoxRow)
END_EVENT_TABLE()

BrushPanel::BrushPanel(wxWindow *parent) :
	wxPanel(parent, wxID_ANY),
	tileset(nullptr),
	brushbox(nullptr),
	loaded(false),
	list_type(BRUSHLIST_LISTBOX)
{
	sizer = newd wxBoxSizer(wxVERTICAL);
	SetSizerAndFit(sizer);
}

BrushPanel::~BrushPanel()
{
	////
}

void BrushPanel::AssignTileset(const TilesetCategory* _tileset)
{
	if(_tileset != tileset) {
		InvalidateContents();
		tileset = _tileset;
	}
}

void BrushPanel::SetListType(BrushListType ltype)
{
	if(list_type != ltype) {
		InvalidateContents();
		list_type = ltype;
	}
}

void BrushPanel::SetListType(wxString ltype)
{
	if(ltype == "small icons") {
		SetListType(BRUSHLIST_SMALL_ICONS);
	} else if(ltype == "large icons") {
		SetListType(BRUSHLIST_LARGE_ICONS);
	} else if(ltype == "listbox") {
		SetListType(BRUSHLIST_LISTBOX);
	} else if(ltype == "textlistbox") {
		SetListType(BRUSHLIST_TEXT_LISTBOX);
	}
}

void BrushPanel::InvalidateContents()
{
	sizer->Clear(true);
	loaded = false;
	brushbox = nullptr;
}

void BrushPanel::LoadContents()
{
	if(loaded) {
		return;
	}
	loaded = true;
	ASSERT(tileset != nullptr);
	switch (list_type) {
		case BRUSHLIST_LARGE_ICONS:
			brushbox = newd BrushIconBox(this, tileset, RENDER_SIZE_32x32);
			break;
		case BRUSHLIST_SMALL_ICONS:
			brushbox = newd BrushIconBox(this, tileset, RENDER_SIZE_16x16);
			break;
		case BRUSHLIST_LISTBOX:
			brushbox = newd BrushListBox(this, tileset);
			break;
		default:
			break;
	}
	ASSERT(brushbox != nullptr);
	sizer->Add(brushbox->GetSelfWindow(), 1, wxEXPAND);
	Fit();
	brushbox->SelectFirstBrush();
}

void BrushPanel::SelectFirstBrush()
{
	if(loaded) {
		ASSERT(brushbox != nullptr);
		brushbox->SelectFirstBrush();
	}
}

Brush* BrushPanel::GetSelectedBrush() const
{
	if(loaded) {
		ASSERT(brushbox != nullptr);
		return brushbox->GetSelectedBrush();
	}

	if(tileset && tileset->size() > 0) {
		return tileset->brushlist[0];
	}
	return nullptr;
}

bool BrushPanel::SelectBrush(const Brush* whatbrush)
{
	if(loaded) {
		//std::cout << loaded << std::endl;
		//std::cout << brushbox << std::endl;
		ASSERT(brushbox != nullptr);
		return brushbox->SelectBrush(whatbrush);
	}

	for(BrushVector::const_iterator iter = tileset->brushlist.begin(); iter != tileset->brushlist.end(); ++iter) {
		if(*iter == whatbrush) {
			LoadContents();
			return brushbox->SelectBrush(whatbrush);
		}
	}
	return false;
}

void BrushPanel::OnSwitchIn()
{
	LoadContents();
}

void BrushPanel::OnSwitchOut()
{
	////
}

void BrushPanel::OnClickListBoxRow(wxCommandEvent& event)
{
	ASSERT(tileset->getType() >= TILESET_UNKNOWN && tileset->getType() <= TILESET_HOUSE);
	// We just notify the GUI of the action, it will take care of everything else
	ASSERT(brushbox);
	size_t n = event.GetSelection();


	wxWindow* w = this;
	while((w = w->GetParent()) && dynamic_cast<PaletteWindow*>(w) == nullptr);

	if(w)
		g_gui.ActivatePalette(static_cast<PaletteWindow*>(w));

	g_gui.SelectBrush(tileset->brushlist[n], tileset->getType());
}

// ============================================================================
// BrushIconBox

BEGIN_EVENT_TABLE(BrushIconBox, wxScrolledWindow)
	// Listbox style
	EVT_TOGGLEBUTTON(wxID_ANY, BrushIconBox::OnClickBrushButton)
	EVT_LEFT_DOWN(BrushIconBox::OnMouseDown)
	EVT_MOTION(BrushIconBox::OnMouseMotion)
	EVT_RIGHT_DOWN(BrushIconBox::OnRightClick)
	EVT_MENU(PALETTE_POPUP_MENU_APPLY_REPLACE_BOX1, BrushIconBox::OnApplyReplaceBox1)
	EVT_MENU(PALETTE_POPUP_MENU_APPLY_REPLACE_BOX2, BrushIconBox::OnApplyReplaceBox2)
END_EVENT_TABLE()

BrushIconBox::BrushIconBox(wxWindow *parent, const TilesetCategory *_tileset, RenderSize rsz) :
	wxScrolledWindow(parent, wxID_ANY, wxDefaultPosition, wxDefaultSize, wxVSCROLL),
	BrushBoxInterface(_tileset),
	icon_size(rsz),
	dragging(false)
{
	ASSERT(tileset->getType() >= TILESET_UNKNOWN && tileset->getType() <= TILESET_HOUSE);
	int width;
	if(icon_size == RENDER_SIZE_32x32) {
		width = std::max(g_settings.getInteger(Config::PALETTE_COL_COUNT) / 2 + 1, 1);
	} else {
		width = std::max(g_settings.getInteger(Config::PALETTE_COL_COUNT) + 1, 1);
	}

	// Create buttons
	wxSizer* stacksizer = newd wxBoxSizer(wxVERTICAL);
	wxSizer* rowsizer = nullptr;
	int item_counter = 0;
	for(BrushVector::const_iterator iter = tileset->brushlist.begin(); iter != tileset->brushlist.end(); ++iter) {
		ASSERT(*iter);
		++item_counter;

		if(!rowsizer) {
			rowsizer = newd wxBoxSizer(wxHORIZONTAL);
		}

		BrushButton* bb = newd BrushButton(this, *iter, rsz);
		rowsizer->Add(bb);
		brush_buttons.push_back(bb);

		if(item_counter % width == 0) { // newd row
			stacksizer->Add(rowsizer);
			rowsizer = nullptr;
		}
	}
	if(rowsizer) {
		stacksizer->Add(rowsizer);
	}

	SetScrollbars(20,20, 8, item_counter/width, 0, 0);
	SetSizer(stacksizer);
}

BrushIconBox::~BrushIconBox()
{
	////
}

void BrushIconBox::SelectFirstBrush()
{
	if(tileset && tileset->size() > 0) {
		DeselectAll();
		brush_buttons[0]->SetValue(true);
		EnsureVisible((size_t)0);
	}
}

Brush* BrushIconBox::GetSelectedBrush() const
{
	if(!tileset) {
		return nullptr;
	}

	for(std::vector<BrushButton*>::const_iterator it = brush_buttons.begin(); it != brush_buttons.end(); ++it) {
		if((*it)->GetValue()) {
			return (*it)->brush;
		}
	}
	return nullptr;
}

bool BrushIconBox::SelectBrush(const Brush* whatbrush)
{
	DeselectAll();
	for(std::vector<BrushButton*>::iterator it = brush_buttons.begin(); it != brush_buttons.end(); ++it) {
		if((*it)->brush == whatbrush) {
			(*it)->SetValue(true);
			EnsureVisible(*it);
			return true;
		}
	}
	return false;
}

void BrushIconBox::DeselectAll()
{
	for(std::vector<BrushButton*>::iterator it = brush_buttons.begin(); it != brush_buttons.end(); ++it) {
		(*it)->SetValue(false);
	}
}

void BrushIconBox::EnsureVisible(BrushButton* btn)
{
	int windowSizeX, windowSizeY;
	GetVirtualSize(&windowSizeX, &windowSizeY);

	int scrollUnitX;
	int scrollUnitY;
	GetScrollPixelsPerUnit(&scrollUnitX, &scrollUnitY);

	wxRect rect = btn->GetRect();
	int y;
	CalcUnscrolledPosition(0, rect.y, nullptr, &y);

	int maxScrollPos = windowSizeY / scrollUnitY;
	int scrollPosY = std::min(maxScrollPos, (y / scrollUnitY));

	int startScrollPosY;
	GetViewStart(nullptr, &startScrollPosY);

	int clientSizeX, clientSizeY;
	GetClientSize(&clientSizeX, &clientSizeY);
	int endScrollPosY = startScrollPosY + clientSizeY / scrollUnitY;

	if(scrollPosY < startScrollPosY || scrollPosY > endScrollPosY){
		//only scroll if the button isnt visible
		Scroll(-1, scrollPosY);
	}
}

void BrushIconBox::EnsureVisible(size_t n)
{
	EnsureVisible(brush_buttons[n]);
}

void BrushIconBox::OnClickBrushButton(wxCommandEvent& event)
{
	wxObject* obj = event.GetEventObject();
	BrushButton* btn = dynamic_cast<BrushButton*>(obj);
	if(btn) {
		wxWindow* w = this;
		while((w = w->GetParent()) && dynamic_cast<PaletteWindow*>(w) == nullptr);
		if(w)
			g_gui.ActivatePalette(static_cast<PaletteWindow*>(w));
		g_gui.SelectBrush(btn->brush, tileset->getType());
	}
}

void BrushIconBox::OnMouseDown(wxMouseEvent& event)
{
	drag_start_pos = event.GetPosition();
	dragging = false;
	event.Skip();
}

void BrushIconBox::OnMouseMotion(wxMouseEvent& event)
{
	if(event.Dragging() && event.LeftIsDown()) {
		if(!dragging) {
			int dx = abs(event.GetPosition().x - drag_start_pos.x);
			int dy = abs(event.GetPosition().y - drag_start_pos.y);
			if(dx > 3 || dy > 3) {
				dragging = true;

				Brush* brush = GetSelectedBrush();
				if(brush && brush->isRaw()) {
					RAWBrush* raw = brush->asRaw();
					if(raw) {
						uint16_t itemId = raw->getItemID();
						wxString data = wxString::Format("ITEM_ID:%d", itemId);
						wxTextDataObject dragData(data);
						wxDropSource dragSource(this);
						dragSource.SetData(dragData);
						dragSource.DoDragDrop(wxDrag_CopyOnly);
					}
				}
			}
		}
	} else {
		dragging = false;
	}
	event.Skip();
}

void BrushIconBox::OnRightClick(wxMouseEvent& event)
{
	Brush* brush = GetSelectedBrush();
	if(brush && brush->isRaw()) {
		wxMenu menu;
		menu.Append(PALETTE_POPUP_MENU_APPLY_REPLACE_BOX1, "Apply to Replace Box 1");
		menu.Append(PALETTE_POPUP_MENU_APPLY_REPLACE_BOX2, "Apply to Replace Box 2");
		PopupMenu(&menu, event.GetPosition());
	}
}

void BrushIconBox::OnApplyReplaceBox1(wxCommandEvent& WXUNUSED(event))
{
	Brush* brush = GetSelectedBrush();
	if(brush && brush->isRaw()) {
		RAWBrush* raw = brush->asRaw();
		if(raw) {
			uint16_t itemId = raw->getItemID();
			MapTab* tab = g_gui.GetCurrentMapTab();
			if(tab) {
				MapWindow* window = dynamic_cast<MapWindow*>(tab);
				if(window) {
					window->ApplyItemToReplaceBox(itemId, 1);
				}
			}
		}
	}
}

void BrushIconBox::OnApplyReplaceBox2(wxCommandEvent& WXUNUSED(event))
{
	Brush* brush = GetSelectedBrush();
	if(brush && brush->isRaw()) {
		RAWBrush* raw = brush->asRaw();
		if(raw) {
			uint16_t itemId = raw->getItemID();
			MapTab* tab = g_gui.GetCurrentMapTab();
			if(tab) {
				MapWindow* window = dynamic_cast<MapWindow*>(tab);
				if(window) {
					window->ApplyItemToReplaceBox(itemId, 2);
				}
			}
		}
	}
}

// ============================================================================
// BrushListBox

BEGIN_EVENT_TABLE(BrushListBox, wxVListBox)
	EVT_KEY_DOWN(BrushListBox::OnKey)
	EVT_LEFT_DOWN(BrushListBox::OnMouseDown)
	EVT_MOTION(BrushListBox::OnMouseMotion)
	EVT_RIGHT_DOWN(BrushListBox::OnRightClick)
	EVT_MENU(PALETTE_POPUP_MENU_APPLY_REPLACE_BOX1, BrushListBox::OnApplyReplaceBox1)
	EVT_MENU(PALETTE_POPUP_MENU_APPLY_REPLACE_BOX2, BrushListBox::OnApplyReplaceBox2)
END_EVENT_TABLE()

BrushListBox::BrushListBox(wxWindow* parent, const TilesetCategory* tileset) :
	wxVListBox(parent, wxID_ANY, wxDefaultPosition, wxDefaultSize, wxLB_SINGLE),
	BrushBoxInterface(tileset),
	icon_pixel_size(GetConfiguredListIconSize()),
	dragging(false)
{
	SetBackgroundColour(kPaletteListBackgroundColour);
	SetOwnForegroundColour(kPaletteListTextColour);
	SetItemCount(tileset->size());
}

BrushListBox::~BrushListBox()
{
	////
}

void BrushListBox::SelectFirstBrush()
{
	SetSelection(0);
	wxWindow::ScrollLines(-1);
}

Brush* BrushListBox::GetSelectedBrush() const
{
	if(!tileset) {
		return nullptr;
	}

	int n = GetSelection();
	if(n != wxNOT_FOUND) {
		return tileset->brushlist[n];
	} else if(tileset->size() > 0) {
		return tileset->brushlist[0];
	}
	return nullptr;
}

bool BrushListBox::SelectBrush(const Brush* whatbrush)
{
	for(size_t n = 0; n < tileset->size(); ++n) {
		if(tileset->brushlist[n] == whatbrush) {
			SetSelection(n);
			return true;
		}
	}
	return false;
}

void BrushListBox::OnDrawItem(wxDC& dc, const wxRect& rect, size_t n) const
{
	ASSERT(n < tileset->size());
	dc.SetPen(*wxTRANSPARENT_PEN);
	if(IsSelected(n)) {
		dc.SetBrush(wxBrush(kPaletteListSelectionColour));
	} else {
		dc.SetBrush(wxBrush(kPaletteListBackgroundColour));
	}
	dc.DrawRectangle(rect);

	const int padding = 4;
	const int icon_x = rect.GetX() + padding;
	const int icon_y = rect.GetY() + std::max(0, (rect.GetHeight() - icon_pixel_size) / 2);
	Sprite* spr = g_gui.gfx.getSprite(tileset->brushlist[n]->getLookID());
	if(spr) {
		spr->DrawTo(&dc, SPRITE_SIZE_32x32, icon_x, icon_y, icon_pixel_size, icon_pixel_size);
	}
	if(IsSelected(n)) {
		dc.SetTextForeground(wxColor(0xFF, 0xFF, 0xFF));
	} else {
		dc.SetTextForeground(kPaletteListTextColour);
	}
	const int text_x = icon_x + icon_pixel_size + padding;
	const int char_height = dc.GetCharHeight();
	const int text_y = rect.GetY() + std::max(0, (rect.GetHeight() - char_height) / 2);
	dc.DrawText(wxstr(tileset->brushlist[n]->getName()), text_x, text_y);
}

wxCoord BrushListBox::OnMeasureItem(size_t n) const
{
	const int padding = 4;
	return icon_pixel_size + padding * 2;
}

void BrushListBox::OnKey(wxKeyEvent& event)
{
	switch(event.GetKeyCode()) {
		case WXK_UP:
		case WXK_DOWN:
		case WXK_LEFT:
		case WXK_RIGHT:
			if(g_settings.getInteger(Config::LISTBOX_EATS_ALL_EVENTS)) {
		case WXK_PAGEUP:
		case WXK_PAGEDOWN:
		case WXK_HOME:
		case WXK_END:
			event.Skip(true);
			} else {
			[[fallthrough]];
		default:
			if(g_gui.GetCurrentTab() != nullptr) {
				g_gui.GetCurrentMapTab()->GetEventHandler()->AddPendingEvent(event);
			}
		}
	}
}

void BrushListBox::OnMouseDown(wxMouseEvent& event)
{
	drag_start_pos = event.GetPosition();
	dragging = false;
	event.Skip();
}

void BrushListBox::OnMouseMotion(wxMouseEvent& event)
{
	if(event.Dragging() && event.LeftIsDown()) {
		if(!dragging) {
			int dx = abs(event.GetPosition().x - drag_start_pos.x);
			int dy = abs(event.GetPosition().y - drag_start_pos.y);
			if(dx > 3 || dy > 3) {
				dragging = true;

				int n = GetSelection();
				if(n != wxNOT_FOUND && tileset && (size_t)n < tileset->size()) {
					Brush* brush = tileset->brushlist[n];
					if(brush && brush->isRaw()) {
						RAWBrush* raw = brush->asRaw();
						if(raw) {
							uint16_t itemId = raw->getItemID();
							wxString data = wxString::Format("ITEM_ID:%d", itemId);
							wxTextDataObject dragData(data);
							wxDropSource dragSource(this);
							dragSource.SetData(dragData);
							dragSource.DoDragDrop(wxDrag_CopyOnly);
						}
					}
				}
			}
		}
	} else {
		dragging = false;
	}
	event.Skip();
}

void BrushListBox::OnRightClick(wxMouseEvent& event)
{
	int n = GetSelection();
	if(n != wxNOT_FOUND && tileset && (size_t)n < tileset->size()) {
		Brush* brush = tileset->brushlist[n];
		if(brush && brush->isRaw()) {
			wxMenu menu;
			menu.Append(PALETTE_POPUP_MENU_APPLY_REPLACE_BOX1, "Apply to Replace Box 1");
			menu.Append(PALETTE_POPUP_MENU_APPLY_REPLACE_BOX2, "Apply to Replace Box 2");
			PopupMenu(&menu, event.GetPosition());
		}
	}
}

void BrushListBox::OnApplyReplaceBox1(wxCommandEvent& WXUNUSED(event))
{
	int n = GetSelection();
	if(n != wxNOT_FOUND && tileset && (size_t)n < tileset->size()) {
		Brush* brush = tileset->brushlist[n];
		if(brush && brush->isRaw()) {
			RAWBrush* raw = brush->asRaw();
			if(raw) {
				uint16_t itemId = raw->getItemID();
				MapTab* tab = g_gui.GetCurrentMapTab();
				if(tab) {
					MapWindow* window = dynamic_cast<MapWindow*>(tab);
					if(window) {
						window->ApplyItemToReplaceBox(itemId, 1);
					}
				}
			}
		}
	}
}

void BrushListBox::OnApplyReplaceBox2(wxCommandEvent& WXUNUSED(event))
{
	int n = GetSelection();
	if(n != wxNOT_FOUND && tileset && (size_t)n < tileset->size()) {
		Brush* brush = tileset->brushlist[n];
		if(brush && brush->isRaw()) {
			RAWBrush* raw = brush->asRaw();
			if(raw) {
				uint16_t itemId = raw->getItemID();
				MapTab* tab = g_gui.GetCurrentMapTab();
				if(tab) {
					MapWindow* window = dynamic_cast<MapWindow*>(tab);
					if(window) {
						window->ApplyItemToReplaceBox(itemId, 2);
					}
				}
			}
		}
	}
}
