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
#include "hotkey_utils.h"
#include "settings.h"

#include <wx/accel.h>

#include <algorithm>
#include <cctype>
#include <vector>
#include <map>
#include <set>
#include <sstream>

namespace
{
	std::string Trim(const std::string& text)
	{
		if(text.empty())
			return text;
		size_t start = 0;
		while(start < text.size() && std::isspace(static_cast<unsigned char>(text[start]))) {
			++start;
		}
		size_t end = text.size();
		while(end > start && std::isspace(static_cast<unsigned char>(text[end - 1]))) {
			--end;
		}
		return text.substr(start, end - start);
	}

	std::vector<std::string> Tokenize(const std::string& text)
	{
		std::vector<std::string> tokens;
		std::string current;
		for(char ch : text) {
			if(ch == '+') {
				if(!current.empty()) {
					tokens.push_back(Trim(current));
					current.clear();
				} else {
					tokens.emplace_back("+");
				}
			} else {
				current.push_back(ch);
			}
		}
		if(!current.empty()) {
			tokens.push_back(Trim(current));
		}

		return tokens;
	}

	bool StartsWithFKey(const std::string& token, int& outKey)
	{
		if(token.size() < 2 || (token[0] != 'F' && token[0] != 'f'))
			return false;

		int number = 0;
		for(size_t i = 1; i < token.size(); ++i) {
			if(!std::isdigit(static_cast<unsigned char>(token[i])))
				return false;
			number = number * 10 + (token[i] - '0');
		}
		if(number < 1 || number > 24)
			return false;

		outKey = WXK_F1 + (number - 1);
		return true;
	}

	int KeyFromToken(const std::string& token)
	{
		if(token.empty())
			return 0;

		if(token.size() == 1) {
			char ch = token[0];
			if(std::islower(static_cast<unsigned char>(ch)))
				ch = std::toupper(static_cast<unsigned char>(ch));
			return ch;
		}

		std::string upper = token;
		std::transform(upper.begin(), upper.end(), upper.begin(), [](unsigned char ch) { return std::toupper(ch); });

		if(upper == "+")
			return '+';
		if(upper == "PLUS")
			return '+';
		if(upper == "MINUS")
			return '-';
		if(upper == "EQUALS" || upper == "EQUAL")
			return '=';
		if(upper == "SPACE")
			return WXK_SPACE;
		if(upper == "TAB")
			return WXK_TAB;
		if(upper == "ENTER" || upper == "RETURN")
			return WXK_RETURN;
		if(upper == "ESC" || upper == "ESCAPE")
			return WXK_ESCAPE;
		if(upper == "BACKSPACE")
			return WXK_BACK;
		if(upper == "DELETE" || upper == "DEL")
			return WXK_DELETE;
		if(upper == "INSERT" || upper == "INS")
			return WXK_INSERT;
		if(upper == "HOME")
			return WXK_HOME;
		if(upper == "END")
			return WXK_END;
		if(upper == "PGUP" || upper == "PAGEUP")
			return WXK_PAGEUP;
		if(upper == "PGDN" || upper == "PAGEDOWN")
			return WXK_PAGEDOWN;
		if(upper == "UP")
			return WXK_UP;
		if(upper == "DOWN")
			return WXK_DOWN;
		if(upper == "LEFT")
			return WXK_LEFT;
		if(upper == "RIGHT")
			return WXK_RIGHT;
		if(upper == "PERIOD")
			return '.';
		if(upper == "COMMA")
			return ',';
		if(upper == "SLASH")
			return '/';
		if(upper == "BACKSLASH")
			return '\\';

		int fKey = 0;
		if(StartsWithFKey(upper, fKey))
			return fKey;

		return 0;
	}

	std::string KeyToString(int keycode)
	{
		if(keycode >= 'A' && keycode <= 'Z')
			return std::string(1, static_cast<char>(keycode));
		if(keycode >= '0' && keycode <= '9')
			return std::string(1, static_cast<char>(keycode));

		switch(keycode) {
			case '+': return "+";
			case '-': return "-";
			case '=': return "=";
			case '.': return "Period";
			case ',': return "Comma";
			case '/': return "Slash";
			case '\\': return "Backslash";
			case WXK_SPACE: return "Space";
			case WXK_TAB: return "Tab";
			case WXK_RETURN: return "Enter";
			case WXK_ESCAPE: return "Esc";
			case WXK_BACK: return "Backspace";
			case WXK_DELETE: return "Delete";
			case WXK_INSERT: return "Insert";
			case WXK_HOME: return "Home";
			case WXK_END: return "End";
			case WXK_PAGEUP: return "PgUp";
			case WXK_PAGEDOWN: return "PgDn";
			case WXK_UP: return "Up";
			case WXK_DOWN: return "Down";
			case WXK_LEFT: return "Left";
			case WXK_RIGHT: return "Right";
			default:
				break;
		}

		if(keycode >= WXK_F1 && keycode <= WXK_F24) {
			int number = (keycode - WXK_F1) + 1;
			return "F" + std::to_string(number);
		}

		return "";
	}
}

bool ParseHotkeyText(const std::string& text, HotkeyData& out)
{
	std::vector<std::string> tokens = Tokenize(text);
	if(tokens.empty())
		return false;

	int flags = 0;
	int keycode = 0;

	for(const std::string& token : tokens) {
		if(token.empty())
			continue;

		std::string upper = token;
		std::transform(upper.begin(), upper.end(), upper.begin(), [](unsigned char ch) { return std::toupper(ch); });
		if(upper == "CTRL" || upper == "CONTROL") {
			flags |= wxACCEL_CTRL;
			continue;
		}
		if(upper == "SHIFT") {
			flags |= wxACCEL_SHIFT;
			continue;
		}
		if(upper == "ALT") {
			flags |= wxACCEL_ALT;
			continue;
		}
		if(upper == "CMD" || upper == "COMMAND" || upper == "META") {
			flags |= wxACCEL_CMD;
			continue;
		}

		int key = KeyFromToken(token);
		if(key == 0)
			return false;
		keycode = key;
	}

	if(keycode == 0)
		return false;

	out.flags = flags;
	out.keycode = keycode;
	return true;
}

std::string HotkeyToText(const HotkeyData& hotkey)
{
	if(hotkey.keycode == 0)
		return "";

	std::vector<std::string> parts;
	if(hotkey.flags & wxACCEL_CTRL)
		parts.emplace_back("Ctrl");
	if(hotkey.flags & wxACCEL_ALT)
		parts.emplace_back("Alt");
	if(hotkey.flags & wxACCEL_SHIFT)
		parts.emplace_back("Shift");
#ifdef __APPLE__
	if(hotkey.flags & wxACCEL_CMD)
		parts.emplace_back("Cmd");
#endif

	std::string key = KeyToString(hotkey.keycode);
	if(key.empty())
		return "";
	parts.push_back(key);

	std::string result;
	for(size_t i = 0; i < parts.size(); ++i) {
		if(i > 0)
			result += '+';
		result += parts[i];
	}
	return result;
}

bool EventToHotkey(const wxKeyEvent& event, HotkeyData& out)
{
	int keycode = event.GetKeyCode();
	if(keycode == WXK_SHIFT || keycode == WXK_CONTROL || keycode == WXK_ALT || keycode == WXK_RAW_CONTROL)
		return false;
#ifdef WXK_WINDOWS_LEFT
	if(keycode == WXK_WINDOWS_LEFT)
		return false;
#endif
#ifdef WXK_WINDOWS_RIGHT
	if(keycode == WXK_WINDOWS_RIGHT)
		return false;
#endif
#ifdef WXK_COMMAND
	if(keycode == WXK_COMMAND)
		return false;
#endif

	int flags = 0;
	if(event.ControlDown())
		flags |= wxACCEL_CTRL;
	if(event.AltDown())
		flags |= wxACCEL_ALT;
	if(event.ShiftDown())
		flags |= wxACCEL_SHIFT;
#ifdef __APPLE__
	if(event.CmdDown())
		flags |= wxACCEL_CMD;
#endif

	out.flags = flags;
	out.keycode = keycode;
	return true;
}

namespace
{
	struct MouseActionInfo
	{
		MouseActionID id;
		const char* action;
		MouseButtonBinding defaultBinding;
		Config::Key configKey;
		const char* defaultKeyboardHotkey;
	};

	const MouseActionInfo g_mouse_actions[] = {
		{ MouseActionID::PrimaryAction, "Primary Action", MouseButtonBinding::Left, Config::MOUSE_ACTION_PRIMARY_BUTTON, "" },
		{ MouseActionID::Camera, "Camera Drag", MouseButtonBinding::Middle, Config::MOUSE_ACTION_CAMERA_BUTTON, "" },
		{ MouseActionID::Properties, "Properties Tool", MouseButtonBinding::Right, Config::MOUSE_ACTION_PROPERTIES_BUTTON, "" },
	};

	std::map<MouseActionID, std::string> stored_mouse_keyboard_hotkeys;
	bool mouse_keyboard_hotkeys_loaded = false;

	MouseButtonBinding BindingFromInt(int value)
	{
		switch(value) {
			case 0: return MouseButtonBinding::Left;
			case 1: return MouseButtonBinding::Middle;
			case 2: return MouseButtonBinding::Right;
			case 3: return MouseButtonBinding::Button4;
			case 4: return MouseButtonBinding::Button5;
			default: return MouseButtonBinding::Left;
		}
	}

int BindingToInt(MouseButtonBinding binding)
{
	switch(binding) {
		case MouseButtonBinding::Left: return 0;
		case MouseButtonBinding::Middle: return 1;
		case MouseButtonBinding::Right: return 2;
		case MouseButtonBinding::Button4: return 3;
		case MouseButtonBinding::Button5: return 4;
		default: return 0;
	}
}

const MouseActionInfo* FindMouseAction(MouseActionID id)
	{
		for(const MouseActionInfo& info : g_mouse_actions) {
			if(info.id == id)
				return &info;
		}
		return nullptr;
	}

	void EnsureMouseBindingsInitialized()
	{
		const int version = g_settings.getInteger(Config::MOUSE_BINDINGS_VERSION);
		if(version >= 2)
			return;

		if(version <= 0) {
			const bool swapped = g_settings.getInteger(Config::SWITCH_MOUSEBUTTONS) != 0;
			g_settings.setInteger(Config::MOUSE_ACTION_PRIMARY_BUTTON, BindingToInt(MouseButtonBinding::Left));
			g_settings.setInteger(Config::MOUSE_ACTION_CAMERA_BUTTON, BindingToInt(swapped ? MouseButtonBinding::Right : MouseButtonBinding::Middle));
			g_settings.setInteger(Config::MOUSE_ACTION_PROPERTIES_BUTTON, BindingToInt(swapped ? MouseButtonBinding::Middle : MouseButtonBinding::Right));
		}

		g_settings.setInteger(Config::MOUSE_BINDINGS_VERSION, 2);
	}

	void LoadMouseKeyboardHotkeys()
	{
		if(mouse_keyboard_hotkeys_loaded)
			return;

		mouse_keyboard_hotkeys_loaded = true;
		const std::string serialized = g_settings.getString(Config::MOUSE_ACTION_KEYBOARD_HOTKEYS);
		std::istringstream stream(serialized);
		std::string line;
		while(std::getline(stream, line)) {
			line = Trim(line);
			if(line.empty())
				continue;
			size_t delimiter = line.find('=');
			if(delimiter == std::string::npos)
				continue;

			std::string actionName = Trim(line.substr(0, delimiter));
			std::string hotkey = Trim(line.substr(delimiter + 1));

			for(const MouseActionInfo& info : g_mouse_actions) {
				if(actionName == info.action) {
					stored_mouse_keyboard_hotkeys[info.id] = hotkey;
					break;
				}
			}
		}
	}

	void SaveMouseKeyboardHotkeys()
	{
		std::ostringstream output;
		for(const MouseActionInfo& info : g_mouse_actions) {
			auto it = stored_mouse_keyboard_hotkeys.find(info.id);
			if(it == stored_mouse_keyboard_hotkeys.end())
				continue;
			if(it->second.empty())
				continue;

			output << info.action << '=' << it->second << '\n';
		}

		g_settings.setString(Config::MOUSE_ACTION_KEYBOARD_HOTKEYS, output.str());
	}

	std::string ResolveMouseKeyboardHotkey(MouseActionID id)
	{
		LoadMouseKeyboardHotkeys();
		auto it = stored_mouse_keyboard_hotkeys.find(id);
		if(it != stored_mouse_keyboard_hotkeys.end())
			return it->second;

		const MouseActionInfo* info = FindMouseAction(id);
		if(info && info->defaultKeyboardHotkey)
			return info->defaultKeyboardHotkey;
		return "";
	}

	bool IsSwappedLayout()
	{
		const int action = g_settings.getInteger(Config::MOUSE_ACTION_PRIMARY_BUTTON);
		const int camera = g_settings.getInteger(Config::MOUSE_ACTION_CAMERA_BUTTON);
		const int properties = g_settings.getInteger(Config::MOUSE_ACTION_PROPERTIES_BUTTON);
		return action == BindingToInt(MouseButtonBinding::Left) &&
			camera == BindingToInt(MouseButtonBinding::Right) &&
			properties == BindingToInt(MouseButtonBinding::Middle);
	}

	void SyncSwitchPreference()
	{
		EnsureMouseBindingsInitialized();
		g_settings.setInteger(Config::SWITCH_MOUSEBUTTONS, IsSwappedLayout() ? 1 : 0);
	}
}

std::string MouseBindingToText(MouseButtonBinding binding)
{
	switch(binding) {
		case MouseButtonBinding::Left: return "Left Mouse Button";
		case MouseButtonBinding::Middle: return "Middle Mouse Button";
		case MouseButtonBinding::Right: return "Right Mouse Button";
		case MouseButtonBinding::Button4: return "Mouse Button 4";
		case MouseButtonBinding::Button5: return "Mouse Button 5";
		default:
			break;
	}
	return "";
}

std::vector<MouseHotkeyEntry> GetMouseHotkeyEntries()
{
	EnsureMouseBindingsInitialized();

	std::vector<MouseHotkeyEntry> entries;
	entries.reserve(sizeof(g_mouse_actions) / sizeof(g_mouse_actions[0]));

	for(const MouseActionInfo& info : g_mouse_actions) {
		MouseHotkeyEntry entry;
		entry.id = info.id;
		entry.menu = "Mouse";
		entry.action = info.action;
		entry.defaultBinding = info.defaultBinding;
		entry.currentBinding = BindingFromInt(g_settings.getInteger(info.configKey));
		entry.defaultKeyboardHotkey = info.defaultKeyboardHotkey ? info.defaultKeyboardHotkey : "";
		entry.currentKeyboardHotkey = ResolveMouseKeyboardHotkey(info.id);
		entries.push_back(entry);
	}

	return entries;
}

void ApplyMouseHotkeys(const std::vector<MouseHotkeyEntry>& entries)
{
	EnsureMouseBindingsInitialized();
	LoadMouseKeyboardHotkeys();

	for(const MouseHotkeyEntry& entry : entries) {
		const MouseActionInfo* info = FindMouseAction(entry.id);
		if(!info)
			continue;
		g_settings.setInteger(info->configKey, BindingToInt(entry.currentBinding));
		if(entry.currentKeyboardHotkey.empty())
			stored_mouse_keyboard_hotkeys.erase(entry.id);
		else
			stored_mouse_keyboard_hotkeys[entry.id] = entry.currentKeyboardHotkey;
	}

	SaveMouseKeyboardHotkeys();
	SyncSwitchPreference();
}

MouseButtonBinding GetMouseBinding(MouseActionID id)
{
	EnsureMouseBindingsInitialized();
	const MouseActionInfo* info = FindMouseAction(id);
	if(!info)
		return MouseButtonBinding::Left;
	return BindingFromInt(g_settings.getInteger(info->configKey));
}

void SetMouseBinding(MouseActionID id, MouseButtonBinding binding)
{
	EnsureMouseBindingsInitialized();
	const MouseActionInfo* info = FindMouseAction(id);
	if(!info)
		return;

	g_settings.setInteger(info->configKey, BindingToInt(binding));
	SyncSwitchPreference();
}

std::string GetMouseKeyboardHotkey(MouseActionID id)
{
	return ResolveMouseKeyboardHotkey(id);
}

bool MatchMouseKeyboardHotkey(const HotkeyData& hotkey, MouseActionID& outAction)
{
	LoadMouseKeyboardHotkeys();
	std::string text = HotkeyToText(hotkey);
	if(text.empty())
		return false;

	for(const MouseActionInfo& info : g_mouse_actions) {
		std::string current = ResolveMouseKeyboardHotkey(info.id);
		if(current.empty())
			continue;
		if(current == text) {
			outAction = info.id;
			return true;
		}
	}
	return false;
}

int MouseBindingToIndex(MouseButtonBinding binding)
{
	return BindingToInt(binding);
}

MouseButtonBinding MouseBindingFromIndex(int value)
{
	return BindingFromInt(value);
}
