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

#ifndef WELCOME_DIALOG_H
#define WELCOME_DIALOG_H

#include <wx/wx.h>
#include <vector>

#include "theme.h"

wxDECLARE_EVENT(WELCOME_DIALOG_ACTION, wxCommandEvent);
wxDECLARE_EVENT(WELCOME_DIALOG_FAVORITE, wxCommandEvent);

class WelcomeDialogPanel;
class RecentMapsPanel;

class WelcomeDialog : public wxDialog
{
public:
    WelcomeDialog(const wxString& titleText,
            const wxString& versionText,
            const wxSize& size,
            const wxBitmap& rmeLogo,
            const std::vector<wxString> &recentFiles);
    void OnButtonClicked(const wxMouseEvent& event);
    void OnCheckboxClicked(const wxCommandEvent& event);
    void OnIgnoreWarningsCheckbox(const wxCommandEvent& event);
    void OnConfirmRecentOpenCheckbox(const wxCommandEvent& event);
    void OnRecentItemClicked(const wxMouseEvent& event);
private:
    WelcomeDialogPanel* m_welcome_dialog_panel;
};

class WelcomeDialogPanel : public wxPanel
{
public:
    WelcomeDialogPanel(WelcomeDialog* parent,
            const wxSize& size,
            const wxString& title_text,
            const wxString& version_text,
            const ThemeColors& theme,
            const wxBitmap& rme_logo,
            const std::vector<wxString> &recent_files);
    void OnPaint(const wxPaintEvent& event);
    void updateInputs();
    void updateRecentFiles(const std::vector<wxString>& recent_files);
private:
    wxBitmap m_rme_logo;
    wxString m_title_text;
    wxString m_version_text;
    ThemeColors m_theme;
    wxColour m_text_colour;
    wxColour m_background_colour;
    wxCheckBox* m_show_welcome_dialog_checkbox;
    wxCheckBox* m_ignore_warnings_checkbox;
    wxCheckBox* m_confirm_recent_open_checkbox;
    RecentMapsPanel* m_recent_maps_panel;
};

class WelcomeDialogButton : public wxPanel
{
public:
    WelcomeDialogButton(wxWindow* parent, const wxPoint& pos, const wxSize& size, const ThemeColors& theme, const wxString &text);
    void OnPaint(const wxPaintEvent& event);
    void OnMouseEnter(const wxMouseEvent& event);
    void OnMouseLeave(const wxMouseEvent& event);
    wxStandardID GetAction() { return m_action; };
    void SetAction(wxStandardID action) { m_action = action; };
private:
    wxStandardID m_action;
    wxString m_text;
    ThemeColors m_theme;
    wxColour m_text_colour;
    wxColour m_background;
    wxColour m_background_hover;
    bool m_is_hover;
};

class RecentMapsPanel : public wxPanel
{
public:
    RecentMapsPanel(wxWindow* parent,
            WelcomeDialog* dialog,
            const ThemeColors& theme,
            const std::vector<wxString> &recent_files);
    void UpdateRecentFiles(const std::vector<wxString>& recent_files,
            const std::vector<wxString>& favorite_files);
private:
    void OnFavoriteClicked(wxCommandEvent& event);
    WelcomeDialog* m_dialog;
    ThemeColors m_theme;
    wxBoxSizer* m_sizer;
    std::vector<wxString> m_recent_files;
};

class RecentItem : public wxPanel
{
public:
    RecentItem(wxWindow* parent,
            const ThemeColors& theme,
            const wxString &item_name,
            bool is_favorite);
    void OnMouseEnter(const wxMouseEvent& event);
    void OnMouseLeave(const wxMouseEvent& event);
    void PropagateItemClicked(wxMouseEvent& event);
    void OnFavoriteClicked(wxMouseEvent& event);
    wxString GetText() { return m_item_text; };
    bool IsFavorite() const { return m_is_favorite; }
private:
    ThemeColors m_theme;
    wxColour m_text_colour;
    wxColour m_text_colour_hover;
    wxStaticText* m_title;
    wxStaticText* m_file_path;
    wxStaticText* m_modified_text;
    wxStaticBitmap* m_favorite_toggle;
    wxString m_item_text;
    bool m_is_favorite;
    wxBitmap m_star_filled;
    wxBitmap m_star_outline;
    wxBitmap m_star_outline_hover;
};

#endif //WELCOME_DIALOG_H
