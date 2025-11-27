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

#include "recent_brushes_window.h"

#include <algorithm>
#include <array>
#include <wx/statline.h>
#include <wx/wrapsizer.h>
#include <wx/scrolwin.h>
#include <wx/button.h>

#include "brush.h"
#include "gui.h"
#include "sprites.h"
#include <wx/dcbuffer.h>

namespace {

constexpr size_t kMaxButtonsPerCategory = 32;

const std::array<TilesetCategoryType, 7> kCategoryOrder = {
	TILESET_TERRAIN,
	TILESET_RAW,
	TILESET_DOODAD,
	TILESET_ITEM,
	TILESET_CREATURE,
	TILESET_HOUSE,
	TILESET_WAYPOINT,
};

} // namespace

class RecentBrushButton : public ItemButton
{
public:
	RecentBrushButton(wxWindow* parent, const Brush* brush, TilesetCategoryType type) :
		ItemButton(parent, RENDER_SIZE_32x32, brush ? brush->getLookID() : 0, wxID_ANY),
		brush_(brush),
		category_(type),
		highlighted_(false)
	{
		if(brush_) {
			SetToolTip(wxstr(brush_->getName()));
		}
		Bind(wxEVT_BUTTON, &RecentBrushButton::OnPressed, this);
		Bind(wxEVT_PAINT, &RecentBrushButton::OnPaint, this);
	}

	void SetHighlighted(bool active)
	{
		if(highlighted_ == active)
			return;

		highlighted_ = active;
		Refresh();
	}

	const Brush* GetBrush() const { return brush_; }

private:
	void OnPressed(wxCommandEvent& WXUNUSED(event))
	{
		if(brush_) {
			g_gui.ActivateBrush(brush_);
		}
	}

	void OnPaint(wxPaintEvent& event);

	const Brush* brush_;
	TilesetCategoryType category_;
	bool highlighted_;
};

void RecentBrushButton::OnPaint(wxPaintEvent& event)
{
	ItemButton::OnPaint(event);

	if(!highlighted_) {
		return;
	}

	wxClientDC dc(this);
	dc.SetPen(wxPen(wxColour(200, 0, 0), 3));
	dc.SetBrush(*wxTRANSPARENT_BRUSH);
	const wxSize size = GetClientSize();
	dc.DrawRectangle(2, 2, size.GetWidth() - 4, size.GetHeight() - 4);
}

RecentBrushesWindow::RecentBrushesWindow(wxWindow* parent) :
	wxPanel(parent, wxID_ANY),
	selected_brush(nullptr)
{
	auto* root_sizer = newd wxBoxSizer(wxVERTICAL);

	auto* title = newd wxStaticText(this, wxID_ANY, "Recent Brushes");
	wxFont font = title->GetFont();
	font.MakeBold();
	title->SetFont(font);
	root_sizer->Add(title, 0, wxALL, 5);

	scroll_window = newd wxScrolledWindow(this, wxID_ANY, wxDefaultPosition, wxDefaultSize, wxVSCROLL);
	scroll_window->SetScrollRate(5, 5);
	scroll_sizer = newd wxBoxSizer(wxVERTICAL);
	scroll_window->SetSizer(scroll_sizer);
	root_sizer->Add(scroll_window, 1, wxEXPAND | wxLEFT | wxRIGHT, 5);

	clear_button = newd wxButton(this, wxID_ANY, "Clear All");
	clear_button->Bind(wxEVT_BUTTON, &RecentBrushesWindow::OnClearAll, this);
	root_sizer->Add(clear_button, 0, wxALIGN_CENTER | wxALL, 5);

	SetSizerAndFit(root_sizer);

	BuildInterface();
}

void RecentBrushesWindow::BuildInterface()
{
	for(TilesetCategoryType type : kCategoryOrder) {
		auto* container = newd wxPanel(scroll_window, wxID_ANY);
		auto* container_sizer = newd wxBoxSizer(wxVERTICAL);
		container->SetSizer(container_sizer);

		auto* label = newd wxStaticText(container, wxID_ANY, GetCategoryLabel(type));
		wxFont font = label->GetFont();
		font.MakeBold();
		label->SetFont(font);
		container_sizer->Add(label, 0, wxTOP | wxLEFT | wxRIGHT, 3);

		auto* line = newd wxStaticLine(container);
		container_sizer->Add(line, 0, wxEXPAND | wxLEFT | wxRIGHT | wxBOTTOM, 3);

		auto* brush_sizer = newd wxWrapSizer(wxHORIZONTAL);
		container_sizer->Add(brush_sizer, 0, wxEXPAND | wxALL, 2);

		scroll_sizer->Add(container, 0, wxEXPAND | wxBOTTOM, 6);

		CategoryWidgets widgets;
		widgets.container = container;
		widgets.label = label;
		widgets.brush_sizer = brush_sizer;
		categories[type] = widgets;

		container->Hide();
	}
}

void RecentBrushesWindow::UpdateBrushes(const RecentBrushMap& brushes)
{
	for(auto& [type, widgets] : categories) {
		for(RecentBrushButton* button : widgets.buttons) {
			widgets.brush_sizer->Detach(button);
			button->Destroy();
		}
		widgets.buttons.clear();

		auto it = brushes.find(type);
		if(it == brushes.end() || it->second.empty()) {
			widgets.container->Hide();
			continue;
		}

		size_t count = 0;
		for(const Brush* brush : it->second) {
			if(!brush) {
				continue;
			}
			if(count >= kMaxButtonsPerCategory) {
				break;
			}

			auto* button = newd RecentBrushButton(widgets.container, brush, type);
			widgets.brush_sizer->Add(button, 0, wxALL, 2);
			widgets.buttons.push_back(button);
			++count;
		}

		if(!widgets.buttons.empty()) {
			widgets.container->Show();
		} else {
			widgets.container->Hide();
		}
	}

	HideEmptyCategories();
	scroll_window->FitInside();
	Layout();

	SetSelectedBrush(selected_brush);
}

void RecentBrushesWindow::SetSelectedBrush(const Brush* brush)
{
	selected_brush = brush;
	for(auto& entry : categories) {
		for(RecentBrushButton* button : entry.second.buttons) {
			const bool highlighted = selected_brush && button->GetBrush() == selected_brush;
			button->SetHighlighted(highlighted);
		}
	}
}

void RecentBrushesWindow::HideEmptyCategories()
{
	bool any_visible = false;
	for(auto& entry : categories) {
		if(entry.second.container->IsShown()) {
			any_visible = true;
			break;
		}
	}

	clear_button->Enable(any_visible);
}

wxString RecentBrushesWindow::GetCategoryLabel(TilesetCategoryType type) const
{
	switch(type) {
	case TILESET_TERRAIN: return "Terrain";
	case TILESET_DOODAD: return "Doodad";
	case TILESET_ITEM: return "Item";
	case TILESET_RAW: return "RAW";
	case TILESET_CREATURE: return "Creatures";
	case TILESET_HOUSE: return "House";
	case TILESET_WAYPOINT: return "Waypoint";
	default: return "Other";
	}
}

void RecentBrushesWindow::OnClearAll(wxCommandEvent& WXUNUSED(event))
{
	g_gui.ClearRecentBrushes();
}
