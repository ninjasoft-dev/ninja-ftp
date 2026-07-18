#include "filezilla.h"

#include "graphics.h"

#include <wx/app.h>
#include <wx/listbox.h>
#include <wx/combobox.h>
#include <wx/listctrl.h>
#include <wx/textctrl.h>
#include <wx/treectrl.h>
#include <wx/weakref.h>

#ifdef __WXMSW__
	#include <commctrl.h>
	#include <windows.h>
#endif

namespace {
wxColour const background_colors[] = {
	wxColour(),
	wxColour(255, 0, 0, 32),
	wxColour(0, 255, 0, 32),
	wxColour(0, 0, 255, 32),
	wxColour(255, 255, 0, 32),
	wxColour(0, 255, 255, 32),
	wxColour(255, 0, 255, 32),
	wxColour(255, 128, 0, 32) };

CInterfaceAppearance* active_appearance{};

#ifdef __WXMSW__
using SetPreferredAppModeFunction = int (WINAPI*)(int);
using AllowDarkModeForWindowFunction = BOOL (WINAPI*)(HWND, BOOL);
using FlushMenuThemesFunction = void (WINAPI*)();
using SetWindowThemeFunction = HRESULT (WINAPI*)(HWND, LPCWSTR, LPCWSTR);
using DwmSetWindowAttributeFunction = HRESULT (WINAPI*)(HWND, DWORD, LPCVOID, DWORD);

template<typename Function>
Function GetFunction(HMODULE module, char const* name)
{
	return module ? reinterpret_cast<Function>(GetProcAddress(module, name)) : nullptr;
}

template<typename Function>
Function GetFunction(HMODULE module, WORD ordinal)
{
	return module ? reinterpret_cast<Function>(GetProcAddress(module, MAKEINTRESOURCEA(ordinal))) : nullptr;
}

void ApplyNativeWindowAppearance(wxWindow& window, bool dark)
{
	auto const handle = reinterpret_cast<HWND>(window.GetHandle());
	if (!handle) {
		return;
	}

	auto const themeLibrary = GetModuleHandleW(L"uxtheme.dll");
	if (auto const allowDarkMode = GetFunction<AllowDarkModeForWindowFunction>(themeLibrary, 133)) {
		allowDarkMode(handle, dark ? TRUE : FALSE);
	}

	wchar_t className[64]{};
	GetClassNameW(handle, className, sizeof(className) / sizeof(*className));
	bool const isTree = wcscmp(className, WC_TREEVIEWW) == 0;
	bool const isList = wcscmp(className, WC_LISTVIEWW) == 0;
	bool const isHeader = wcscmp(className, WC_HEADERW) == 0;
	if (auto const setWindowTheme = GetFunction<SetWindowThemeFunction>(themeLibrary, "SetWindowTheme")) {
		if (isTree || isList || wcscmp(className, L"ScrollBar") == 0) {
			setWindowTheme(handle, dark ? L"DarkMode_Explorer" : L"Explorer", nullptr);
		}
		else if (isHeader) {
			setWindowTheme(handle, dark ? L"DarkMode_ItemsView" : L"Explorer", nullptr);
		}
	}

	COLORREF const background = dark ? RGB(27, 29, 33) : GetSysColor(COLOR_WINDOW);
	COLORREF const text = dark ? RGB(232, 234, 237) : GetSysColor(COLOR_WINDOWTEXT);
	if (isTree) {
		TreeView_SetBkColor(handle, background);
		TreeView_SetTextColor(handle, text);
	}
	else if (isList) {
		ListView_SetBkColor(handle, background);
		ListView_SetTextBkColor(handle, background);
		ListView_SetTextColor(handle, text);
	}

	if (window.IsTopLevel()) {
		auto const dwmLibrary = LoadLibraryW(L"dwmapi.dll");
		if (auto const setAttribute = GetFunction<DwmSetWindowAttributeFunction>(dwmLibrary, "DwmSetWindowAttribute")) {
			BOOL const enabled = dark ? TRUE : FALSE;
			// O atributo 20 é o nome atual; o 19 mantém compatibilidade com versões anteriores do Windows 10.
			if (FAILED(setAttribute(handle, 20, &enabled, sizeof(enabled)))) {
				setAttribute(handle, 19, &enabled, sizeof(enabled));
			}
		}
		if (dwmLibrary) {
			FreeLibrary(dwmLibrary);
		}
	}
}
#endif
}

CInterfaceAppearance::CInterfaceAppearance(interface_appearance appearance)
	: configured_(appearance)
{
	dark_ = configured_ == interface_appearance::dark ||
		(configured_ == interface_appearance::automatic && wxSystemSettings::GetAppearance().IsDark());
	active_appearance = this;
	ConfigureNativeAppearance();
	wxEvtHandler::AddFilter(this);
}

CInterfaceAppearance::~CInterfaceAppearance()
{
	wxEvtHandler::RemoveFilter(this);
	if (active_appearance == this) {
		active_appearance = nullptr;
	}
}

int CInterfaceAppearance::FilterEvent(wxEvent& event)
{
	if (event.GetEventType() == wxEVT_CREATE) {
		auto& createEvent = static_cast<wxWindowCreateEvent&>(event);
		if (auto const window = createEvent.GetWindow()) {
			Apply(*window);
			wxWeakRef<wxWindow> const weakWindow(window);
			wxTheApp->CallAfter([this, weakWindow] {
				if (weakWindow) {
					Apply(*weakWindow);
				}
			});
		}
	}
	else if (configured_ == interface_appearance::automatic &&
		event.GetEventType() == wxEVT_SYS_COLOUR_CHANGED)
	{
		QueueSystemAppearanceUpdate();
	}

	return Event_Skip;
}

wxColour CInterfaceAppearance::GetColour(interface_colour role) const
{
	if (!dark_) {
		switch (role) {
		case interface_colour::input:
			return wxSystemSettings::GetColour(wxSYS_COLOUR_WINDOW);
		case interface_colour::text:
			return wxSystemSettings::GetColour(wxSYS_COLOUR_WINDOWTEXT);
		case interface_colour::panel:
		default:
			return wxSystemSettings::GetColour(wxSYS_COLOUR_BTNFACE);
		}
	}

	switch (role) {
	case interface_colour::input:
		return wxColour(27, 29, 33);
	case interface_colour::text:
		return wxColour(232, 234, 237);
	case interface_colour::panel:
	default:
		return wxColour(36, 38, 42);
	}
}

void CInterfaceAppearance::Apply(wxWindow& window)
{
	if (!dark_) {
		window.SetBackgroundColour(wxColour());
		window.SetForegroundColour(wxColour());
#ifdef __WXMSW__
		ApplyNativeWindowAppearance(window, false);
#endif
		window.Refresh();
		return;
	}

	bool const acceptsText = dynamic_cast<wxTextCtrl*>(&window) ||
		dynamic_cast<wxComboBox*>(&window) || dynamic_cast<wxChoice*>(&window);
	bool const displaysItems = dynamic_cast<wxListCtrl*>(&window) ||
		dynamic_cast<wxTreeCtrl*>(&window) || dynamic_cast<wxListBox*>(&window) ||
		dynamic_cast<wxCheckListBox*>(&window);

	window.SetBackgroundColour(GetColour(
		acceptsText || displaysItems ? interface_colour::input : interface_colour::panel));
	window.SetForegroundColour(GetColour(interface_colour::text));
#ifdef __WXMSW__
	// Os controles comuns podem substituir a paleta do wxWidgets ao ativar o tema nativo.
	ApplyNativeWindowAppearance(window, true);
#endif
	window.Refresh();
}

void CInterfaceAppearance::ApplyRecursively(wxWindow& window)
{
	Apply(window);
	for (auto const child : window.GetChildren()) {
		if (child) {
			ApplyRecursively(*child);
		}
	}
}

void CInterfaceAppearance::ConfigureNativeAppearance()
{
#ifdef __WXMSW__
	auto const themeLibrary = LoadLibraryW(L"uxtheme.dll");
	if (auto const setPreferredMode = GetFunction<SetPreferredAppModeFunction>(themeLibrary, 135)) {
		int const mode = configured_ == interface_appearance::light ? 3 :
			(configured_ == interface_appearance::dark ? 2 : 1);
		setPreferredMode(mode);
	}
	if (auto const flushMenuThemes = GetFunction<FlushMenuThemesFunction>(themeLibrary, 136)) {
		flushMenuThemes();
	}
	if (themeLibrary) {
		FreeLibrary(themeLibrary);
	}
#endif
}

void CInterfaceAppearance::QueueSystemAppearanceUpdate()
{
	if (update_pending_) {
		return;
	}
	update_pending_ = true;
	wxTheApp->CallAfter([this] {
		update_pending_ = false;
		bool const useDark = wxSystemSettings::GetAppearance().IsDark();
		if (useDark == dark_) {
			return;
		}
		dark_ = useDark;
		ConfigureNativeAppearance();
		for (auto const window : wxTopLevelWindows) {
			if (window) {
				ApplyRecursively(*window);
			}
		}
	});
}

bool IsDarkInterface()
{
	return active_appearance && active_appearance->IsDark();
}

wxColour GetInterfaceColour(interface_colour role)
{
	if (active_appearance) {
		return active_appearance->GetColour(role);
	}
	return wxSystemSettings::GetColour(role == interface_colour::text ?
		wxSYS_COLOUR_WINDOWTEXT : wxSYS_COLOUR_WINDOW);
}

wxColor site_colour_to_wx(site_colour c)
{
	auto index = static_cast<size_t>(c);
	if (index < sizeof(background_colors) / sizeof(*background_colors)){
		return background_colors[index];
	}
	return background_colors[0];
}

CWindowTinter::CWindowTinter(wxWindow& wnd)
	: m_wnd(wnd)
{
	m_wnd.Bind(wxEVT_SYS_COLOUR_CHANGED, &CWindowTinter::OnColorChange, this);
}

CWindowTinter::~CWindowTinter()
{
	m_wnd.Unbind(wxEVT_SYS_COLOUR_CHANGED, &CWindowTinter::OnColorChange, this);
}

void CWindowTinter::OnColorChange(wxSysColourChangedEvent &)
{
	SetBackgroundTint(site_colour_to_wx(tint_));
}

void CWindowTinter::SetBackgroundTint(site_colour tint)
{
	tint_ = tint;
	SetBackgroundTint(site_colour_to_wx(tint));
}

wxColour CWindowTinter::GetOriginalColor()
{
	if (IsDarkInterface()) {
		return GetInterfaceColour(interface_colour::input);
	}
#ifndef __WXMSW__
	auto listctrl = dynamic_cast<wxListCtrl*>(m_wnd.GetParent());
	if (listctrl && reinterpret_cast<wxWindow*>(listctrl->m_mainWin) == &m_wnd) {
		return listctrl->GetDefaultAttributes().colBg;
	}
#endif

#ifdef __WXMAC__
	auto combo = dynamic_cast<wxComboBox*>(&m_wnd);
	if (combo) {
		wxColour c = wxTextCtrl::GetClassDefaultAttributes().colBg;

		// In old versions of macOS, the wrong color may be reported. Try to detect it and just default to white.
		if ((!c.IsOk() || c == *wxBLACK) && !wxSystemSettingsNative::GetAppearance().IsDark()) {
			return wxColour(255, 255, 255);
		}

		return c;
	}
#endif
	return m_wnd.GetDefaultAttributes().colBg;
}

void CWindowTinter::SetBackgroundTint(wxColour const& tint)
{
	if (!tint.IsOk() && dynamic_cast<wxComboBox*>(&m_wnd)) {
		m_wnd.SetBackgroundColour(wxColour());
		m_wnd.Refresh();
		return;
	}

	wxColour originalColor = GetOriginalColor();

	wxColour const newColour = tint.IsOk() ? AlphaComposite_Over(originalColor, tint) : originalColor;
	if (newColour != m_wnd.GetBackgroundColour()) {
		if (m_wnd.SetBackgroundColour(newColour)) {
			m_wnd.Refresh();
		}
	}
}

void Overlay(wxBitmap& bg, wxBitmap const& fg)
{
	if (!bg.IsOk() || !fg.IsOk()) {
		return;
	}

	wxImage foreground = fg.ConvertToImage();
	if (!foreground.HasAlpha()) {
		foreground.InitAlpha();
	}

	wxImage background = bg.ConvertToImage();
	if (!background.HasAlpha()) {
		background.InitAlpha();
	}

	if (foreground.GetSize() != background.GetSize()) {
		foreground.Rescale(background.GetSize().x, background.GetSize().y, wxIMAGE_QUALITY_HIGH);
	}

	unsigned char* bg_data = background.GetData();
	unsigned char* bg_alpha = background.GetAlpha();
	unsigned char* fg_data = foreground.GetData();
	unsigned char* fg_alpha = foreground.GetAlpha();
	unsigned char* bg_end = bg_data + background.GetWidth() * background.GetHeight() * 3;
	while (bg_data != bg_end) {
		AlphaComposite_Over_Inplace(
			*bg_data, *(bg_data + 1), *(bg_data + 2), *bg_alpha,
			*fg_data, *(fg_data + 1), *(fg_data + 2), *fg_alpha);
		bg_data += 3;
		fg_data += 3;
		++bg_alpha;
		++fg_alpha;
	}

#ifdef __WXMAC__
	bg = wxBitmap(background, -1, bg.GetScaleFactor());
#else
	bg = wxBitmap(background, -1);
#endif
}
