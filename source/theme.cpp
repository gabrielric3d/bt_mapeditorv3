//////////////////////////////////////////////////////////////////////
// This file is part of Remere's Map Editor
//////////////////////////////////////////////////////////////////////
// Remere's Map Editor is free software: you can redistribute it and/or modify
// it under the terms of the GNU General Public License as published by
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

#include "theme.h"

#include <functional>

namespace {

void ApplyRecursive(wxWindow* window, const std::function<void(wxWindow*)>& fn)
{
	if(!window) {
		return;
	}
	fn(window);
	wxWindowList& children = window->GetChildren();
	for(wxWindowList::compatibility_iterator node = children.GetFirst(); node; node = node->GetNext()) {
		wxWindow* child = node->GetData();
		if(child) {
			ApplyRecursive(child, fn);
		}
	}
}

}

const ThemeColors& Theme::Dark()
{
	static const ThemeColors kDarkTheme = {
		wxColour(13, 15, 19),   // background
        wxColour(20, 23, 30),   // surface
        wxColour(26, 30, 39),   // surfaceAlt
        wxColour(34, 39, 52),   // surfaceHighlight
        wxColour(36, 40, 53),   // controlBase
        wxColour(47, 53, 69),   // controlHover
        wxColour(62, 74, 98),   // controlActive
        wxColour(54, 60, 74),   // border
        wxColour(237, 239, 245),// text
        wxColour(176, 182, 196),// textMuted
        wxColour(91, 140, 255), // accent
        wxColour(70, 97, 150)   // accentSoft
	};
	return kDarkTheme;
}

void Theme::ApplyText(wxWindow* window, bool recursive)
{
	if(!window) {
		return;
	}
	const ThemeColors& theme = Dark();

	if(recursive) {
		ApplyRecursive(window, [&](wxWindow* target) {
			if(target) {
				target->SetForegroundColour(theme.text);
			}
		});
	} else {
		window->SetForegroundColour(theme.text);
	}
}

void Theme::ApplySurface(wxWindow* window, const wxColour& colour, bool recursive)
{
	if(!window) {
		return;
	}

	const ThemeColors& theme = Dark();
	auto applyFn = [&](wxWindow* target) {
		if(!target) {
			return;
		}
		target->SetOwnBackgroundColour(colour);
		target->SetBackgroundColour(colour);
		target->SetOwnForegroundColour(theme.text);
		target->SetForegroundColour(theme.text);
	};

	if(recursive) {
		ApplyRecursive(window, applyFn);
	} else {
		applyFn(window);
	}
}

void Theme::ApplyMenu(wxMenu* menu)
{
	if(!menu) {
		return;
	}
#if wxUSE_OWNER_DRAWN
	const ThemeColors& theme = Dark();
	wxMenuItemList& items = menu->GetMenuItems();
	for(wxMenuItemList::iterator it = items.begin(); it != items.end(); ++it) {
		wxMenuItem* item = *it;
		if(!item) {
			continue;
		}
		item->SetTextColour(theme.text);
		item->SetBackgroundColour(theme.surfaceAlt);
		item->SetOwnerDrawn(true);
		wxMenu* submenu = item->GetSubMenu();
		if(submenu) {
			ApplyMenu(submenu);
		}
	}
#endif
}

void Theme::ApplyMenu(wxMenuBar* menu_bar)
{
	if(!menu_bar) {
		return;
	}
	for(size_t i = 0; i < menu_bar->GetMenuCount(); ++i) {
		ApplyMenu(menu_bar->GetMenu(i));
	}
}
