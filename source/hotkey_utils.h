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

#ifndef RME_HOTKEY_UTILS_H_
#define RME_HOTKEY_UTILS_H_

#include "main.h"

#include <string>
#include <vector>
#include <wx/event.h>

struct HotkeyData
{
	int flags = 0;
	int keycode = 0;
};

enum class MouseActionID
{
	PrimaryAction = 0,
	Camera,
	Properties,
	Count,
};

enum class MouseButtonBinding
{
	Left = 0,
	Middle = 1,
	Right = 2,
	Button4 = 3,
	Button5 = 4,
};

struct MouseHotkeyEntry
{
	MouseActionID id;
	std::string menu;
	std::string action;
	MouseButtonBinding defaultBinding;
	MouseButtonBinding currentBinding;
	std::string defaultKeyboardHotkey;
	std::string currentKeyboardHotkey;
};

bool ParseHotkeyText(const std::string& text, HotkeyData& out);
std::string HotkeyToText(const HotkeyData& hotkey);
bool EventToHotkey(const wxKeyEvent& event, HotkeyData& out);
std::string MouseBindingToText(MouseButtonBinding binding);
std::vector<MouseHotkeyEntry> GetMouseHotkeyEntries();
void ApplyMouseHotkeys(const std::vector<MouseHotkeyEntry>& entries);
MouseButtonBinding GetMouseBinding(MouseActionID id);
void SetMouseBinding(MouseActionID id, MouseButtonBinding binding);
std::string GetMouseKeyboardHotkey(MouseActionID id);
bool MatchMouseKeyboardHotkey(const HotkeyData& hotkey, MouseActionID& outAction);
int MouseBindingToIndex(MouseButtonBinding binding);
MouseButtonBinding MouseBindingFromIndex(int value);

#endif
