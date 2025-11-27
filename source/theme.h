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

#ifndef RME_THEME_H_
#define RME_THEME_H_

#include <wx/colour.h>
#include <wx/window.h>
#include <wx/menu.h>

struct ThemeColors
{
    wxColour background;        // Main application background
    wxColour surface;           // Default panel background
    wxColour surfaceAlt;        // Alternative panel shade
    wxColour surfaceHighlight;  // Highlighted/raised surface
    wxColour controlBase;       // Button/toolbar base
    wxColour controlHover;      // Hovered button background
    wxColour controlActive;     // Selected/active control
    wxColour border;            // Divider/border colour
    wxColour text;              // Primary text colour
    wxColour textMuted;         // Secondary text colour
    wxColour accent;            // Accent colour for focus/selection
    wxColour accentSoft;        // Softer accent for gradients and fills
};

class Theme
{
public:
	static const ThemeColors& Dark();
	static void ApplyText(wxWindow* window, bool recursive = false);
	static void ApplySurface(wxWindow* window, const wxColour& colour, bool recursive = false);
	static void ApplyMenu(wxMenu* menu);
	static void ApplyMenu(wxMenuBar* menu_bar);
};

#endif
