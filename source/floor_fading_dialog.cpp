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
#include "floor_fading_dialog.h"
#include "settings.h"
#include "gui.h"
#include "main_menubar.h"

BEGIN_EVENT_TABLE(FloorFadingDialog, wxDialog)
	EVT_BUTTON(wxID_OK, FloorFadingDialog::OnOK)
	EVT_BUTTON(wxID_CANCEL, FloorFadingDialog::OnCancel)
END_EVENT_TABLE()

FloorFadingDialog::FloorFadingDialog(wxWindow* parent)
	: wxDialog(parent, wxID_ANY, "Floor Fading Settings", wxDefaultPosition, wxDefaultSize, wxCAPTION | wxCLOSE_BOX)
{
	CreateControls();
	Centre();
}

void FloorFadingDialog::CreateControls()
{
	wxBoxSizer* mainSizer = newd wxBoxSizer(wxVERTICAL);

	// Enabled checkbox
	m_enabledCheck = newd wxCheckBox(this, wxID_ANY, "Enable Floor Fading");
	m_enabledCheck->SetValue(g_settings.getBoolean(Config::FLOOR_FADING));
	mainSizer->Add(m_enabledCheck, 0, wxALL, 10);

	// Mode selection
	wxString modeChoices[] = {
		"Crossfade",
		"Continuous",
		"Fade to Black",
		"Fade Out"
	};
	m_modeRadio = newd wxRadioBox(this, wxID_ANY, "Fade Mode",
		wxDefaultPosition, wxDefaultSize, 4, modeChoices, 1, wxRA_SPECIFY_COLS);
	m_modeRadio->SetSelection(g_settings.getInteger(Config::FLOOR_FADING_MODE));
	m_modeRadio->SetToolTip(
		"Crossfade: Old floor fades out, new floor visible underneath.\n"
		"Continuous: Multiple floors cascade when changing rapidly.\n"
		"Fade to Black: Screen fades to black then reveals new floor.\n"
		"Fade Out: New floor gradually appears from transparent.");
	mainSizer->Add(m_modeRadio, 0, wxLEFT | wxRIGHT | wxEXPAND, 10);

	// Duration slider
	wxStaticBoxSizer* paramBox = newd wxStaticBoxSizer(wxVERTICAL, this, "Parameters");

	wxFlexGridSizer* grid = newd wxFlexGridSizer(3, 5, 10);
	grid->AddGrowableCol(1);

	grid->Add(newd wxStaticText(this, wxID_ANY, "Duration:"), 0, wxALIGN_CENTER_VERTICAL);
	int duration = g_settings.getInteger(Config::FLOOR_FADING_DURATION);
	m_durationSlider = newd wxSlider(this, wxID_ANY, duration, 100, 1000,
		wxDefaultPosition, wxSize(200, -1));
	m_durationSlider->Bind(wxEVT_SLIDER, &FloorFadingDialog::OnDurationSlider, this);
	grid->Add(m_durationSlider, 1, wxEXPAND);
	m_durationLabel = newd wxStaticText(this, wxID_ANY, wxString::Format("%dms", duration),
		wxDefaultPosition, wxSize(50, -1));
	grid->Add(m_durationLabel, 0, wxALIGN_CENTER_VERTICAL);

	// Easing choice
	grid->Add(newd wxStaticText(this, wxID_ANY, "Easing:"), 0, wxALIGN_CENTER_VERTICAL);
	m_easingChoice = newd wxChoice(this, wxID_ANY);
	m_easingChoice->Append("Linear");
	m_easingChoice->Append("Ease Out");
	m_easingChoice->Append("Ease In-Out");
	m_easingChoice->SetSelection(g_settings.getInteger(Config::FLOOR_FADING_EASING));
	grid->Add(m_easingChoice, 1, wxEXPAND);
	grid->AddSpacer(0);

	// Opacity slider
	grid->Add(newd wxStaticText(this, wxID_ANY, "Max Opacity:"), 0, wxALIGN_CENTER_VERTICAL);
	int opacity = g_settings.getInteger(Config::FLOOR_FADING_OPACITY);
	m_opacitySlider = newd wxSlider(this, wxID_ANY, opacity, 50, 100,
		wxDefaultPosition, wxSize(200, -1));
	m_opacitySlider->Bind(wxEVT_SLIDER, &FloorFadingDialog::OnOpacitySlider, this);
	grid->Add(m_opacitySlider, 1, wxEXPAND);
	m_opacityLabel = newd wxStaticText(this, wxID_ANY, wxString::Format("%d%%", opacity),
		wxDefaultPosition, wxSize(50, -1));
	grid->Add(m_opacityLabel, 0, wxALIGN_CENTER_VERTICAL);

	paramBox->Add(grid, 1, wxALL | wxEXPAND, 5);
	mainSizer->Add(paramBox, 0, wxALL | wxEXPAND, 10);

	// Buttons
	wxSizer* btnSizer = newd wxBoxSizer(wxHORIZONTAL);
	btnSizer->Add(newd wxButton(this, wxID_OK, "OK"), 0, wxRIGHT, 5);
	btnSizer->Add(newd wxButton(this, wxID_CANCEL, "Cancel"), 0);
	mainSizer->Add(btnSizer, 0, wxALIGN_RIGHT | wxLEFT | wxRIGHT | wxBOTTOM, 10);

	SetSizerAndFit(mainSizer);
}

void FloorFadingDialog::Apply()
{
	g_settings.setInteger(Config::FLOOR_FADING, m_enabledCheck->GetValue() ? 1 : 0);
	g_settings.setInteger(Config::FLOOR_FADING_MODE, m_modeRadio->GetSelection());
	g_settings.setInteger(Config::FLOOR_FADING_DURATION, m_durationSlider->GetValue());
	g_settings.setInteger(Config::FLOOR_FADING_EASING, m_easingChoice->GetSelection());
	g_settings.setInteger(Config::FLOOR_FADING_OPACITY, m_opacitySlider->GetValue());

	// Sync the menu checkbox with the enabled state
	g_gui.root->GetMainMenuBar()->CheckItem(MenuBar::FLOOR_FADING, m_enabledCheck->GetValue());
}

void FloorFadingDialog::OnOK(wxCommandEvent& WXUNUSED(event))
{
	Apply();
	EndModal(wxID_OK);
}

void FloorFadingDialog::OnCancel(wxCommandEvent& WXUNUSED(event))
{
	EndModal(wxID_CANCEL);
}

void FloorFadingDialog::OnDurationSlider(wxCommandEvent& WXUNUSED(event))
{
	m_durationLabel->SetLabel(wxString::Format("%dms", m_durationSlider->GetValue()));
}

void FloorFadingDialog::OnOpacitySlider(wxCommandEvent& WXUNUSED(event))
{
	m_opacityLabel->SetLabel(wxString::Format("%d%%", m_opacitySlider->GetValue()));
}
