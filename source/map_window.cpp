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

#include "map_window.h"
#include "gui.h"
#include "sprites.h"
#include "editor.h"

MapWindow::MapWindow(wxWindow* parent, Editor& editor) :
	wxPanel(parent, PANE_MAIN),
	editor(editor),
	advancedReplaceDialog(nullptr),
	preview_mode(false),
	preview_scroll_x(0),
	preview_scroll_y(0),
	preview_width_tiles(0),
	preview_height_tiles(0),
	preview_bounds_valid(false)
{
	int GL_settings[3];
	GL_settings[0] = WX_GL_RGBA;
	GL_settings[1] = WX_GL_DOUBLEBUFFER;
	GL_settings[2] = 0;
	canvas = newd MapCanvas(this, editor, GL_settings);

	vScroll = newd MapScrollBar(this, MAP_WINDOW_VSCROLL, wxVERTICAL, canvas);
	hScroll = newd MapScrollBar(this, MAP_WINDOW_HSCROLL, wxHORIZONTAL, canvas);

	gem = newd DCButton(this, MAP_WINDOW_GEM, wxDefaultPosition, DC_BTN_NORMAL, RENDER_SIZE_16x16, EDITOR_SPRITE_SELECTION_GEM);

	wxFlexGridSizer* topsizer = newd wxFlexGridSizer(2, 0, 0);

	topsizer->AddGrowableCol(0);
	topsizer->AddGrowableRow(0);

	topsizer->Add(canvas, wxSizerFlags(1).Expand());
	topsizer->Add(vScroll, wxSizerFlags(1).Expand());
	topsizer->Add(hScroll, wxSizerFlags(1).Expand());
	topsizer->Add(gem, wxSizerFlags(1));

	SetSizerAndFit(topsizer);
}

MapWindow::~MapWindow()
{
	DestroyAdvancedReplaceDialog();
}

void MapWindow::SetPreviewMode(bool preview)
{
	preview_mode = preview;
	preview_scroll_x = 0;
	preview_scroll_y = 0;

	if(wxSizer* sizer = GetSizer()) {
		if(hScroll) {
			sizer->Show(hScroll, !preview_mode);
		}
		if(vScroll) {
			sizer->Show(vScroll, !preview_mode);
		}
		if(gem) {
			sizer->Show(gem, !preview_mode);
		}
		sizer->Layout();
	}
}

void MapWindow::SetPreviewBounds(int widthTiles, int heightTiles)
{
	preview_width_tiles = std::max(1, widthTiles);
	preview_height_tiles = std::max(1, heightTiles);
	preview_bounds_valid = true;
}

void MapWindow::ShowAdvancedReplaceDialog()
{
	if(advancedReplaceDialog) {
		advancedReplaceDialog->Show();
		advancedReplaceDialog->Raise();
		return;
	}

	advancedReplaceDialog = new AdvancedReplaceDialog(this);
	advancedReplaceDialog->Connect(wxEVT_CLOSE_WINDOW, wxCloseEventHandler(MapWindow::OnAdvancedReplaceDialogClose), NULL, this);
	advancedReplaceDialog->Show();
}

void MapWindow::CloseAdvancedReplaceDialog()
{
	if(advancedReplaceDialog)
		advancedReplaceDialog->Close();
}

void MapWindow::DestroyAdvancedReplaceDialog()
{
	if(advancedReplaceDialog) {
		advancedReplaceDialog->Disconnect(wxEVT_CLOSE_WINDOW, wxCloseEventHandler(MapWindow::OnAdvancedReplaceDialogClose), NULL, this);
		advancedReplaceDialog->Destroy();
		advancedReplaceDialog = nullptr;
	}
}

void MapWindow::OnAdvancedReplaceDialogClose(wxCloseEvent& event)
{
	if(advancedReplaceDialog) {
		advancedReplaceDialog->Hide();
	}
}

void MapWindow::ApplyItemToReplaceBoxOriginal(uint16_t itemId)
{
	// If dialog doesn't exist, create it first
	if(!advancedReplaceDialog) {
		ShowAdvancedReplaceDialog();
	}

	if(advancedReplaceDialog) {
		advancedReplaceDialog->ApplyItemToOriginal(itemId);
	}
}

void MapWindow::ApplyItemToReplaceBoxReplacement(uint16_t itemId)
{
	// If dialog doesn't exist, create it first
	if(!advancedReplaceDialog) {
		ShowAdvancedReplaceDialog();
	}

	if(advancedReplaceDialog) {
		advancedReplaceDialog->ApplyItemToReplacement(itemId);
	}
}

void MapWindow::ApplyBrushToReplaceBoxOriginal(Brush* brush)
{
	// If dialog doesn't exist, create it first
	if(!advancedReplaceDialog) {
		ShowAdvancedReplaceDialog();
	}

	if(advancedReplaceDialog) {
		advancedReplaceDialog->ApplyBrushToOriginal(brush);
	}
}

void MapWindow::ApplyBrushToReplaceBoxReplacement(Brush* brush)
{
	// If dialog doesn't exist, create it first
	if(!advancedReplaceDialog) {
		ShowAdvancedReplaceDialog();
	}

	if(advancedReplaceDialog) {
		advancedReplaceDialog->ApplyBrushToReplacement(brush);
	}
}

void MapWindow::SetSize(int x, int y, bool center)
{
	if(x == 0 || y == 0) return;

	int windowSizeX;
	int windowSizeY;

	canvas->GetSize(&windowSizeX, &windowSizeY);

	hScroll->SetScrollbar(center? (x - windowSizeX)/2 : hScroll->GetThumbPosition(), windowSizeX / x,  x, windowSizeX / x);
	vScroll->SetScrollbar(center? (y - windowSizeY)/2 : vScroll->GetThumbPosition(), windowSizeY / y,  y, windowSizeX / y);
	//wxPanel::SetSize(x, y);
}

void MapWindow::UpdateScrollbars(int nx, int ny)
{
	// nx and ny are size of this window
	hScroll->SetScrollbar(hScroll->GetThumbPosition(), nx / std::max(1, hScroll->GetRange()), std::max(1, hScroll->GetRange()), 96);
	vScroll->SetScrollbar(vScroll->GetThumbPosition(), ny / std::max(1, vScroll->GetRange()), std::max(1, vScroll->GetRange()), 96);
}

void MapWindow::UpdateDialogs(bool show)
{
	if(advancedReplaceDialog)
		advancedReplaceDialog->Show(show);
}

void MapWindow::GetViewStart(int* x, int* y)
{
	int sx = preview_mode ? preview_scroll_x : hScroll->GetThumbPosition();
	int sy = preview_mode ? preview_scroll_y : vScroll->GetThumbPosition();

	*x = sx;
	*y = sy;
}

void MapWindow::GetViewSize(int* x, int* y)
{
	canvas->GetSize(x, y);
	*x *= canvas->GetContentScaleFactor();
	*y *= canvas->GetContentScaleFactor();
}

void MapWindow::FitToMap()
{
	const Map& map = editor.getMap();
	SetSize(map.getWidth() * rme::TileSize, map.getHeight() * rme::TileSize, true);
}

Position MapWindow::GetScreenCenterPosition()
{
	int x, y;
	canvas->GetScreenCenter(&x, &y);
	return Position(x, y, canvas->GetFloor());
}

void MapWindow::SetScreenCenterPosition(const Position& position, bool showIndicator)
{
	if(!position.isValid())
		return;

	int x = position.x * rme::TileSize;
	int y = position.y * rme::TileSize;
	int z = position.z;
	if(position.z < 8) {
		// Compensate for floor offset above ground
		x -= (rme::MapGroundLayer - z) * rme::TileSize;
		y -= (rme::MapGroundLayer - z) * rme::TileSize;
	}

	const Position& center = GetScreenCenterPosition();
	if(previous_position != center) {
		previous_position.x = center.x;
		previous_position.y = center.y;
		previous_position.z = center.z;
	}

	Scroll(x, y, true);
	canvas->ChangeFloor(z);

	if(showIndicator) {
		canvas->ShowPositionIndicator(position);
		Refresh();
	}
}

void MapWindow::SetScreenCenterPosition(double x, double y, int z, bool showIndicator)
{
	if(z < 0 || z > rme::MapMaxLayer) {
		return;
	}

	const int pixel_x = static_cast<int>(std::round(x * rme::TileSize));
	const int pixel_y = static_cast<int>(std::round(y * rme::TileSize));
	int scroll_x = pixel_x;
	int scroll_y = pixel_y;

	if(z < rme::MapGroundLayer) {
		const int offset = (rme::MapGroundLayer - z) * rme::TileSize;
		scroll_x -= offset;
		scroll_y -= offset;
	}

	const Position& center = GetScreenCenterPosition();
	if(previous_position != center) {
		previous_position.x = center.x;
		previous_position.y = center.y;
		previous_position.z = center.z;
	}

	Scroll(scroll_x, scroll_y, true);
	canvas->ChangeFloor(z);

	if(showIndicator) {
		const Position indicator_pos(static_cast<int>(std::round(x)), static_cast<int>(std::round(y)), z);
		canvas->ShowPositionIndicator(indicator_pos);
		Refresh();
	}
}

void MapWindow::GoToPreviousCenterPosition()
{
	SetScreenCenterPosition(previous_position, true);
}

void MapWindow::Scroll(int x, int y, bool center)
{
	if(center) {
		int windowSizeX, windowSizeY;

		canvas->GetSize(&windowSizeX, &windowSizeY);
		const double current_zoom = preview_mode ? canvas->GetZoom() : g_gui.GetCurrentZoom();
		x -= int((windowSizeX * current_zoom) / 2.0);
		y -= int((windowSizeY * current_zoom) / 2.0);
	}

	if(preview_mode) {
		preview_scroll_x = x;
		preview_scroll_y = y;
	} else {
		hScroll->SetThumbPosition(x);
		vScroll->SetThumbPosition(y);
	}
	if(!preview_mode) {
		g_gui.UpdateMinimap();
	}
}

void MapWindow::ScrollRelative(int x, int y)
{
	if(preview_mode) {
		preview_scroll_x += x;
		preview_scroll_y += y;
	} else {
		hScroll->SetThumbPosition(hScroll->GetThumbPosition()+x);
		vScroll->SetThumbPosition(vScroll->GetThumbPosition()+y);
	}
	if(!preview_mode) {
		g_gui.UpdateMinimap();
	}
}

void MapWindow::OnGem(wxCommandEvent& WXUNUSED(event))
{
	if(preview_mode) {
		return;
	}
	g_gui.SwitchMode();
}

void MapWindow::OnSize(wxSizeEvent& event)
{
	UpdateScrollbars(event.GetSize().GetWidth(), event.GetSize().GetHeight());
	event.Skip();
}

void MapWindow::OnScroll(wxScrollEvent& event)
{
	Refresh();
}

void MapWindow::OnScrollLineDown(wxScrollEvent& event)
{
	if(event.GetOrientation() == wxHORIZONTAL)
		ScrollRelative(96,0);
	else
		ScrollRelative(0,96);
	Refresh();
}

void MapWindow::OnScrollLineUp(wxScrollEvent& event)
{
	if(event.GetOrientation() == wxHORIZONTAL)
		ScrollRelative(-96,0);
	else
		ScrollRelative(0,-96);
	Refresh();
}

void MapWindow::OnScrollPageDown(wxScrollEvent& event)
{
	if(event.GetOrientation() == wxHORIZONTAL)
		ScrollRelative(5*96,0);
	else
		ScrollRelative(0,5*96);
	Refresh();
}

void MapWindow::OnScrollPageUp(wxScrollEvent& event)
{
	if(event.GetOrientation() == wxHORIZONTAL)
		ScrollRelative(-5*96,0);
	else
		ScrollRelative(0,-5*96);
	Refresh();
}
