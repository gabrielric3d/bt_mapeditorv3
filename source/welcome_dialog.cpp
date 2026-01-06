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
#include "welcome_dialog.h"
#include "settings.h"
#include "preferences.h"
#include "theme.h"
#include <wx/dcbuffer.h>
#include <wx/dcmemory.h>
#include <wx/filename.h>
#include <wx/graphics.h>
#include <wx/display.h>
#include <algorithm>
#include <cmath>
#include <sstream>

wxDEFINE_EVENT(WELCOME_DIALOG_ACTION, wxCommandEvent);
wxDEFINE_EVENT(WELCOME_DIALOG_FAVORITE, wxCommandEvent);

namespace {
	bool PathEquals(const wxString& left, const wxString& right) {
#ifdef __WINDOWS__
		return left.CmpNoCase(right) == 0;
#else
		return left == right;
#endif
	}

	void AddUniquePath(std::vector<wxString>& paths, const wxString& value) {
		if(value.empty()) {
			return;
		}
		for(const wxString& existing : paths) {
			if(PathEquals(existing, value)) {
				return;
			}
		}
		paths.push_back(value);
	}

	std::vector<wxString> LoadFavoriteFiles() {
		std::vector<wxString> favorites;
		std::string raw = g_settings.getString(Config::FAVORITE_FILES);
		std::istringstream stream(raw);
		std::string line;
		while(std::getline(stream, line)) {
			if(!line.empty() && line.back() == '\r') {
				line.pop_back();
			}
			if(!line.empty()) {
				AddUniquePath(favorites, wxstr(line));
			}
		}
		return favorites;
	}

	void SaveFavoriteFiles(const std::vector<wxString>& favorites) {
		std::ostringstream stream;
		for(size_t i = 0; i < favorites.size(); ++i) {
			if(i > 0) {
				stream << "\n";
			}
			stream << nstr(favorites[i]);
		}
		g_settings.setString(Config::FAVORITE_FILES, stream.str());
	}

	bool IsFavoriteFile(const std::vector<wxString>& favorites, const wxString& path) {
		for(const wxString& favorite : favorites) {
			if(PathEquals(favorite, path)) {
				return true;
			}
		}
		return false;
	}

	wxBitmap CreateStarBitmap(wxWindow* window, const wxColour& colour, bool filled) {
		const int size = FROM_DIP(window, 14);
		wxBitmap bitmap(size, size, 32);
		wxMemoryDC dc;
		dc.SelectObject(bitmap);
		dc.SetBackground(*wxTRANSPARENT_BRUSH);
		dc.Clear();
		wxGraphicsContext* gc = wxGraphicsContext::Create(dc);
		if(gc) {
			const double cx = (size - 1) / 2.0;
			const double cy = (size - 1) / 2.0;
			const double outer = size * 0.45;
			const double inner = size * 0.2;
			wxGraphicsPath path = gc->CreatePath();
			for(int i = 0; i < 10; ++i) {
				const double angle = (i * 36.0 - 90.0) * (3.14159265358979323846 / 180.0);
				const double radius = (i % 2 == 0) ? outer : inner;
				const double x = cx + std::cos(angle) * radius;
				const double y = cy + std::sin(angle) * radius;
				if(i == 0) {
					path.MoveToPoint(x, y);
				} else {
					path.AddLineToPoint(x, y);
				}
			}
			path.CloseSubpath();
			gc->SetPen(wxPen(colour, 1));
			if(filled) {
				gc->SetBrush(wxBrush(colour));
			} else {
				gc->SetBrush(*wxTRANSPARENT_BRUSH);
			}
			gc->DrawPath(path);
			delete gc;
		}
		dc.SelectObject(wxNullBitmap);
		return bitmap;
	}
}

WelcomeDialog::WelcomeDialog(const wxString& title_text,
                             const wxString &version_text,
                             const wxSize& size,
                             const wxBitmap &rme_logo,
                             const std::vector<wxString> &recent_files)
        : wxDialog(nullptr, wxID_ANY, __W_RME_APPLICATION_NAME__, wxDefaultPosition, size,
                   wxDEFAULT_DIALOG_STYLE | wxMINIMIZE_BOX | wxMAXIMIZE_BOX | wxRESIZE_BORDER) {
    wxDisplay display;
    if(display.IsOk()) {
        SetSize(display.GetClientArea().GetSize());
    }
    const ThemeColors& theme = Theme::Dark();
    m_welcome_dialog_panel = newd WelcomeDialogPanel(this,
                                                     GetClientSize(),
                                                     title_text,
                                                     version_text,
                                                     theme,
                                                     wxBitmap(rme_logo.ConvertToImage().Scale(FROM_DIP(this, 72), FROM_DIP(this, 72))),
                                                     recent_files);
    Maximize(true);
    Centre();
}

void WelcomeDialog::OnButtonClicked(const wxMouseEvent &event) {
    auto *button = dynamic_cast<WelcomeDialogButton *>(event.GetEventObject());
    wxSize button_size = button->GetSize();
    wxPoint click_point = event.GetPosition();
    if(click_point.x > 0 && click_point.x < button_size.x && click_point.y > 0 && click_point.y < button_size.x) {
        if(button->GetAction() == wxID_PREFERENCES) {
            PreferencesWindow preferences_window(m_welcome_dialog_panel, true);
            preferences_window.ShowModal();
            m_welcome_dialog_panel->updateInputs();
        } else {
            wxCommandEvent action_event(WELCOME_DIALOG_ACTION);
            if(button->GetAction() == wxID_OPEN) {
                wxString wildcard = g_settings.getInteger(Config::USE_OTGZ) != 0 ?
                                    "(*.otbm;*.otgz)|*.otbm;*.otgz" :
                                    "(*.otbm)|*.otbm|Compressed OpenTibia Binary Map (*.otgz)|*.otgz";
                wxFileDialog file_dialog(this, "Open map file", "", "", wildcard, wxFD_OPEN | wxFD_FILE_MUST_EXIST);
                if(file_dialog.ShowModal() == wxID_OK) {
                    action_event.SetString(file_dialog.GetPath());
                } else {
                    return;
                }
            }
            action_event.SetId(button->GetAction());
            ProcessWindowEvent(action_event);
        }
    }
}

void WelcomeDialog::OnCheckboxClicked(const wxCommandEvent &event) {
    g_settings.setInteger(Config::WELCOME_DIALOG, event.GetInt());
}

void WelcomeDialog::OnIgnoreWarningsCheckbox(const wxCommandEvent& event) {
    g_settings.setInteger(Config::IGNORE_WARNINGS_ON_OPEN, event.GetInt());
}

void WelcomeDialog::OnConfirmRecentOpenCheckbox(const wxCommandEvent& event) {
    g_settings.setInteger(Config::CONFIRM_RECENT_OPEN, event.GetInt());
}

void WelcomeDialog::OnRecentItemClicked(const wxMouseEvent &event) {
    auto *recent_item = dynamic_cast<RecentItem *>(event.GetEventObject());
    wxSize button_size = recent_item->GetSize();
    wxPoint click_point = event.GetPosition();
    if(click_point.x > 0 && click_point.x < button_size.x && click_point.y > 0 && click_point.y < button_size.x) {
        if(g_settings.getInteger(Config::CONFIRM_RECENT_OPEN) == 1) {
            wxString message = "Open map?\n\n" + recent_item->GetText();
            int result = wxMessageBox(message, "Confirm Open", wxYES_NO | wxICON_QUESTION, this);
            if(result != wxYES) {
                return;
            }
        }
        wxCommandEvent action_event(WELCOME_DIALOG_ACTION);
        action_event.SetString(recent_item->GetText());
        action_event.SetId(wxID_OPEN);
        ProcessWindowEvent(action_event);
    }
}

WelcomeDialogPanel::WelcomeDialogPanel(WelcomeDialog *dialog,
                                       const wxSize &size,
                                       const wxString &title_text,
                                       const wxString &version_text,
                                       const ThemeColors &theme,
                                       const wxBitmap &rme_logo,
                                       const std::vector<wxString> &recent_files)
        : wxPanel(dialog),
          m_theme(theme),
          m_rme_logo(rme_logo),
          m_title_text(title_text),
          m_version_text(version_text),
          m_text_colour(theme.text),
          m_background_colour(theme.surface),
          m_show_welcome_dialog_checkbox(nullptr),
          m_ignore_warnings_checkbox(nullptr),
          m_confirm_recent_open_checkbox(nullptr),
          m_recent_maps_panel(nullptr) {

    SetBackgroundColour(m_background_colour);

    auto *recent_maps_panel = newd RecentMapsPanel(this,
                                                   dialog,
                                                   theme,
                                                   recent_files);
    recent_maps_panel->SetMaxSize(wxSize(size.x / 2, size.y));
    recent_maps_panel->SetBackgroundColour(theme.surfaceAlt);

    wxSize button_size = FROM_DIP(this, wxSize(150, 35));

    int button_pos_center_x = size.x / 4 - button_size.x / 2;
    int button_pos_center_y = size.y / 2;

    wxPoint newMapButtonPoint(button_pos_center_x, button_pos_center_y);
    auto *new_map_button = newd WelcomeDialogButton(this,
                                                    wxDefaultPosition,
                                                    button_size,
                                                    theme,
                                                    "New");
    new_map_button->SetAction(wxID_NEW);
    new_map_button->Bind(wxEVT_LEFT_UP, &WelcomeDialog::OnButtonClicked, dialog);

    auto *open_map_button = newd WelcomeDialogButton(this,
                                                     wxDefaultPosition,
                                                     button_size,
                                                     theme,
                                                     "Open");
    open_map_button->SetAction(wxID_OPEN);
    open_map_button->Bind(wxEVT_LEFT_UP, &WelcomeDialog::OnButtonClicked, dialog);
    auto *preferences_button = newd WelcomeDialogButton(this,
                                                        wxDefaultPosition,
                                                        button_size,
                                                        theme,
                                                        "Preferences");
    preferences_button->SetAction(wxID_PREFERENCES);
    preferences_button->Bind(wxEVT_LEFT_UP, &WelcomeDialog::OnButtonClicked, dialog);

    Bind(wxEVT_PAINT, &WelcomeDialogPanel::OnPaint, this);

    wxSizer *rootSizer = newd wxBoxSizer(wxHORIZONTAL);
    wxSizer *buttons_sizer = newd wxBoxSizer(wxVERTICAL);
    buttons_sizer->AddSpacer(size.y / 2);
    buttons_sizer->Add(new_map_button, 0, wxALIGN_CENTER | wxTOP, FROM_DIP(this, 10));
    buttons_sizer->Add(open_map_button, 0, wxALIGN_CENTER | wxTOP, FROM_DIP(this, 10));
    buttons_sizer->Add(preferences_button, 0, wxALIGN_CENTER | wxTOP, FROM_DIP(this, 10));

    wxSizer *vertical_sizer = newd wxBoxSizer(wxVERTICAL);
    wxSizer *horizontal_sizer = newd wxBoxSizer(wxHORIZONTAL);
    wxSizer *checkbox_sizer = newd wxBoxSizer(wxVERTICAL);

    m_confirm_recent_open_checkbox = newd wxCheckBox(this, wxID_ANY, "Ask before opening recent maps");
    m_confirm_recent_open_checkbox->SetValue(g_settings.getInteger(Config::CONFIRM_RECENT_OPEN) == 1);
    m_confirm_recent_open_checkbox->Bind(wxEVT_CHECKBOX, &WelcomeDialog::OnConfirmRecentOpenCheckbox, dialog);
    m_confirm_recent_open_checkbox->SetBackgroundColour(m_background_colour);
    m_confirm_recent_open_checkbox->SetForegroundColour(m_theme.textMuted);
    checkbox_sizer->Add(m_confirm_recent_open_checkbox, 0, wxBOTTOM, FROM_DIP(this, 4));

    m_ignore_warnings_checkbox = newd wxCheckBox(this, wxID_ANY, "Do not show warnings when opening maps");
    m_ignore_warnings_checkbox->SetValue(g_settings.getInteger(Config::IGNORE_WARNINGS_ON_OPEN) == 1);
    m_ignore_warnings_checkbox->Bind(wxEVT_CHECKBOX, &WelcomeDialog::OnIgnoreWarningsCheckbox, dialog);
    m_ignore_warnings_checkbox->SetBackgroundColour(m_background_colour);
    m_ignore_warnings_checkbox->SetForegroundColour(m_theme.textMuted);
    checkbox_sizer->Add(m_ignore_warnings_checkbox, 0, wxBOTTOM, FROM_DIP(this, 4));

    m_show_welcome_dialog_checkbox = newd wxCheckBox(this, wxID_ANY, "Show this dialog on startup");
    m_show_welcome_dialog_checkbox->SetValue(g_settings.getInteger(Config::WELCOME_DIALOG) == 1);
    m_show_welcome_dialog_checkbox->Bind(wxEVT_CHECKBOX, &WelcomeDialog::OnCheckboxClicked, dialog);
    m_show_welcome_dialog_checkbox->SetBackgroundColour(m_background_colour);
    m_show_welcome_dialog_checkbox->SetForegroundColour(m_theme.textMuted);
    checkbox_sizer->Add(m_show_welcome_dialog_checkbox, 0, wxTOP, FROM_DIP(this, 4));

    horizontal_sizer->Add(checkbox_sizer, 0, wxALIGN_BOTTOM | wxALL, FROM_DIP(this, 10));
    vertical_sizer->Add(buttons_sizer, 1, wxEXPAND);
    vertical_sizer->Add(horizontal_sizer, 1, wxEXPAND);

    rootSizer->Add(vertical_sizer, 1, wxEXPAND);
    rootSizer->Add(recent_maps_panel, 1, wxEXPAND);
    m_recent_maps_panel = recent_maps_panel;
    SetSizer(rootSizer);
}

void WelcomeDialogPanel::updateInputs() {
    m_show_welcome_dialog_checkbox->SetValue(g_settings.getInteger(Config::WELCOME_DIALOG) == 1);
    if(m_ignore_warnings_checkbox) {
        m_ignore_warnings_checkbox->SetValue(g_settings.getInteger(Config::IGNORE_WARNINGS_ON_OPEN) == 1);
    }
    if(m_confirm_recent_open_checkbox) {
        m_confirm_recent_open_checkbox->SetValue(g_settings.getInteger(Config::CONFIRM_RECENT_OPEN) == 1);
    }
}

void WelcomeDialogPanel::updateRecentFiles(const std::vector<wxString>& recent_files) {
    if(m_recent_maps_panel) {
        m_recent_maps_panel->UpdateRecentFiles(recent_files, LoadFavoriteFiles());
        Layout();
    }
}

void WelcomeDialogPanel::OnPaint(const wxPaintEvent &event) {
    wxAutoBufferedPaintDC dc(this);

    dc.SetBackground(wxBrush(m_background_colour));
    dc.Clear();

    wxGraphicsContext* gc = wxGraphicsContext::Create(dc);
    if(gc) {
        delete gc;
    }

    dc.DrawBitmap(m_rme_logo, wxPoint(GetSize().x / 4 - m_rme_logo.GetWidth() / 2, FROM_DIP(this, 90)), true);

    wxFont font = GetFont();
    font.SetPointSize(18);
    dc.SetFont(font);
    wxSize header_size = dc.GetTextExtent(m_title_text);
    wxSize header_point(GetSize().x / 4, GetSize().y / 4);
    dc.SetTextForeground(m_text_colour);
    dc.DrawText(m_title_text, wxPoint(header_point.x - header_size.x / 2, header_point.y));

    dc.SetFont(GetFont());
    wxSize version_size = dc.GetTextExtent(m_version_text);
    dc.SetTextForeground(m_theme.textMuted);
    const wxPoint versionPoint(header_point.x - version_size.x / 2, header_point.y + header_size.y + 10);
    dc.DrawText(m_version_text, versionPoint);
    dc.SetPen(wxPen(m_theme.accent, 2));
    dc.DrawLine(versionPoint.x, versionPoint.y + version_size.y + 6, versionPoint.x + version_size.x, versionPoint.y + version_size.y + 6);
}

WelcomeDialogButton::WelcomeDialogButton(wxWindow *parent,
                                         const wxPoint &pos,
                                         const wxSize &size,
                                         const ThemeColors &theme,
                                         const wxString &text)
        : wxPanel(parent, wxID_ANY, pos, size),
          m_action(wxID_CLOSE),
          m_text(text),
          m_theme(theme),
          m_text_colour(theme.text),
          m_background(theme.controlBase),
          m_background_hover(theme.controlHover),
          m_is_hover(false) {
    Bind(wxEVT_PAINT, &WelcomeDialogButton::OnPaint, this);
    Bind(wxEVT_ENTER_WINDOW, &WelcomeDialogButton::OnMouseEnter, this);
    Bind(wxEVT_LEAVE_WINDOW, &WelcomeDialogButton::OnMouseLeave, this);
    SetBackgroundColour(theme.controlBase);
    SetForegroundColour(theme.text);
}

void WelcomeDialogButton::OnPaint(const wxPaintEvent &event) {
    wxAutoBufferedPaintDC dc(this);

    wxColour colour = m_is_hover ? m_background_hover : m_background;
    dc.SetBrush(wxBrush(colour));
    dc.SetPen(wxPen(m_theme.border, 1));
    dc.DrawRoundedRectangle(wxRect(wxPoint(0, 0), GetClientSize()), FROM_DIP(this, 4));

    dc.SetFont(GetFont());
    dc.SetTextForeground(m_text_colour);
    wxSize text_size = dc.GetTextExtent(m_text);
    dc.DrawText(m_text, wxPoint(GetSize().x / 2 - text_size.x / 2, GetSize().y / 2 - text_size.y / 2));
}

void WelcomeDialogButton::OnMouseEnter(const wxMouseEvent &event) {
    m_is_hover = true;
    Refresh();
}

void WelcomeDialogButton::OnMouseLeave(const wxMouseEvent &event) {
    m_is_hover = false;
    Refresh();
}

RecentMapsPanel::RecentMapsPanel(wxWindow *parent,
                                 WelcomeDialog *dialog,
                                 const ThemeColors &theme,
                                 const std::vector<wxString> &recent_files)
        : wxPanel(parent, wxID_ANY),
          m_dialog(dialog),
          m_theme(theme),
          m_sizer(new wxBoxSizer(wxVERTICAL)) {
    SetBackgroundColour(theme.surfaceAlt);
    Bind(WELCOME_DIALOG_FAVORITE, &RecentMapsPanel::OnFavoriteClicked, this);
    SetSizer(m_sizer);
    UpdateRecentFiles(recent_files, LoadFavoriteFiles());
}

void RecentMapsPanel::UpdateRecentFiles(const std::vector<wxString>& recent_files,
                                        const std::vector<wxString>& favorite_files) {
    Freeze();
    m_recent_files = recent_files;
    m_sizer->Clear(true);
    std::vector<wxString> unique_favorites;
    for(const wxString& favorite : favorite_files) {
        AddUniquePath(unique_favorites, favorite);
    }
    std::vector<wxString> unique_recent;
    for(const wxString& file : recent_files) {
        if(!IsFavoriteFile(unique_favorites, file)) {
            AddUniquePath(unique_recent, file);
        }
    }

    for(const wxString& file : unique_favorites) {
        auto *recent_item = newd RecentItem(this, m_theme, file, true);
        m_sizer->Add(recent_item, 0, wxEXPAND);
        recent_item->Bind(wxEVT_LEFT_UP, &WelcomeDialog::OnRecentItemClicked, m_dialog);
    }
    if(!unique_favorites.empty() && !unique_recent.empty()) {
        auto *divider = newd wxPanel(this, wxID_ANY);
        divider->SetBackgroundColour(m_theme.border);
        divider->SetMinSize(wxSize(-1, FROM_DIP(this, 1)));
        m_sizer->Add(divider, 0, wxEXPAND | wxLEFT | wxRIGHT | wxTOP | wxBOTTOM, FROM_DIP(this, 8));
    }
    for(const wxString& file : unique_recent) {
        auto *recent_item = newd RecentItem(this, m_theme, file, false);
        m_sizer->Add(recent_item, 0, wxEXPAND);
        recent_item->Bind(wxEVT_LEFT_UP, &WelcomeDialog::OnRecentItemClicked, m_dialog);
    }
    Layout();
    Thaw();
}

RecentItem::RecentItem(wxWindow *parent,
                       const ThemeColors &theme,
                       const wxString &item_name,
                       bool is_favorite)
        : wxPanel(parent, wxID_ANY),
          m_theme(theme),
          m_text_colour(theme.text),
          m_text_colour_hover(theme.accent),
          m_item_text(item_name),
          m_is_favorite(is_favorite) {
    const wxColour favorite_colour(214, 170, 46);
    SetBackgroundColour(theme.surfaceHighlight);
    m_title = newd wxStaticText(this, wxID_ANY, wxFileNameFromPath(m_item_text));
    m_title->SetFont(GetFont().Bold());
    m_title->SetForegroundColour(m_is_favorite ? favorite_colour : m_text_colour);
    m_title->SetToolTip(m_item_text);
    m_file_path = newd wxStaticText(this, wxID_ANY, m_item_text, wxDefaultPosition, wxDefaultSize, wxST_ELLIPSIZE_START);
    m_file_path->SetToolTip(m_item_text);
    m_file_path->SetFont(GetFont().Smaller());
    m_file_path->SetForegroundColour(m_is_favorite ? favorite_colour : theme.textMuted);
    wxString modified_label = "Last modified: -";
    wxFileName file_name(m_item_text);
    if(file_name.FileExists()) {
        wxDateTime modified_time = file_name.GetModificationTime();
        if(modified_time.IsValid()) {
            modified_label = "Last modified: " + modified_time.FormatISODate() + " " + modified_time.FormatISOTime();
        }
    }
    m_modified_text = newd wxStaticText(this, wxID_ANY, modified_label);
    m_modified_text->SetFont(GetFont().Smaller());
    m_modified_text->SetForegroundColour(m_is_favorite ? favorite_colour : theme.textMuted);
    m_star_filled = CreateStarBitmap(this, m_text_colour_hover, true);
    m_star_outline = CreateStarBitmap(this, theme.textMuted, false);
    m_star_outline_hover = CreateStarBitmap(this, m_text_colour_hover, false);
    m_favorite_toggle = newd wxStaticBitmap(this, wxID_ANY,
        m_is_favorite ? m_star_filled : m_star_outline);
    wxBoxSizer *mainSizer = newd wxBoxSizer(wxHORIZONTAL);
    wxBoxSizer *sizer = newd wxBoxSizer(wxVERTICAL);
    sizer->Add(m_title);
    sizer->Add(m_file_path, 1, wxTOP, FROM_DIP(this, 2));
    sizer->Add(m_modified_text, 0, wxTOP, FROM_DIP(this, 2));
    mainSizer->Add(sizer, 0, wxEXPAND | wxALL, FROM_DIP(this, 8));
    mainSizer->AddStretchSpacer(1);
    mainSizer->Add(m_favorite_toggle, 0, wxALIGN_CENTER_VERTICAL | wxRIGHT, FROM_DIP(this, 10));
    Bind(wxEVT_ENTER_WINDOW, &RecentItem::OnMouseEnter, this);
    Bind(wxEVT_LEAVE_WINDOW, &RecentItem::OnMouseLeave, this);
    m_title->Bind(wxEVT_LEFT_UP, &RecentItem::PropagateItemClicked, this);
    m_file_path->Bind(wxEVT_LEFT_UP, &RecentItem::PropagateItemClicked, this);
    m_modified_text->Bind(wxEVT_LEFT_UP, &RecentItem::PropagateItemClicked, this);
    m_favorite_toggle->Bind(wxEVT_LEFT_UP, &RecentItem::OnFavoriteClicked, this);
    SetSizerAndFit(mainSizer);
}

void RecentItem::PropagateItemClicked(wxMouseEvent& event) {
    event.ResumePropagation(1);
    event.SetEventObject(this);
    event.Skip();
}

void RecentItem::OnMouseEnter(const wxMouseEvent &event) {
    if(GetScreenRect().Contains(ClientToScreen(event.GetPosition()))
        && m_title->GetForegroundColour() != m_text_colour_hover) {
        if(m_is_favorite) {
            const wxColour favorite_colour(214, 170, 46);
            m_title->SetForegroundColour(favorite_colour);
            m_file_path->SetForegroundColour(favorite_colour);
            m_modified_text->SetForegroundColour(favorite_colour);
        } else {
            m_title->SetForegroundColour(m_text_colour_hover);
            m_file_path->SetForegroundColour(m_text_colour_hover);
            m_modified_text->SetForegroundColour(m_text_colour_hover);
        }
        if(!m_is_favorite) {
            m_favorite_toggle->SetBitmap(m_star_outline_hover);
        }
        m_title->Refresh();
        m_file_path->Refresh();
        m_modified_text->Refresh();
        m_favorite_toggle->Refresh();
    }
}

void RecentItem::OnMouseLeave(const wxMouseEvent &event) {
    if(!GetScreenRect().Contains(ClientToScreen(event.GetPosition()))
        && m_title->GetForegroundColour() != m_text_colour) {
        if(m_is_favorite) {
            const wxColour favorite_colour(214, 170, 46);
            m_title->SetForegroundColour(favorite_colour);
            m_file_path->SetForegroundColour(favorite_colour);
            m_modified_text->SetForegroundColour(favorite_colour);
        } else {
            m_title->SetForegroundColour(m_text_colour);
            m_file_path->SetForegroundColour(m_theme.textMuted);
            m_modified_text->SetForegroundColour(m_theme.textMuted);
        }
        if(!m_is_favorite) {
            m_favorite_toggle->SetBitmap(m_star_outline);
        }
        m_title->Refresh();
        m_file_path->Refresh();
        m_modified_text->Refresh();
        m_favorite_toggle->Refresh();
    }
}

void RecentItem::OnFavoriteClicked(wxMouseEvent& event) {
    wxCommandEvent favorite_event(WELCOME_DIALOG_FAVORITE);
    favorite_event.SetString(m_item_text);
    favorite_event.SetInt(m_is_favorite ? 0 : 1);
    favorite_event.SetEventObject(this);
    GetParent()->GetEventHandler()->ProcessEvent(favorite_event);
    event.StopPropagation();
}

void RecentMapsPanel::OnFavoriteClicked(wxCommandEvent& event) {
    wxString path = event.GetString();
    std::vector<wxString> favorites = LoadFavoriteFiles();
    if(event.GetInt() != 0) {
        AddUniquePath(favorites, path);
    } else {
        favorites.erase(
            std::remove_if(favorites.begin(), favorites.end(),
                [&path](const wxString& current) { return PathEquals(current, path); }),
            favorites.end());
    }
    SaveFavoriteFiles(favorites);
    UpdateRecentFiles(m_recent_files, favorites);
}
