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

#ifndef RME_FLOOR_FADING_DIALOG_H_
#define RME_FLOOR_FADING_DIALOG_H_

class FloorFadingDialog : public wxDialog
{
public:
	explicit FloorFadingDialog(wxWindow* parent);

private:
	void CreateControls();
	void Apply();

	void OnOK(wxCommandEvent& event);
	void OnCancel(wxCommandEvent& event);
	void OnDurationSlider(wxCommandEvent& event);
	void OnOpacitySlider(wxCommandEvent& event);

	wxCheckBox* m_enabledCheck;
	wxRadioBox* m_modeRadio;
	wxSlider* m_durationSlider;
	wxStaticText* m_durationLabel;
	wxChoice* m_easingChoice;
	wxSlider* m_opacitySlider;
	wxStaticText* m_opacityLabel;

	DECLARE_EVENT_TABLE()
};

#endif // RME_FLOOR_FADING_DIALOG_H_
