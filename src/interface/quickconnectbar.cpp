#include "filezilla.h"
#include "quickconnectbar.h"
#include "recentserverlist.h"
#include "commandqueue.h"
#include "state.h"
#include "Options.h"
#include "loginmanager.h"
#include "Mainfrm.h"
#include "asksavepassworddialog.h"
#include "filezillaapp.h"
#include "graphics.h"
#include "textctrlex.h"

#include <wx/control.h>
#include <wx/dcbuffer.h>
#include <wx/graphics.h>
#include <wx/menu.h>
#include <wx/statline.h>

#include <functional>

namespace {

class CModernSplitButton final : public wxControl
{
public:
	CModernSplitButton(wxWindow* parent, wxString label, int actionId, int menuId)
		: wxControl(parent, wxID_ANY, wxDefaultPosition, wxDefaultSize, wxBORDER_NONE | wxWANTS_CHARS)
		, label_(std::move(label))
		, actionId_(actionId)
		, menuId_(menuId)
	{
		SetBackgroundStyle(wxBG_STYLE_PAINT);
		SetCursor(wxCursor(wxCURSOR_HAND));
		SetToolTip(_("Connect to the server or choose a recent connection"));

		wxClientDC dc(this);
		dc.SetFont(GetFont());
		auto const textSize = dc.GetTextExtent(label_);
		SetMinSize(wxSize(textSize.GetWidth() + FromDIP(58), FromDIP(32)));

		Bind(wxEVT_PAINT, &CModernSplitButton::OnPaint, this);
		Bind(wxEVT_ENTER_WINDOW, [this](wxMouseEvent&) {
			hovered_ = true;
			Refresh();
		});
		Bind(wxEVT_LEAVE_WINDOW, [this](wxMouseEvent&) {
			hovered_ = false;
			pressed_ = false;
			Refresh();
		});
		Bind(wxEVT_LEFT_DOWN, [this](wxMouseEvent&) {
			pressed_ = true;
			SetFocus();
			if (!HasCapture()) {
				CaptureMouse();
			}
			Refresh();
		});
		Bind(wxEVT_LEFT_UP, &CModernSplitButton::OnLeftUp, this);
		Bind(wxEVT_KEY_DOWN, &CModernSplitButton::OnKeyDown, this);
		Bind(wxEVT_SET_FOCUS, [this](wxFocusEvent& event) {
			Refresh();
			event.Skip();
		});
		Bind(wxEVT_KILL_FOCUS, [this](wxFocusEvent& event) {
			Refresh();
			event.Skip();
		});
	}

private:
	void Activate(bool menu)
	{
		wxCommandEvent event(wxEVT_BUTTON, menu ? menuId_ : actionId_);
		event.SetEventObject(this);
		ProcessWindowEvent(event);
	}

	void OnLeftUp(wxMouseEvent& event)
	{
		if (HasCapture()) {
			ReleaseMouse();
		}
		bool const activate = pressed_ && GetClientRect().Contains(event.GetPosition());
		pressed_ = false;
		Refresh();
		if (activate) {
			Activate(event.GetX() >= GetClientSize().GetWidth() - FromDIP(34));
		}
	}

	void OnKeyDown(wxKeyEvent& event)
	{
		if (event.GetKeyCode() == WXK_DOWN) {
			Activate(true);
		}
		else if (event.GetKeyCode() == WXK_RETURN || event.GetKeyCode() == WXK_SPACE) {
			Activate(false);
		}
		else {
			event.Skip();
		}
	}

	void OnPaint(wxPaintEvent&)
	{
		wxAutoBufferedPaintDC dc(this);
		dc.SetBackground(wxBrush(GetInterfaceColour(interface_colour::panel)));
		dc.Clear();

		wxRect button = GetClientRect();
		button.Deflate(1);
		auto const fill = hovered_ || pressed_ ? interface_colour::accent_hover : interface_colour::accent;
		dc.SetPen(wxPen(GetInterfaceColour(interface_colour::accent)));
		dc.SetBrush(wxBrush(GetInterfaceColour(fill)));
		dc.DrawRoundedRectangle(button, FromDIP(6));

		int const menuWidth = FromDIP(34);
		int const separator = button.GetRight() - menuWidth;
		dc.SetPen(wxPen(GetInterfaceColour(interface_colour::border)));
		dc.DrawLine(separator, button.GetTop() + FromDIP(5), separator, button.GetBottom() - FromDIP(5));

		auto font = GetFont();
		font.SetWeight(wxFONTWEIGHT_BOLD);
		dc.SetFont(font);
		dc.SetTextForeground(GetInterfaceColour(interface_colour::accent_text));
		wxRect labelRect = button;
		labelRect.SetRight(separator - 1);
		dc.DrawLabel(label_, labelRect, wxALIGN_CENTER);

		int const centreX = separator + menuWidth / 2;
		int const centreY = button.GetTop() + button.GetHeight() / 2;
		dc.SetPen(wxPen(GetInterfaceColour(interface_colour::accent_text), FromDIP(2)));
		dc.DrawLine(centreX - FromDIP(4), centreY - FromDIP(2), centreX, centreY + FromDIP(2));
		dc.DrawLine(centreX, centreY + FromDIP(2), centreX + FromDIP(4), centreY - FromDIP(2));

		if (HasFocus()) {
			wxRect focus = button;
			focus.Deflate(FromDIP(3));
			dc.SetPen(wxPen(GetInterfaceColour(interface_colour::accent_text), 1, wxPENSTYLE_DOT));
			dc.SetBrush(*wxTRANSPARENT_BRUSH);
			dc.DrawRoundedRectangle(focus, FromDIP(4));
		}
	}

	wxString label_;
	int actionId_{};
	int menuId_{};
	bool hovered_{};
	bool pressed_{};
};

class CThemeSelector final : public wxControl
{
public:
	CThemeSelector(wxWindow* parent, std::function<void(interface_appearance)> onChange)
		: wxControl(parent, wxID_ANY, wxDefaultPosition, wxDefaultSize, wxBORDER_NONE | wxWANTS_CHARS)
		, onChange_(std::move(onChange))
	{
		SetBackgroundStyle(wxBG_STYLE_PAINT);
		SetCursor(wxCursor(wxCURSOR_HAND));
		SetMinSize(FromDIP(wxSize(146, 32)));
		SetToolTip(_("Switch between light and dark mode"));

		Bind(wxEVT_PAINT, &CThemeSelector::OnPaint, this);
		Bind(wxEVT_MOTION, [this](wxMouseEvent& event) {
			hoveredSegment_ = event.GetX() < GetClientSize().GetWidth() / 2 ? 0 : 1;
			Refresh();
		});
		Bind(wxEVT_LEAVE_WINDOW, [this](wxMouseEvent&) {
			hoveredSegment_ = -1;
			Refresh();
		});
		Bind(wxEVT_LEFT_UP, [this](wxMouseEvent& event) {
			SetFocus();
			Select(event.GetX() < GetClientSize().GetWidth() / 2 ? interface_appearance::light : interface_appearance::dark);
		});
		Bind(wxEVT_KEY_DOWN, [this](wxKeyEvent& event) {
			if (event.GetKeyCode() == WXK_LEFT) {
				Select(interface_appearance::light);
			}
			else if (event.GetKeyCode() == WXK_RIGHT || event.GetKeyCode() == WXK_SPACE) {
				Select(interface_appearance::dark);
			}
			else {
				event.Skip();
			}
		});
		Bind(wxEVT_SET_FOCUS, [this](wxFocusEvent& event) {
			Refresh();
			event.Skip();
		});
		Bind(wxEVT_KILL_FOCUS, [this](wxFocusEvent& event) {
			Refresh();
			event.Skip();
		});
	}

private:
	void Select(interface_appearance appearance)
	{
		onChange_(appearance);
		Refresh();
	}

	void OnPaint(wxPaintEvent&)
	{
		wxAutoBufferedPaintDC dc(this);
		dc.SetBackground(wxBrush(GetInterfaceColour(interface_colour::panel)));
		dc.Clear();

		wxRect outer = GetClientRect();
		outer.Deflate(1);
		int const selected = IsDarkInterface() ? 1 : 0;
		int const innerPadding = FromDIP(3);
		wxRect active = outer;
		active.Deflate(innerPadding);
		int const firstSegmentWidth = active.GetWidth() / 2;
		if (selected == 0) {
			active.SetWidth(firstSegmentWidth);
		}
		else {
			active.SetX(active.GetX() + firstSegmentWidth);
			active.SetWidth(active.GetWidth() - firstSegmentWidth);
		}

		std::unique_ptr<wxGraphicsContext> graphics(wxGraphicsContext::Create(dc));
		if (graphics) {
			graphics->SetAntialiasMode(wxANTIALIAS_DEFAULT);
			graphics->SetPen(wxPen(GetInterfaceColour(interface_colour::border), 1));
			graphics->SetBrush(wxBrush(GetInterfaceColour(interface_colour::surface_strong)));
			graphics->DrawRoundedRectangle(
				outer.GetX() + 0.5, outer.GetY() + 0.5,
				outer.GetWidth() - 1.0, outer.GetHeight() - 1.0, FromDIP(9));

			graphics->SetPen(*wxTRANSPARENT_PEN);
			graphics->SetBrush(wxBrush(GetInterfaceColour(interface_colour::accent)));
			graphics->DrawRoundedRectangle(
				active.GetX(), active.GetY(), active.GetWidth(), active.GetHeight(), FromDIP(6));

			if (HasFocus()) {
				graphics->SetPen(wxPen(GetInterfaceColour(interface_colour::accent), 1));
				graphics->SetBrush(*wxTRANSPARENT_BRUSH);
				graphics->DrawRoundedRectangle(
					outer.GetX() - 0.5, outer.GetY() - 0.5,
					outer.GetWidth() + 1.0, outer.GetHeight() + 1.0, FromDIP(9));
			}
		}
		graphics.reset();

		auto font = GetFont();
		font.SetWeight(wxFONTWEIGHT_BOLD);
		dc.SetFont(font);
		for (int segment = 0; segment < 2; ++segment) {
			wxRect labelRect = outer;
			labelRect.SetWidth(outer.GetWidth() / 2);
			if (segment == 1) {
				labelRect.Offset(outer.GetWidth() - labelRect.GetWidth(), 0);
			}
			auto const textRole = segment == selected ? interface_colour::accent_text :
				(segment == hoveredSegment_ ? interface_colour::text : interface_colour::muted);
			dc.SetTextForeground(GetInterfaceColour(textRole));
			dc.DrawLabel(segment == 0 ? _("Light") : _("Dark"), labelRect, wxALIGN_CENTER);
		}
	}

	std::function<void(interface_appearance)> onChange_;
	int hoveredSegment_{-1};
};

}

BEGIN_EVENT_TABLE(CQuickconnectBar, wxPanel)
EVT_BUTTON(XRCID("ID_QUICKCONNECT_OK"), CQuickconnectBar::OnQuickconnect)
EVT_BUTTON(XRCID("ID_QUICKCONNECT_DROPDOWN"), CQuickconnectBar::OnQuickconnectDropdown)
EVT_MENU(wxID_ANY, CQuickconnectBar::OnMenu)
EVT_TEXT_ENTER(wxID_ANY, CQuickconnectBar::OnQuickconnect)
END_EVENT_TABLE()

CQuickconnectBar::CQuickconnectBar(CMainFrame & parent)
	: wxPanel(&parent, -1)
	, options_(parent.GetOptions())
	, mainFrame_(parent)
{
	auto sizer = new wxBoxSizer(wxVERTICAL);
	SetSizer(sizer);
#ifndef __WXMAC__
	sizer->Add(new wxStaticLine(this, -1, wxDefaultPosition, wxDefaultSize, wxLI_HORIZONTAL), wxSizerFlags().Expand());
#endif

	DialogLayout layout(&parent);
	auto mainSizer = layout.createFlex(0, 1);
	int const padding = FromDIP(8);
	auto rowSizer = new wxBoxSizer(wxHORIZONTAL);
	sizer->Add(rowSizer, wxSizerFlags().Expand().Border(wxALL, padding));
	rowSizer->Add(mainSizer, wxSizerFlags().Align(wxALIGN_CENTER_VERTICAL));

	auto addLabel = [this, mainSizer](wxString const& text) {
		auto label = new wxStaticText(this, nullID, text);
		label->SetName(L"ninja-muted-label");
		mainSizer->Add(label, wxSizerFlags().Align(wxALIGN_CENTER_VERTICAL));
	};
	auto prepareInput = [this](wxTextCtrl* input) {
		auto size = input->GetMinSize();
		size.SetHeight(wxMax(input->GetBestSize().GetHeight(), FromDIP(30)));
		input->SetMinSize(size);
	};

	addLabel(_("&Host:"));
	m_pHost = new wxTextCtrlEx(this, -1, wxString(), wxDefaultPosition, ConvertDialogToPixels(wxSize(63, -1)), wxTE_PROCESS_ENTER);
	m_pHost->SetToolTip(_("Enter the address of the server. To specify the server protocol, prepend the host with the protocol identifier. If no protocol is specified, the default protocol (ftp://) will be used. You can also enter complete URLs in the form protocol://user:pass@host:port here, the values in the other fields will be overwritten then.\n\nSupported protocols are:\n- ftp:// for normal FTP with optional encryption\n- sftp:// for SSH file transfer protocol\n- ftps:// for FTP over TLS (implicit)\n- ftpes:// for FTP over TLS (explicit)"));
	m_pHost->SetMaxLength(1000);
	prepareInput(m_pHost);
	mainSizer->Add(m_pHost, wxSizerFlags().Align(wxALIGN_CENTER_VERTICAL).Border(wxRIGHT, 5));

	addLabel(_("&Username:"));
	m_pUser = new wxTextCtrlEx(this, -1, wxString(), wxDefaultPosition, layout.defTextCtrlSize, wxTE_PROCESS_ENTER);
	m_pUser->SetMaxLength(1000);
	prepareInput(m_pUser);
	mainSizer->Add(m_pUser, wxSizerFlags().Align(wxALIGN_CENTER_VERTICAL).Border(wxRIGHT, 5));

	addLabel(_("Pass&word:"));
	m_pPass = new wxTextCtrlEx(this, nullID, wxString(), wxDefaultPosition, layout.defTextCtrlSize, wxTE_PROCESS_ENTER|wxTE_PASSWORD);
	m_pPass->SetMaxLength(1000);
	prepareInput(m_pPass);
	mainSizer->Add(m_pPass, wxSizerFlags().Align(wxALIGN_CENTER_VERTICAL).Border(wxRIGHT, 5));

	addLabel(_("&Port:"));
	m_pPort = new wxTextCtrlEx(this, nullID, wxString(), wxDefaultPosition, ConvertDialogToPixels(wxSize(27, -1)), wxTE_PROCESS_ENTER);
	m_pPort->SetToolTip(_("Enter the port on which the server listens. The default for FTP is 21, the default for SFTP is 22."));
	m_pPort->SetMaxLength(5);
	prepareInput(m_pPort);
	mainSizer->Add(m_pPort, wxSizerFlags().Align(wxALIGN_CENTER_VERTICAL).Border(wxRIGHT, 5));

	connectButton_ = new CModernSplitButton(
		this, _("Quickconnect"), XRCID("ID_QUICKCONNECT_OK"), XRCID("ID_QUICKCONNECT_DROPDOWN"));
	mainSizer->Add(connectButton_, wxSizerFlags().Align(wxALIGN_CENTER_VERTICAL));

	rowSizer->AddStretchSpacer(1);
	auto themeSelector = new CThemeSelector(this, [this](interface_appearance appearance) {
		options_.set(OPTION_INTERFACE_APPEARANCE, static_cast<int>(appearance));
		SetInterfaceAppearance(appearance);
	});
	rowSizer->Add(themeSelector, wxSizerFlags().Align(wxALIGN_CENTER_VERTICAL).Border(wxLEFT, FromDIP(10)));

#ifdef __WXMAC__
	// Under OS X default buttons are toplevel window wide, where under Windows / GTK they stop at the parent panel.
	wxTopLevelWindow *tlw = dynamic_cast<wxTopLevelWindow*>(wxGetTopLevelParent(&parent));
	if (tlw) {
		tlw->SetDefaultItem(0);
	}
#endif

	Bind(wxEVT_CHAR_HOOK, [this](wxKeyEvent& event) {
		if (event.GetKeyCode() == WXK_DOWN) {
			wxCommandEvent evt(wxEVT_CHAR_HOOK);
			OnQuickconnectDropdown(evt);
		}
		else {
			event.Skip();
		}
	});

#ifdef __WXMSW__
	Bind(wxEVT_SYS_COLOUR_CHANGED, [this](wxSysColourChangedEvent& evt) {
		// Delay refresh until children had time to process change.
		CallAfter([this](){Refresh();});
		evt.Skip();
	});
#endif

	GetSizer()->Fit(this);
}

void CQuickconnectBar::OnQuickconnect(wxCommandEvent& event)
{
	CState* pState = CContextManager::Get()->GetCurrentContext();
	if (!pState || !pState->engine_) {
		return;
	}

	std::wstring host = m_pHost->GetValue().ToStdWstring();
	std::wstring user = m_pUser->GetValue().ToStdWstring();
	std::wstring pass = m_pPass->GetValue().ToStdWstring();
	std::wstring port = m_pPort->GetValue().ToStdWstring();

	Site site;

	std::wstring error;

	CServerPath path;
	if (!site.ParseUrl(host, port, user, pass, error, path)) {
		wxString msg = _("Could not parse server address:");
		msg += _T("\n");
		msg += error;
		wxMessageBoxEx(msg, _("Syntax error"), wxICON_EXCLAMATION);
		return;
	}

	host = site.Format(ServerFormat::host_only);
	ServerProtocol protocol = site.server.GetProtocol();
	switch (protocol)
	{
	case FTP:
	case UNKNOWN:
		if (CServer::GetProtocolFromPort(site.server.GetPort()) != FTP &&
			CServer::GetProtocolFromPort(site.server.GetPort()) != UNKNOWN)
		{
			host = _T("ftp://") + host;
		}
		break;
	default:
		{
			std::wstring const prefix = site.server.GetPrefixFromProtocol(protocol);
			if (!prefix.empty()) {
				host = prefix + _T("://") + host;
			}
		}
		break;
	}

	m_pHost->SetValue(host);
	if (site.server.GetPort() != site.server.GetDefaultPort(site.server.GetProtocol())) {
		m_pPort->SetValue(wxString::Format(_T("%d"), site.server.GetPort()));
	}
	else {
		m_pPort->ChangeValue(wxString());
	}

	m_pUser->SetValue(site.server.GetUser());
	if (site.credentials.logonType_ != LogonType::anonymous) {
		m_pPass->SetValue(site.credentials.GetPass());
	}
	else {
		m_pPass->ChangeValue(wxString());
	}

	if (protocol == HTTP || protocol == HTTPS || protocol == S3) {
		wxString protocolError = _("Invalid protocol specified. Valid protocols are:\nftp:// for normal FTP with optional encryption,\nsftp:// for SSH file transfer protocol,\nftps:// for FTP over TLS (implicit) and\nftpes:// for FTP over TLS (explicit).");
		wxMessageBoxEx(protocolError, _("Syntax error"), wxICON_EXCLAMATION);
		return;
	}

	if (event.GetId() == 1) {
		site.server.SetBypassProxy(true);
	}

	if (site.credentials.logonType_ != LogonType::anonymous && !CAskSavePasswordDialog::Run(this, options_)) {
		return;
	}

	if (options_.get_int(OPTION_DEFAULT_KIOSKMODE) && site.credentials.logonType_ == LogonType::normal) {
		site.SetLogonType(LogonType::ask);
		mainFrame_.GetLoginManager().RememberPassword(site);
	}
	Bookmark bm;
	bm.m_remoteDir = path;
	if (!mainFrame_.ConnectToSite(site, bm)) {
		return;
	}

	CRecentServerList::SetMostRecentServer(site, mainFrame_.GetLoginManager(), options_);
}

void CQuickconnectBar::OnQuickconnectDropdown(wxCommandEvent& event)
{
	wxMenu* pMenu = new wxMenu;

	// We have to start with id 1 since menu items with id 0 don't work under OS X
	if (options_.get_int(OPTION_FTP_PROXY_TYPE)) {
		pMenu->Append(1, _("Connect bypassing proxy settings"));
	}
	pMenu->Append(2, _("Clear quickconnect bar"));
	pMenu->Append(3, _("Clear history"));

	m_recentServers = CRecentServerList::GetMostRecentServers();
	if (!m_recentServers.empty()) {
		pMenu->AppendSeparator();

		unsigned int i = 0;
		for (auto iter = m_recentServers.cbegin();
			iter != m_recentServers.end();
			++iter, ++i)
		{
			std::wstring name = LabelEscape(iter->Format(ServerFormat::with_user_and_optional_port)).substr(0, 255);
			pMenu->Append(10 + i, name);
		}
	}
	else {
		pMenu->Enable(3, false);
	}

	auto size = connectButton_->GetSize() / 2;
	connectButton_->PopupMenu(pMenu, (event.GetEventType() == wxEVT_CHAR_HOOK) ? wxPoint(size.x, size.y) : wxDefaultPosition);
	delete pMenu;
	m_recentServers.clear();
}

void CQuickconnectBar::OnMenu(wxCommandEvent& event)
{
	const int id = event.GetId();
	if (id == 1) {
		OnQuickconnect(event);
	}
	else if (id == 2) {
		ClearFields();
	}
	else if (id == 3) {
		CRecentServerList::Clear();
	}

	if (id < 10) {
		return;
	}

	unsigned int index = id - 10;
	if (index >= m_recentServers.size()) {
		return;
	}

	auto iter = m_recentServers.cbegin();
	std::advance(iter, index);

	Site site = *iter;
	mainFrame_.ConnectToSite(site, Bookmark());
}

void CQuickconnectBar::ClearFields()
{
	m_pHost->ChangeValue(_T(""));
	m_pPort->SetValue(_T(""));
	m_pUser->SetValue(_T(""));
	m_pPass->SetValue(_T(""));
}
