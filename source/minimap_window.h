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

#ifndef RME_MINIMAP_WINDOW_H_
#define RME_MINIMAP_WINDOW_H_

class MinimapCanvas;

class MinimapWindow : public wxPanel {
public:
	MinimapWindow(wxWindow* parent);
	virtual ~MinimapWindow();

	void OnFloorUp(wxCommandEvent& event);
	void OnFloorDown(wxCommandEvent& event);
	void OnClose(wxCloseEvent&);

	void DelayedUpdate();
	void UpdateFloorLabel();

protected:
	MinimapCanvas* canvas;
	wxStaticText* floor_label;
	wxButton* floor_up_button;
	wxButton* floor_down_button;

	DECLARE_EVENT_TABLE()
};

class MinimapCanvas : public wxPanel {
public:
	MinimapCanvas(wxWindow* parent);
	virtual ~MinimapCanvas();

	void OnPaint(wxPaintEvent&);
	void OnEraseBackground(wxEraseEvent&) {}
	void OnMouseDown(wxMouseEvent&);
	void OnMouseUp(wxMouseEvent&);
	void OnMouseMove(wxMouseEvent&);
	void OnMouseWheel(wxMouseEvent&);
	void OnSize(wxSizeEvent&);

	void OnDelayedUpdate(wxTimerEvent& event);
	void OnKey(wxKeyEvent& event);

	void InvalidateCache() { cache_valid = false; }
	void DelayedUpdate();

	double GetZoom() const { return zoom; }

protected:
	void RegenerateBitmap();

	wxTimer	update_timer;
	int last_start_x;
	int last_start_y;
	double zoom;

	wxBitmap cached_bitmap;
	bool cache_valid;
	int cached_floor;
	int cached_center_x;
	int cached_center_y;
	double cached_zoom;

	bool dragging;
	int drag_start_x;
	int drag_start_y;

	DECLARE_EVENT_TABLE()
};

#endif
