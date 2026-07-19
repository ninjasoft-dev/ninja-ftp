#include "filezilla.h"
#include "filezillaapp.h"
#include "filter_manager.h"
#include "graphics.h"
#include "listingcomparison.h"
#include "Mainfrm.h"
#include "Options.h"
#include "QueueView.h"
#include "themeprovider.h"
#include "toolbar.h"

#include <wx/dcclient.h>

#ifdef __WXMSW__
#include <commctrl.h>
#endif

namespace {
	// Sem NOALIGN, o controle nativo expande cada barra sobre toda a janela.
	constexpr int toolbarStyle = wxTB_FLAT | wxTB_HORIZONTAL |
		wxTB_NODIVIDER | wxTB_NOALIGN;

#ifdef __WXMSW__
	COLORREF ToNativeColour(wxColour const& colour)
	{
		return RGB(colour.Red(), colour.Green(), colour.Blue());
	}

	void FillToolbarRect(HDC dc, RECT const& rect, interface_colour role)
	{
		auto const brush = CreateSolidBrush(
			ToNativeColour(GetInterfaceColour(role)));
		FillRect(dc, &rect, brush);
		DeleteObject(brush);
	}

	bool DrawToggleToolBitmap(
		NMTBCUSTOMDRAW const& draw, bool disabled)
	{
		auto const toolbar = draw.nmcd.hdr.hwndFrom;
		auto const buttonIndex = SendMessageW(toolbar, TB_COMMANDTOINDEX,
			draw.nmcd.dwItemSpec, 0);
		if (buttonIndex < 0) {
			return false;
		}

		TBBUTTON button{};
		if (!SendMessageW(toolbar, TB_GETBUTTON, buttonIndex,
			reinterpret_cast<LPARAM>(&button)) ||
			(button.fsStyle & BTNS_CHECK) == 0 ||
			button.iBitmap == I_IMAGECALLBACK) {
			return false;
		}

		auto imageList = reinterpret_cast<HIMAGELIST>(SendMessageW(
			toolbar, disabled ?
				TB_GETDISABLEDIMAGELIST : TB_GETIMAGELIST, 0, 0));
		if (!imageList && disabled) {
			imageList = reinterpret_cast<HIMAGELIST>(
				SendMessageW(toolbar, TB_GETIMAGELIST, 0, 0));
		}
		if (!imageList) {
			return false;
		}

		int width{};
		int height{};
		if (!ImageList_GetIconSize(imageList, &width, &height)) {
			return false;
		}

		RECT const rect = draw.nmcd.rc;
		int const x = rect.left + (rect.right - rect.left - width) / 2;
		int const y = rect.top + (rect.bottom - rect.top - height) / 2;
		UINT const style = disabled ? ILD_BLEND50 : ILD_TRANSPARENT;
		return ImageList_DrawEx(imageList, button.iBitmap, draw.nmcd.hdc,
			x, y, width, height, CLR_NONE, CLR_NONE, style) != FALSE;
	}

#endif
}

#ifdef __WXMAC__
void fix_toolbar_style(wxFrame& frame);
#endif

CToolBar::CToolBar(CMainFrame& mainFrame, COptions& options)
	: wxPanel(&mainFrame, nullID)
	, COptionChangeEventHandler(this)
	, mainFrame_(mainFrame)
	, options_(options)
{
	SetBackgroundStyle(wxBG_STYLE_PAINT);
#ifdef __WXMAC__
	fix_toolbar_style(mainFrame_);

	// These days, OS X only knows one hardcoded toolbar size
	iconSize_ = wxSize(32, 32);
	if (wxGetApp().GetTopWindow()) {
                double scale = wxGetApp().GetTopWindow()->GetContentScaleFactor();
                iconSize_.Scale(scale, scale);
	}
#else
	iconSize_ = CThemeProvider::GetIconSize(iconSize24, true);
#endif

	localToolBar_ = new wxToolBar(this, nullID, wxDefaultPosition, wxDefaultSize, toolbarStyle);
	remoteToolBar_ = new wxToolBar(this, nullID, wxDefaultPosition, wxDefaultSize, toolbarStyle);
	localToolBar_->SetToolBitmapSize(iconSize_);
	remoteToolBar_->SetToolBitmapSize(iconSize_);

	MakeTools();
	localToolBar_->Realize();
	remoteToolBar_->Realize();
#ifdef __WXMSW__
	localToolBar_->Bind(
		wxEVT_MOTION, &CToolBar::OnToolbarMouseMove, this);
	localToolBar_->Bind(
		wxEVT_LEAVE_WINDOW, &CToolBar::OnToolbarMouseLeave, this);
	remoteToolBar_->Bind(
		wxEVT_MOTION, &CToolBar::OnToolbarMouseMove, this);
	remoteToolBar_->Bind(
		wxEVT_LEAVE_WINDOW, &CToolBar::OnToolbarMouseLeave, this);
#endif

	int const toolbarHeight = wxMax(
		localToolBar_->GetBestSize().GetHeight(),
		remoteToolBar_->GetBestSize().GetHeight()
	);
	int const verticalPadding = FromDIP(4);
	SetMinSize(wxSize(-1, toolbarHeight + 2 * verticalPadding));
	SetSize(wxSize(mainFrame.GetClientSize().GetWidth(), toolbarHeight + 2 * verticalPadding));

	auto const layoutToolbars = [this, verticalPadding] {
		auto const size = GetClientSize();
		int const localWidth = size.GetWidth() / 2;
		int const toolbarAreaHeight = wxMax(0, size.GetHeight() - 2 * verticalPadding);
		localToolBar_->SetSize(0, verticalPadding, localWidth, toolbarAreaHeight);
		remoteToolBar_->SetSize(localWidth, verticalPadding, size.GetWidth() - localWidth, toolbarAreaHeight);
	};
	Bind(wxEVT_SIZE, [layoutToolbars](wxSizeEvent& event) {
		layoutToolbars();
		event.Skip();
	});
	layoutToolbars();
	Bind(wxEVT_PAINT, [this](wxPaintEvent&) {
		wxPaintDC dc(this);
		auto const rect = GetClientRect();
		dc.SetPen(*wxTRANSPARENT_PEN);
		dc.SetBrush(wxBrush(GetInterfaceColour(interface_colour::panel)));
		dc.DrawRectangle(rect);
		dc.SetPen(wxPen(GetInterfaceColour(interface_colour::border)));
		dc.DrawLine(rect.GetWidth() / 2, rect.GetTop(), rect.GetWidth() / 2, rect.GetBottom());
		dc.DrawLine(rect.GetLeft(), rect.GetBottom(), rect.GetRight(), rect.GetBottom());
	});

	CContextManager::Get()->RegisterHandler(this, STATECHANGE_REMOTE_IDLE, true);
	CContextManager::Get()->RegisterHandler(this, STATECHANGE_SERVER, true);
	CContextManager::Get()->RegisterHandler(this, STATECHANGE_SYNC_BROWSE, true);
	CContextManager::Get()->RegisterHandler(this, STATECHANGE_COMPARISON, true);
	CContextManager::Get()->RegisterHandler(this, STATECHANGE_APPLYFILTER, true);

	CContextManager::Get()->RegisterHandler(this, STATECHANGE_QUEUEPROCESSING, false);
	CContextManager::Get()->RegisterHandler(this, STATECHANGE_CHANGEDCONTEXT, false);

	options_.watch(OPTION_SHOW_MESSAGELOG, this);
	options_.watch(OPTION_SHOW_QUEUE, this);
	options_.watch(OPTION_SHOW_TREE_LOCAL, this);
	options_.watch(OPTION_SHOW_TREE_REMOTE, this);
	options_.watch(OPTION_MESSAGELOG_POSITION, this);
	options_.watch(OPTION_INTERFACE_APPEARANCE, this);

	ToggleTool(XRCID("ID_TOOLBAR_FILTER"), CFilterManager::HasActiveFilters());
	ToggleTool(XRCID("ID_TOOLBAR_LOGVIEW"), options_.get_int(OPTION_SHOW_MESSAGELOG) != 0);
	ToggleTool(XRCID("ID_TOOLBAR_QUEUEVIEW"), options_.get_int(OPTION_SHOW_QUEUE) != 0);
	ToggleTool(XRCID("ID_TOOLBAR_LOCALTREEVIEW"), options_.get_int(OPTION_SHOW_TREE_LOCAL) != 0);
	ToggleTool(XRCID("ID_TOOLBAR_REMOTETREEVIEW"), options_.get_int(OPTION_SHOW_TREE_REMOTE) != 0);

	if (options_.get_int(OPTION_MESSAGELOG_POSITION) == 2) {
		HideTool(XRCID("ID_TOOLBAR_LOGVIEW"));
	}
}

CToolBar::~CToolBar()
{
	options_.unwatch_all(this);
	for (auto const& [id, hidden] : hiddenTools_) {
		(void)id;
		delete hidden.tool;
	}
}

void CToolBar::MakeTool(wxToolBar& toolbar, char const* id, std::wstring const& art, wxString const& tooltip, wxString const& help, wxItemKind type)
{
	if (help.empty() && !tooltip.empty()) {
		MakeTool(toolbar, id, art, tooltip, tooltip, type);
		return;
	}

	wxBitmap bmp = CThemeProvider::Get()->CreateBitmap(art, wxART_TOOLBAR, iconSize_, true);
	toolbar.AddTool(XRCID(id), wxString(), bmp, wxBitmap(), type, tooltip, help);
}

void CToolBar::MakeTools()
{
#ifdef __WXMSW__
	MakeTool(*localToolBar_, "ID_TOOLBAR_SITEMANAGER", L"ART_SITEMANAGER", _("Open the Site Manager."), _("Open the Site Manager"), wxITEM_DROPDOWN);
#else
	MakeTool(*localToolBar_, "ID_TOOLBAR_SITEMANAGER", L"ART_SITEMANAGER", _("Open the Site Manager. Right-click for a list of sites."), _("Open the Site Manager"), wxITEM_DROPDOWN);
#endif
	localToolBar_->AddSeparator();
	MakeTool(*localToolBar_, "ID_TOOLBAR_LOGVIEW", L"ART_LOGVIEW", _("Toggles the display of the message log"), wxString(), wxITEM_CHECK);
	MakeTool(*localToolBar_, "ID_TOOLBAR_LOCALTREEVIEW", L"ART_LOCALTREEVIEW", _("Toggles the display of the local directory tree"), wxString(), wxITEM_CHECK);
	MakeTool(*localToolBar_, "ID_TOOLBAR_QUEUEVIEW", L"ART_QUEUEVIEW", _("Toggles the display of the transfer queue"), wxString(), wxITEM_CHECK);
	MakeTool(*localToolBar_, "ID_TOOLBAR_REFRESH", L"ART_REFRESH", _("Refresh the file and folder lists"));

	MakeTool(*remoteToolBar_, "ID_TOOLBAR_REMOTETREEVIEW", L"ART_REMOTETREEVIEW", _("Toggles the display of the remote directory tree"), wxString(), wxITEM_CHECK);
	remoteToolBar_->AddSeparator();
	MakeTool(*remoteToolBar_, "ID_TOOLBAR_PROCESSQUEUE", L"ART_PROCESSQUEUE", _("Toggles processing of the transfer queue"), wxString(), wxITEM_CHECK);
	MakeTool(*remoteToolBar_, "ID_TOOLBAR_CANCEL", L"ART_CANCEL", _("Cancels the current operation"), _("Cancel current operation"));
	MakeTool(*remoteToolBar_, "ID_TOOLBAR_DISCONNECT", L"ART_DISCONNECT", _("Disconnects from the currently visible server"), _("Disconnect from server"));
	MakeTool(*remoteToolBar_, "ID_TOOLBAR_RECONNECT", L"ART_RECONNECT", _("Reconnects to the last used server"));
	remoteToolBar_->AddSeparator();
	MakeTool(*remoteToolBar_, "ID_TOOLBAR_FILTER", L"ART_FILTER", _("Opens the directory listing filter dialog. Right-click to toggle filters.") + L"\n" + _("Files matching a filter rule are removed from directory listings."), _("Filter the directory listings"), wxITEM_CHECK);
	MakeTool(*remoteToolBar_, "ID_TOOLBAR_REFRESH", L"ART_REFRESH", _("Refresh the file and folder lists"));
}

wxToolBar* CToolBar::FindToolBar(int id) const
{
	if (localToolBar_->GetToolPos(id) != wxNOT_FOUND) {
		return localToolBar_;
	}
	if (remoteToolBar_->GetToolPos(id) != wxNOT_FOUND) {
		return remoteToolBar_;
	}
	return nullptr;
}

wxToolBar* CToolBar::GetToolBarForTool(int id) const
{
	auto toolbar = FindToolBar(id);
	if (toolbar) {
		return toolbar;
	}

	auto const hidden = hiddenTools_.find(id);
	return hidden != hiddenTools_.end() ? hidden->second.toolbar : nullptr;
}

void CToolBar::ToggleTool(int id, bool toggle)
{
	selectedTools_[id] = toggle;
	auto toolbar = FindToolBar(id);
	if (!toolbar) {
		return;
	}
	toolbar->ToggleTool(id, toggle);
	toolbar->Refresh();
}

void CToolBar::RefreshToolbars()
{
	localToolBar_->Refresh(false);
	remoteToolBar_->Refresh(false);
}

#ifdef __WXMSW__
void CToolBar::OnToolbarMouseMove(wxMouseEvent& event)
{
	auto const toolbar = dynamic_cast<wxToolBar*>(event.GetEventObject());
	auto const tool = toolbar ?
		toolbar->FindToolForPosition(event.GetX(), event.GetY()) : nullptr;
	int const toolId = tool && tool->IsButton() ? tool->GetId() : wxID_NONE;
	if (toolbar != hoveredToolBar_ || toolId != hoveredToolId_) {
		auto const previousToolbar = hoveredToolBar_;
		hoveredToolBar_ = toolbar;
		hoveredToolId_ = toolId;
		if (previousToolbar && previousToolbar != toolbar) {
			previousToolbar->Refresh(false);
		}
		if (toolbar) {
			toolbar->Refresh(false);
		}
	}
	event.Skip();
}

void CToolBar::OnToolbarMouseLeave(wxMouseEvent& event)
{
	auto const toolbar = dynamic_cast<wxToolBar*>(event.GetEventObject());
	if (toolbar && toolbar == hoveredToolBar_) {
		hoveredToolBar_ = nullptr;
		hoveredToolId_ = wxID_NONE;
		toolbar->Refresh(false);
	}
	event.Skip();
}

WXLRESULT CToolBar::MSWWindowProc(
	WXUINT message, WXWPARAM wParam, WXLPARAM lParam)
{
	if (message == WM_NOTIFY && lParam) {
		auto const header = reinterpret_cast<NMHDR*>(lParam);
		bool const isOurToolbar =
			(localToolBar_ &&
				header->hwndFrom == localToolBar_->GetHandle()) ||
			(remoteToolBar_ &&
				header->hwndFrom == remoteToolBar_->GetHandle());
		if (isOurToolbar && header->code == NM_CUSTOMDRAW) {
			auto const draw = reinterpret_cast<NMTBCUSTOMDRAW*>(lParam);
			if (draw->nmcd.dwDrawStage == CDDS_PREPAINT) {
				return CDRF_NOTIFYITEMDRAW;
			}
			if (draw->nmcd.dwDrawStage == CDDS_ITEMPREPAINT) {
				// O realce é pintado sem substituir os bitmaps geridos pelo Windows.
				RECT const itemRect = draw->nmcd.rc;
				FillToolbarRect(draw->nmcd.hdc, itemRect,
					interface_colour::panel);

				auto const state = draw->nmcd.uItemState;
				bool const disabled = (state & CDIS_DISABLED) != 0;
				bool const pressed = !disabled && (state & CDIS_SELECTED) != 0;
				bool const hovered = hoveredToolBar_ &&
					header->hwndFrom == hoveredToolBar_->GetHandle() &&
					draw->nmcd.dwItemSpec ==
						static_cast<DWORD_PTR>(hoveredToolId_);

				if (pressed || hovered) {
					RECT highlight = itemRect;
					int const inset = FromDIP(2);
					InflateRect(&highlight, -inset, -inset);
					auto const backgroundRole = pressed ?
						interface_colour::accent :
						interface_colour::surface_strong;
					auto const borderRole =
						interface_colour::accent_hover;
					auto const brush = CreateSolidBrush(ToNativeColour(
						GetInterfaceColour(backgroundRole)));
					auto const pen = CreatePen(PS_SOLID, 1, ToNativeColour(
						GetInterfaceColour(borderRole)));
					auto const previousBrush =
						SelectObject(draw->nmcd.hdc, brush);
					auto const previousPen = SelectObject(draw->nmcd.hdc, pen);
					int const radius = FromDIP(6);
					RoundRect(draw->nmcd.hdc, highlight.left, highlight.top,
						highlight.right, highlight.bottom, radius, radius);
					SelectObject(draw->nmcd.hdc, previousPen);
					SelectObject(draw->nmcd.hdc, previousBrush);
					DeleteObject(pen);
					DeleteObject(brush);
				}

				// Botões de alternância ignoram a hot image list no desenho nativo.
				if (DrawToggleToolBitmap(*draw, disabled)) {
					return CDRF_SKIPDEFAULT;
				}

				return TBCDRF_NOBACKGROUND | TBCDRF_NOEDGES |
					TBCDRF_NOOFFSET;
			}
		}
	}
	return wxPanel::MSWWindowProc(message, wParam, lParam);
}
#endif

void CToolBar::EnableTool(int id, bool enable)
{
	auto toolbar = FindToolBar(id);
	if (toolbar) {
		toolbar->EnableTool(id, enable);
	}
}

void CToolBar::OnStateChange(CState* pState, t_statechange_notifications notification, std::wstring const&, const void*)
{
	switch (notification)
	{
	case STATECHANGE_CHANGEDCONTEXT:
	case STATECHANGE_SERVER:
	case STATECHANGE_REMOTE_IDLE:
		UpdateToolbarState();
		break;
	case STATECHANGE_QUEUEPROCESSING:
		{
			const bool check = mainFrame_.GetQueue() && mainFrame_.GetQueue()->IsActive() != 0;
			ToggleTool(XRCID("ID_TOOLBAR_PROCESSQUEUE"), check);
		}
		break;
	case STATECHANGE_SYNC_BROWSE:
		{
			bool is_sync_browse = pState && pState->GetSyncBrowse();
			ToggleTool(XRCID("ID_TOOLBAR_SYNCHRONIZED_BROWSING"), is_sync_browse);
		}
		break;
	case STATECHANGE_COMPARISON:
		{
			bool is_comparing = pState && pState->GetComparisonManager()->IsComparing();
			ToggleTool(XRCID("ID_TOOLBAR_COMPARISON"), is_comparing);
		}
		break;
	case STATECHANGE_APPLYFILTER:
		ToggleTool(XRCID("ID_TOOLBAR_FILTER"), CFilterManager::HasActiveFilters());
		break;
	default:
		break;
	}
}

void CToolBar::UpdateToolbarState()
{
	CState* pState = CContextManager::Get()->GetCurrentContext();
	if (!pState) {
		return;
	}

	bool const hasServer = static_cast<bool>(pState->GetSite());
	bool const idle = pState->IsRemoteIdle();

	EnableTool(XRCID("ID_TOOLBAR_DISCONNECT"), hasServer && idle);
	EnableTool(XRCID("ID_TOOLBAR_CANCEL"), hasServer && !idle);
	EnableTool(XRCID("ID_TOOLBAR_SYNCHRONIZED_BROWSING"), hasServer);

	ToggleTool(XRCID("ID_TOOLBAR_COMPARISON"), pState->GetComparisonManager()->IsComparing());
	ToggleTool(XRCID("ID_TOOLBAR_SYNCHRONIZED_BROWSING"), pState->GetSyncBrowse());

	bool canReconnect;
	if (hasServer || !idle) {
		canReconnect = false;
	}
	else {
		canReconnect = static_cast<bool>(pState->GetLastSite());
	}
	EnableTool(XRCID("ID_TOOLBAR_RECONNECT"), canReconnect);
}

void CToolBar::OnOptionsChanged(watched_options const& options)
{
	if (options.test(OPTION_INTERFACE_APPEARANCE)) {
		// A paleta muda antes do próximo ciclo de pintura do toolbar nativo.
		CallAfter([this] {
			RefreshToolbars();
		});
	}
	if (options.test(OPTION_SHOW_MESSAGELOG)) {
		ToggleTool(XRCID("ID_TOOLBAR_LOGVIEW"), options_.get_int(OPTION_SHOW_MESSAGELOG) != 0);
	}
	if (options.test(OPTION_SHOW_QUEUE)) {
		ToggleTool(XRCID("ID_TOOLBAR_QUEUEVIEW"), options_.get_int(OPTION_SHOW_QUEUE) != 0);
	}
	if (options.test(OPTION_SHOW_TREE_LOCAL)) {
		ToggleTool(XRCID("ID_TOOLBAR_LOCALTREEVIEW"), options_.get_int(OPTION_SHOW_TREE_LOCAL) != 0);
	}
	if (options.test(OPTION_SHOW_TREE_REMOTE)) {
		ToggleTool(XRCID("ID_TOOLBAR_REMOTETREEVIEW"), options_.get_int(OPTION_SHOW_TREE_REMOTE) != 0);
	}
	if (options.test(OPTION_MESSAGELOG_POSITION)) {
		if (options_.get_int(OPTION_MESSAGELOG_POSITION) == 2) {
			HideTool(XRCID("ID_TOOLBAR_LOGVIEW"));
		}
		else {
			ShowTool(XRCID("ID_TOOLBAR_LOGVIEW"));
			ToggleTool(XRCID("ID_TOOLBAR_LOGVIEW"), options_.get_int(OPTION_SHOW_MESSAGELOG) != 0);
		}
	}
}

bool CToolBar::ShowTool(int id)
{
	auto hidden = hiddenTools_.find(id);
	if (hidden == hiddenTools_.end()) {
		return false;
	}

	auto const entry = hidden->second;
	entry.toolbar->InsertTool(entry.position, entry.tool);
	hiddenTools_.erase(hidden);
	entry.toolbar->Realize();
	auto const selected = selectedTools_.find(id);
	if (selected != selectedTools_.end()) {
		entry.toolbar->ToggleTool(id, selected->second);
	}
	return true;
}

bool CToolBar::HideTool(int id)
{
	auto toolbar = FindToolBar(id);
	if (!toolbar) {
		return false;
	}

	int const position = toolbar->GetToolPos(id);
	wxToolBarToolBase* tool = toolbar->RemoveTool(id);
	if (!tool) {
		return false;
	}

	hiddenTools_[id] = { toolbar, position, tool };
	toolbar->Realize();
	return true;
}
