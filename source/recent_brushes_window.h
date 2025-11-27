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

#ifndef RME_RECENT_BRUSHES_WINDOW_H_
#define RME_RECENT_BRUSHES_WINDOW_H_

#include "main.h"

#include <map>
#include <vector>

#include "common_windows.h"
#include "tileset.h"

#include <wx/button.h>
#include <wx/scrolwin.h>
#include <wx/stattext.h>
#include <wx/wrapsizer.h>

class Brush;
class RecentBrushButton;

using RecentBrushMap = std::map<TilesetCategoryType, std::vector<const Brush*>>;

class RecentBrushesWindow : public wxPanel
{
public:
	RecentBrushesWindow(wxWindow* parent);

	void UpdateBrushes(const RecentBrushMap& brushes);
	void SetSelectedBrush(const Brush* brush);

private:
	struct CategoryWidgets {
		wxPanel* container;
		wxStaticText* label;
		wxWrapSizer* brush_sizer;
		std::vector<RecentBrushButton*> buttons;
	};

	void BuildInterface();
	void OnClearAll(wxCommandEvent& event);
	wxString GetCategoryLabel(TilesetCategoryType type) const;
	void HideEmptyCategories();

	wxScrolledWindow* scroll_window;
	wxBoxSizer* scroll_sizer;
	wxButton* clear_button;
	const Brush* selected_brush;

	std::map<TilesetCategoryType, CategoryWidgets> categories;
};

#endif
