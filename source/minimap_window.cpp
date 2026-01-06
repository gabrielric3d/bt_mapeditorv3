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

#include "graphics.h"
#include "editor.h"
#include "map.h"

#include "gui.h"
#include "gui_ids.h"
#include "map_display.h"
#include "minimap_window.h"

BEGIN_EVENT_TABLE(MinimapWindow, wxPanel)
	EVT_BUTTON(MINIMAP_FLOOR_UP_BUTTON, MinimapWindow::OnFloorUp)
	EVT_BUTTON(MINIMAP_FLOOR_DOWN_BUTTON, MinimapWindow::OnFloorDown)
	EVT_CLOSE(MinimapWindow::OnClose)
END_EVENT_TABLE()

BEGIN_EVENT_TABLE(MinimapCanvas, wxPanel)
	EVT_LEFT_DOWN(MinimapCanvas::OnMouseDown)
	EVT_LEFT_UP(MinimapCanvas::OnMouseUp)
	EVT_MOTION(MinimapCanvas::OnMouseMove)
	EVT_MOUSEWHEEL(MinimapCanvas::OnMouseWheel)
	EVT_SIZE(MinimapCanvas::OnSize)
	EVT_PAINT(MinimapCanvas::OnPaint)
	EVT_ERASE_BACKGROUND(MinimapCanvas::OnEraseBackground)
	EVT_TIMER(wxID_ANY, MinimapCanvas::OnDelayedUpdate)
	EVT_KEY_DOWN(MinimapCanvas::OnKey)
END_EVENT_TABLE()

MinimapWindow::MinimapWindow(wxWindow* parent) :
	wxPanel(parent, wxID_ANY, wxDefaultPosition, wxSize(205, 155))
{
	wxBoxSizer* main_sizer = new wxBoxSizer(wxVERTICAL);

	// Floor controls panel
	wxPanel* controls_panel = new wxPanel(this, wxID_ANY);
	controls_panel->SetBackgroundColour(wxColour(40, 40, 40));

	wxBoxSizer* controls_sizer = new wxBoxSizer(wxHORIZONTAL);

	floor_label = new wxStaticText(controls_panel, wxID_ANY, "Floor: 7", wxDefaultPosition, wxDefaultSize, wxALIGN_LEFT);
	floor_label->SetForegroundColour(wxColour(200, 200, 200));

	floor_up_button = new wxButton(controls_panel, MINIMAP_FLOOR_UP_BUTTON, wxString::FromUTF8("\xE2\x86\x91"), wxDefaultPosition, wxSize(24, 20));
	floor_down_button = new wxButton(controls_panel, MINIMAP_FLOOR_DOWN_BUTTON, wxString::FromUTF8("\xE2\x86\x93"), wxDefaultPosition, wxSize(24, 20));

	controls_sizer->Add(floor_label, 0, wxALIGN_CENTER_VERTICAL | wxLEFT, 5);
	controls_sizer->AddStretchSpacer();
	controls_sizer->Add(floor_up_button, 0, wxALIGN_CENTER_VERTICAL | wxRIGHT, 2);
	controls_sizer->Add(floor_down_button, 0, wxALIGN_CENTER_VERTICAL | wxRIGHT, 5);

	controls_panel->SetSizer(controls_sizer);

	// Minimap canvas
	canvas = new MinimapCanvas(this);

	main_sizer->Add(controls_panel, 0, wxEXPAND);
	main_sizer->Add(canvas, 1, wxEXPAND);

	SetSizer(main_sizer);
}

MinimapWindow::~MinimapWindow()
{
}

void MinimapWindow::OnFloorUp(wxCommandEvent& event)
{
	if(!g_gui.IsEditorOpen()) return;

	int current_floor = g_gui.GetCurrentFloor();
	if(current_floor > 0) {
		g_gui.ChangeFloor(current_floor - 1);
		UpdateFloorLabel();
		canvas->InvalidateCache();
		canvas->Refresh();
		g_gui.RefreshView();
	}
}

void MinimapWindow::OnFloorDown(wxCommandEvent& event)
{
	if(!g_gui.IsEditorOpen()) return;

	int current_floor = g_gui.GetCurrentFloor();
	if(current_floor < 15) {
		g_gui.ChangeFloor(current_floor + 1);
		UpdateFloorLabel();
		canvas->InvalidateCache();
		canvas->Refresh();
		g_gui.RefreshView();
	}
}

void MinimapWindow::OnClose(wxCloseEvent&)
{
	g_gui.DestroyMinimap();
}

void MinimapWindow::DelayedUpdate()
{
	UpdateFloorLabel();
	canvas->DelayedUpdate();
}

void MinimapWindow::UpdateFloorLabel()
{
	if(!g_gui.IsEditorOpen()) return;

	int floor = g_gui.GetCurrentFloor();
	floor_label->SetLabel(wxString::Format("Floor: %d", floor));
}

// MinimapCanvas implementation

MinimapCanvas::MinimapCanvas(wxWindow* parent) :
	wxPanel(parent, wxID_ANY, wxDefaultPosition, wxDefaultSize),
	update_timer(this),
	zoom(1.0),
	cache_valid(false),
	cached_floor(-1),
	cached_center_x(-1),
	cached_center_y(-1),
	cached_zoom(1.0),
	dragging(false),
	drag_start_x(0),
	drag_start_y(0)
{
}

MinimapCanvas::~MinimapCanvas()
{
}

void MinimapCanvas::OnSize(wxSizeEvent& event)
{
	cache_valid = false;
	Refresh();
}

void MinimapCanvas::DelayedUpdate()
{
	update_timer.Start(g_settings.getInteger(Config::MINIMAP_UPDATE_DELAY), true);
}

void MinimapCanvas::OnDelayedUpdate(wxTimerEvent& event)
{
	cache_valid = false;
	Refresh();
}

void MinimapCanvas::OnPaint(wxPaintEvent& event)
{
	wxBufferedPaintDC pdc(this);

	pdc.SetBackground(*wxBLACK_BRUSH);
	pdc.Clear();

	if(!g_gui.IsEditorOpen()) return;
	if(!g_gui.IsRenderingEnabled()) return;

	Editor& editor = *g_gui.GetCurrentEditor();
	const Map& map = editor.getMap();

	int window_width = GetSize().GetWidth();
	int window_height = GetSize().GetHeight();
	int center_x, center_y;

	MapCanvas* map_canvas = g_gui.GetCurrentMapTab()->GetCanvas();
	map_canvas->GetScreenCenter(&center_x, &center_y);

	int floor = g_gui.GetCurrentFloor();

	bool needs_regenerate = !cache_valid ||
		cached_floor != floor ||
		cached_center_x != center_x ||
		cached_center_y != center_y ||
		cached_zoom != zoom ||
		!cached_bitmap.IsOk() ||
		cached_bitmap.GetWidth() != window_width ||
		cached_bitmap.GetHeight() != window_height;

	if(needs_regenerate) {
		RegenerateBitmap();
	}

	if(cached_bitmap.IsOk()) {
		pdc.DrawBitmap(cached_bitmap, 0, 0, false);
	}

	if(g_settings.getInteger(Config::MINIMAP_VIEW_BOX)) {
		pdc.SetPen(*wxWHITE_PEN);

		int screensize_x, screensize_y;
		int view_scroll_x, view_scroll_y;
		map_canvas->GetViewBox(&view_scroll_x, &view_scroll_y, &screensize_x, &screensize_y);

		int tile_size = int(rme::TileSize / map_canvas->GetZoom());
		int floor_offset = (floor > rme::MapGroundLayer ? 0 : (rme::MapGroundLayer - floor));

		int view_start_x = view_scroll_x / rme::TileSize + floor_offset;
		int view_start_y = view_scroll_y / rme::TileSize + floor_offset;
		int view_end_x = view_start_x + screensize_x / tile_size + 1;
		int view_end_y = view_start_y + screensize_y / tile_size + 1;

		// Apply minimap zoom to view box coordinates
		int box_x = (int)((view_start_x - last_start_x) * zoom);
		int box_y = (int)((view_start_y - last_start_y) * zoom);
		int box_w = (int)((view_end_x - view_start_x) * zoom);
		int box_h = (int)((view_end_y - view_start_y) * zoom);

		pdc.SetBrush(*wxTRANSPARENT_BRUSH);
		pdc.DrawRectangle(box_x, box_y, box_w, box_h);
	}
}

void MinimapCanvas::RegenerateBitmap()
{
	if(!g_gui.IsEditorOpen()) return;

	Editor& editor = *g_gui.GetCurrentEditor();
	const Map& map = editor.getMap();

	int window_width = GetSize().GetWidth();
	int window_height = GetSize().GetHeight();

	if(window_width <= 0 || window_height <= 0) return;

	MapCanvas* map_canvas = g_gui.GetCurrentMapTab()->GetCanvas();
	int center_x, center_y;
	map_canvas->GetScreenCenter(&center_x, &center_y);

	int floor = g_gui.GetCurrentFloor();

	// Calculate map area to display based on zoom
	int map_width = (int)(window_width / zoom);
	int map_height = (int)(window_height / zoom);

	int start_x = center_x - map_width / 2;
	int start_y = center_y - map_height / 2;
	int end_x = center_x + map_width / 2;
	int end_y = center_y + map_height / 2;

	if(start_x < 0) {
		start_x = 0;
		end_x = map_width;
	} else if(end_x > map.getWidth()) {
		start_x = map.getWidth() - map_width;
		end_x = map.getWidth();
	}
	if(start_y < 0) {
		start_y = 0;
		end_y = map_height;
	} else if(end_y > map.getHeight()) {
		start_y = map.getHeight() - map_height;
		end_y = map.getHeight();
	}

	start_x = std::max(start_x, 0);
	start_y = std::max(start_y, 0);
	end_x = std::min(end_x, map.getWidth());
	end_y = std::min(end_y, map.getHeight());

	last_start_x = start_x;
	last_start_y = start_y;

	wxImage image(window_width, window_height);
	image.SetRGB(wxRect(0, 0, window_width, window_height), 0, 0, 0);

	unsigned char* data = image.GetData();

	// Draw tiles with zoom scaling
	for(int map_y = start_y; map_y <= end_y; ++map_y) {
		for(int map_x = start_x; map_x <= end_x; ++map_x) {
			const Tile* tile = map.getTile(map_x, map_y, floor);
			if(tile) {
				uint8_t color = tile->getMiniMapColor();
				if(color) {
					wxColour c = colorFromEightBit(color);

					// Calculate window position based on zoom
					int window_x = (int)((map_x - start_x) * zoom);
					int window_y = (int)((map_y - start_y) * zoom);

					// Calculate pixel size (at least 1 pixel)
					int pixel_size = std::max(1, (int)zoom);

					// Fill the pixel block
					for(int dy = 0; dy < pixel_size && (window_y + dy) < window_height; ++dy) {
						for(int dx = 0; dx < pixel_size && (window_x + dx) < window_width; ++dx) {
							int idx = ((window_y + dy) * window_width + (window_x + dx)) * 3;
							data[idx] = c.Red();
							data[idx + 1] = c.Green();
							data[idx + 2] = c.Blue();
						}
					}
				}
			}
		}
	}

	cached_bitmap = wxBitmap(image);
	cache_valid = true;
	cached_floor = floor;
	cached_center_x = center_x;
	cached_center_y = center_y;
	cached_zoom = zoom;
}

void MinimapCanvas::OnMouseDown(wxMouseEvent& event)
{
	if(!g_gui.IsEditorOpen()) return;

	dragging = true;
	drag_start_x = event.GetX();
	drag_start_y = event.GetY();
	CaptureMouse();
}

void MinimapCanvas::OnMouseUp(wxMouseEvent& event)
{
	if(!g_gui.IsEditorOpen()) return;

	if(dragging) {
		if(HasCapture()) {
			ReleaseMouse();
		}

		int dx = event.GetX() - drag_start_x;
		int dy = event.GetY() - drag_start_y;

		// If it was just a click (no significant movement), center on that position
		if(std::abs(dx) < 3 && std::abs(dy) < 3) {
			int new_map_x = last_start_x + (int)(event.GetX() / zoom);
			int new_map_y = last_start_y + (int)(event.GetY() / zoom);
			g_gui.SetScreenCenterPosition(Position(new_map_x, new_map_y, g_gui.GetCurrentFloor()));
			Refresh();
			g_gui.RefreshView();
		}

		dragging = false;
	}
}

void MinimapCanvas::OnMouseMove(wxMouseEvent& event)
{
	if(!g_gui.IsEditorOpen()) return;

	if(dragging && event.LeftIsDown()) {
		int dx = event.GetX() - drag_start_x;
		int dy = event.GetY() - drag_start_y;

		if(dx != 0 || dy != 0) {
			// Convert pixel movement to map tile movement
			int map_dx = (int)(dx / zoom);
			int map_dy = (int)(dy / zoom);

			if(map_dx != 0 || map_dy != 0) {
				MapCanvas* map_canvas = g_gui.GetCurrentMapTab()->GetCanvas();
				int center_x, center_y;
				map_canvas->GetScreenCenter(&center_x, &center_y);

				// Move in opposite direction of drag
				int new_x = center_x - map_dx;
				int new_y = center_y - map_dy;

				g_gui.SetScreenCenterPosition(Position(new_x, new_y, g_gui.GetCurrentFloor()));

				drag_start_x = event.GetX();
				drag_start_y = event.GetY();

				cache_valid = false;
				Refresh();
				g_gui.RefreshView();
			}
		}
	}
}

void MinimapCanvas::OnMouseWheel(wxMouseEvent& event)
{
	int rotation = event.GetWheelRotation();

	if(rotation > 0) {
		// Zoom in
		if(zoom < 8.0) {
			if(zoom < 1.0) {
				zoom *= 2.0;
			} else {
				zoom += 1.0;
			}
			cache_valid = false;
			Refresh();
		}
	} else if(rotation < 0) {
		// Zoom out
		if(zoom > 0.125) {
			if(zoom <= 1.0) {
				zoom /= 2.0;
			} else {
				zoom -= 1.0;
			}
			cache_valid = false;
			Refresh();
		}
	}
}

void MinimapCanvas::OnKey(wxKeyEvent& event)
{
	if(g_gui.GetCurrentTab() != nullptr) {
		g_gui.GetCurrentMapTab()->GetEventHandler()->AddPendingEvent(event);
	}
}
