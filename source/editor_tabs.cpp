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

#include "editor_tabs.h"
#include "editor.h"
#include "live_tab.h"
#include "theme.h"

#include <wx/aui/tabart.h>
#include <wx/datetime.h>

namespace {

wxColour LerpColour(const wxColour& from, const wxColour& to, double t)
{
	const double clamped = t < 0.0 ? 0.0 : (t > 1.0 ? 1.0 : t);
	const int r = static_cast<int>(from.Red() + (to.Red() - from.Red()) * clamped + 0.5);
	const int g = static_cast<int>(from.Green() + (to.Green() - from.Green()) * clamped + 0.5);
	const int b = static_cast<int>(from.Blue() + (to.Blue() - from.Blue()) * clamped + 0.5);
	return wxColour(r, g, b);
}

double PulseMix()
{
	static const int kPeriodMs = 1200;
	const wxLongLong now = wxGetUTCTimeMillis();
	const int t = static_cast<int>(now.GetValue() % kPeriodMs);
	const double phase = static_cast<double>(t) / static_cast<double>(kPeriodMs);
	if(phase < 0.5) {
		return phase * 2.0;
	}
	return (1.0 - phase) * 2.0;
}

class MapTabArt : public wxAuiSimpleTabArt
{
public:
	wxAuiTabArt* Clone() override
	{
		return new MapTabArt(*this);
	}

	void DrawTab(wxDC& dc,
	             wxWindow* wnd,
	             const wxAuiNotebookPage& pane,
	             const wxRect& in_rect,
	             int close_button_state,
	             wxRect* out_tab_rect,
	             wxRect* out_button_rect,
	             int* x_extent) override
	{
		const ThemeColors& theme = Theme::Dark();
		const wxColour active_green(83, 158, 42);
		const wxColour unsaved_yellow(200, 180, 50);
		const wxColour inactive_grey = theme.surfaceAlt;
		const bool is_active = pane.active;

		bool has_changes = false;
		if(auto* map_tab = dynamic_cast<MapTab*>(pane.window)) {
			Editor* editor = map_tab->GetEditor();
			has_changes = editor && editor->getMap().hasChanged();
		}

		wxColour base_colour = inactive_grey;
		wxColour active_colour = active_green;

		if(has_changes) {
			if(is_active) {
				active_colour = LerpColour(unsaved_yellow, active_green, PulseMix());
			} else {
				base_colour = unsaved_yellow;
			}
		}

		SetColour(base_colour);
		SetActiveColour(active_colour);
		wxAuiSimpleTabArt::DrawTab(dc, wnd, pane, in_rect, close_button_state, out_tab_rect, out_button_rect, x_extent);
	}
};

}

EditorTab::EditorTab()
{
	;
}

EditorTab::~EditorTab()
{
	;
}

BEGIN_EVENT_TABLE(MapTabbook, wxPanel)
	EVT_AUINOTEBOOK_PAGE_CLOSE(wxID_ANY, MapTabbook::OnNotebookPageClose)
	EVT_AUINOTEBOOK_PAGE_CHANGED(wxID_ANY, MapTabbook::OnNotebookPageChanged)
	EVT_TIMER(wxID_ANY, MapTabbook::OnPulseTimer)
END_EVENT_TABLE()

MapTabbook::MapTabbook(wxWindow *parent, wxWindowID id) :
	wxPanel(parent, id, wxDefaultPosition, wxDefaultSize),
	pulse_timer(this)
{
	wxSizer* wxz = newd wxBoxSizer(wxHORIZONTAL);
	notebook = newd wxAuiNotebook(this, wxID_ANY, wxDefaultPosition, wxDefaultSize);
	wxz->Add(notebook, 1, wxEXPAND);
	SetSizerAndFit(wxz);
	const ThemeColors& theme = Theme::Dark();
	SetBackgroundColour(theme.background);
	notebook->SetBackgroundColour(theme.surface);
	notebook->SetForegroundColour(theme.text);
	MapTabArt* art = new MapTabArt();
	art->SetColour(theme.surfaceAlt);
	art->SetActiveColour(wxColour(83, 158, 42));
	notebook->SetArtProvider(art);
}

void MapTabbook::CycleTab(bool forward)
{
	if(!notebook) {
		return;
	}

	int32_t pageCount = notebook->GetPageCount();
	int32_t currentSelection = notebook->GetSelection();

	int32_t selection;
	if(forward) {
		selection = (currentSelection + 1) % pageCount;
	} else {
		selection = (currentSelection - 1 + pageCount) % pageCount;
	}
	notebook->SetSelection(selection);
}

void MapTabbook::OnNotebookPageClose(wxAuiNotebookEvent& event)
{
	EditorTab* editor_tab = GetTab(event.GetInt());

	MapTab* map_tab = dynamic_cast<MapTab*>(editor_tab);
	if(map_tab && map_tab->IsUniqueReference() && map_tab->GetMap()) {
		bool need_refresh = true;
		Editor* editor = map_tab->GetEditor();
		if(editor->IsLive()) {
			if(editor->hasChanges()) {
				SetFocusedTab(event.GetInt());
				if(!g_gui.root->DoQuerySave(false)) {
					need_refresh = false;
					event.Veto();
				}
			}
		} else if(editor->hasChanges()) {
			SetFocusedTab(event.GetInt());
			if(!g_gui.root->DoQuerySave()) {
				need_refresh = false;
				event.Veto();
			}
		}

		if(need_refresh) {
			g_gui.RefreshPalettes(nullptr, false);
			g_gui.UpdateMenus();
		}
		return;
	}

	LiveLogTab* live_tab = dynamic_cast<LiveLogTab*>(editor_tab);
	if(live_tab && live_tab->IsConnected()) {
		event.Veto();
	}
}

void MapTabbook::OnNotebookPageChanged(wxAuiNotebookEvent& evt)
{
	g_gui.UpdateMinimap();
	UpdatePulseState();

	int32_t oldSelection = evt.GetOldSelection();
	int32_t newSelection = evt.GetSelection();

	MapTab* oldMapTab;
	if(oldSelection != -1) {
		oldMapTab = dynamic_cast<MapTab*>(GetTab(oldSelection));
	} else {
		oldMapTab = nullptr;
	}

	MapTab* newMapTab;
	if(newSelection != -1) {
		newMapTab = dynamic_cast<MapTab*>(GetTab(newSelection));
	} else {
		newMapTab = nullptr;
	}

	if(!newMapTab) {
		g_gui.RefreshPalettes(nullptr);
	} else if(!oldMapTab || !oldMapTab->HasSameReference(newMapTab)) {
		g_gui.RefreshPalettes(newMapTab->GetMap());
		g_gui.UpdateMenus();
	}

	if(oldMapTab)
		oldMapTab->VisibilityCheck();
	if(newMapTab)
		newMapTab->VisibilityCheck();
}

void MapTabbook::UpdatePulseState()
{
	bool should_pulse = false;
	EditorTab* editor_tab = GetCurrentTab();
	if(auto* map_tab = dynamic_cast<MapTab*>(editor_tab)) {
		Editor* editor = map_tab->GetEditor();
		should_pulse = editor && editor->getMap().hasChanged();
	}

	if(should_pulse && !pulse_active) {
		pulse_active = true;
		pulse_timer.Start(80);
	} else if(!should_pulse && pulse_active) {
		pulse_active = false;
		pulse_timer.Stop();
		if(notebook) {
			notebook->Refresh();
		}
	}
}

void MapTabbook::OnPulseTimer(wxTimerEvent&)
{
	if(notebook) {
		notebook->Refresh();
	}
}

void MapTabbook::OnAllowNotebookDND(wxAuiNotebookEvent& evt)
{
	evt.Allow();
}

// Wrappers

void MapTabbook::AddTab(EditorTab* tab, bool select)
{
	tab->GetWindow()->Reparent(notebook);
	notebook->AddPage(tab->GetWindow(), tab->GetTitle(), select);
	conv[tab->GetWindow()] = tab;
}

void MapTabbook::SetFocusedTab(int idx)
{
	notebook->SetSelection(idx);
}

EditorTab* MapTabbook::GetInternalTab(int idx)
{
	return conv[notebook->GetPage(idx)];
}

EditorTab* MapTabbook::GetCurrentTab()
{
	if(GetTabCount() == 0 || GetSelection() == -1) {
		return nullptr;
	}
	return dynamic_cast<EditorTab*>(GetInternalTab(GetSelection()));
}

EditorTab* MapTabbook::GetTab(int index)
{
	return GetInternalTab(index);
}

wxWindow* MapTabbook::GetCurrentPage()
{
	if(GetTabCount() == 0) {
		return nullptr;
	}
	return GetCurrentTab()->GetWindow();
}

void MapTabbook::OnSwitchEditorMode(EditorMode mode)
{
	for(int32_t i = 0; i < GetTabCount(); ++i) {
		EditorTab* editorTab = GetTab(i);
		if(editorTab) {
			editorTab->OnSwitchEditorMode(mode);
		}
	}
}

void MapTabbook::SetTabLabel(int idx, wxString label)
{
	if(notebook) {
		notebook->SetPageText(idx, label);
	}
}

void MapTabbook::DeleteTab(int idx)
{
	if(notebook) {
		notebook->DeletePage(idx);
	}
}

int MapTabbook::GetTabCount()
{
	if(notebook) {
		return notebook->GetPageCount();
	}
	return 0;
}

int MapTabbook::GetTabIndex(wxWindow* w)
{
	if(notebook) {
		return notebook->GetPageIndex(w);
	}
	return 0;
}

int MapTabbook::GetSelection()
{
	if(notebook) {
		return notebook->GetSelection();
	}
	return 0;
}
