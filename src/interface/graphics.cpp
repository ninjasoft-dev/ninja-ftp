#include "filezilla.h"

#include "graphics.h"

#include <wx/app.h>
#include <wx/bmpbuttn.h>
#include <wx/button.h>
#include <wx/listbox.h>
#include <wx/combobox.h>
#include <wx/listctrl.h>
#include <wx/splitter.h>
#include <wx/statline.h>
#include <wx/statusbr.h>
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

constexpr UINT_PTR modernHeaderSubclassId = 0x4e534844;
constexpr UINT_PTR modernStatusSubclassId = 0x4e535354;
constexpr UINT_PTR modernFrameSubclassId = 0x4e534652;
constexpr UINT_PTR modernMenuSubclassId = 0x4e534d4e;

// O wxWidgets 3.2 não colore a barra de menu no Windows. Essas mensagens
// preservam navegação e acessibilidade nativas, substituindo apenas a pintura.
constexpr UINT wmUahDrawMenu = 0x0091;
constexpr UINT wmUahDrawMenuItem = 0x0092;

struct UahMenu
{
	HMENU menu;
	HDC dc;
	DWORD flags;
};

struct UahMenuItem
{
	int position;
};

struct UahDrawMenuItem
{
	DRAWITEMSTRUCT draw;
	UahMenu menu;
	UahMenuItem item;
};

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

COLORREF ToColorRef(wxColour const& colour)
{
	return RGB(colour.Red(), colour.Green(), colour.Blue());
}

void FillControlBackground(HDC dc, RECT const& rect, interface_colour role)
{
	auto const brush = CreateSolidBrush(ToColorRef(GetInterfaceColour(role)));
	FillRect(dc, &rect, brush);
	DeleteObject(brush);
}

void DrawHeaderSortIndicator(HDC dc, RECT const& rect, bool ascending)
{
	int const centreX = rect.right - 11;
	int const centreY = (rect.top + rect.bottom) / 2;
	POINT points[3]{};
	if (ascending) {
		points[0] = { centreX - 4, centreY + 2 };
		points[1] = { centreX + 4, centreY + 2 };
		points[2] = { centreX, centreY - 2 };
	}
	else {
		points[0] = { centreX - 4, centreY - 2 };
		points[1] = { centreX + 4, centreY - 2 };
		points[2] = { centreX, centreY + 2 };
	}
	auto const brush = CreateSolidBrush(ToColorRef(GetInterfaceColour(interface_colour::accent)));
	auto const oldBrush = SelectObject(dc, brush);
	Polygon(dc, points, 3);
	SelectObject(dc, oldBrush);
	DeleteObject(brush);
}

void PaintModernHeader(HWND handle, HDC suppliedDc = nullptr)
{
	PAINTSTRUCT paint{};
	HDC dc = suppliedDc ? suppliedDc : BeginPaint(handle, &paint);
	if (!dc) {
		return;
	}

	RECT client{};
	GetClientRect(handle, &client);
	FillControlBackground(dc, client, interface_colour::surface_strong);
	SetBkMode(dc, TRANSPARENT);
	SetTextColor(dc, ToColorRef(GetInterfaceColour(interface_colour::muted)));

	auto const font = reinterpret_cast<HFONT>(SendMessageW(handle, WM_GETFONT, 0, 0));
	auto const oldFont = font ? SelectObject(dc, font) : nullptr;
	auto const borderPen = CreatePen(PS_SOLID, 1, ToColorRef(GetInterfaceColour(interface_colour::border)));
	auto const oldPen = SelectObject(dc, borderPen);
	int const count = Header_GetItemCount(handle);
	for (int index = 0; index < count; ++index) {
		RECT itemRect{};
		if (!Header_GetItemRect(handle, index, &itemRect)) {
			continue;
		}

		wchar_t label[512]{};
		HDITEMW item{};
		item.mask = HDI_TEXT | HDI_FORMAT;
		item.pszText = label;
		item.cchTextMax = static_cast<int>(std::size(label));
		Header_GetItem(handle, index, &item);

		RECT textRect = itemRect;
		textRect.left += 8;
		textRect.right -= 8;
		bool const sorted = (item.fmt & (HDF_SORTUP | HDF_SORTDOWN)) != 0;
		if (sorted) {
			textRect.right -= 13;
		}

		UINT format = DT_SINGLELINE | DT_VCENTER | DT_END_ELLIPSIS | DT_NOPREFIX;
		switch (item.fmt & HDF_JUSTIFYMASK) {
		case HDF_CENTER:
			format |= DT_CENTER;
			break;
		case HDF_RIGHT:
			format |= DT_RIGHT;
			break;
		default:
			format |= DT_LEFT;
			break;
		}
		DrawTextW(dc, label, -1, &textRect, format);

		MoveToEx(dc, itemRect.right - 1, itemRect.top + 4, nullptr);
		LineTo(dc, itemRect.right - 1, itemRect.bottom - 4);

		if (sorted) {
			DrawHeaderSortIndicator(dc, itemRect, (item.fmt & HDF_SORTUP) != 0);
		}
	}

	MoveToEx(dc, client.left, client.bottom - 1, nullptr);
	LineTo(dc, client.right, client.bottom - 1);
	SelectObject(dc, oldPen);
	DeleteObject(borderPen);
	if (oldFont) {
		SelectObject(dc, oldFont);
	}
	if (!suppliedDc) {
		EndPaint(handle, &paint);
	}
}

LRESULT CALLBACK ModernHeaderProc(HWND handle, UINT message, WPARAM wParam, LPARAM lParam,
	UINT_PTR subclassId, DWORD_PTR)
{
	if (message == WM_PAINT) {
		PaintModernHeader(handle);
		return 0;
	}
	if (message == WM_PRINTCLIENT) {
		PaintModernHeader(handle, reinterpret_cast<HDC>(wParam));
		return 0;
	}
	if (message == WM_THEMECHANGED || message == WM_SETTINGCHANGE) {
		InvalidateRect(handle, nullptr, TRUE);
	}
	else if (message == WM_NCDESTROY) {
		RemoveWindowSubclass(handle, ModernHeaderProc, subclassId);
	}
	return DefSubclassProc(handle, message, wParam, lParam);
}

void PaintModernStatusBar(HWND handle, HDC suppliedDc = nullptr)
{
	PAINTSTRUCT paint{};
	HDC dc = suppliedDc ? suppliedDc : BeginPaint(handle, &paint);
	if (!dc) {
		return;
	}

	RECT client{};
	GetClientRect(handle, &client);
	FillControlBackground(dc, client, interface_colour::panel);
	SetBkMode(dc, TRANSPARENT);
	SetTextColor(dc, ToColorRef(GetInterfaceColour(interface_colour::muted)));

	int const length = LOWORD(SendMessageW(handle, SB_GETTEXTLENGTHW, 0, 0));
	std::wstring text(static_cast<size_t>(length) + 1, L'\0');
	SendMessageW(handle, SB_GETTEXTW, 0, reinterpret_cast<LPARAM>(text.data()));
	RECT textRect = client;
	textRect.left += 8;
	textRect.right -= 8;
	auto const font = reinterpret_cast<HFONT>(SendMessageW(handle, WM_GETFONT, 0, 0));
	auto const oldFont = font ? SelectObject(dc, font) : nullptr;
	DrawTextW(dc, text.c_str(), length, &textRect,
		DT_SINGLELINE | DT_VCENTER | DT_END_ELLIPSIS | DT_NOPREFIX);

	auto const borderPen = CreatePen(PS_SOLID, 1, ToColorRef(GetInterfaceColour(interface_colour::border)));
	auto const oldPen = SelectObject(dc, borderPen);
	MoveToEx(dc, client.left, client.top, nullptr);
	LineTo(dc, client.right, client.top);
	SelectObject(dc, oldPen);
	DeleteObject(borderPen);
	if (oldFont) {
		SelectObject(dc, oldFont);
	}
	if (!suppliedDc) {
		EndPaint(handle, &paint);
	}
}

LRESULT CALLBACK ModernStatusProc(HWND handle, UINT message, WPARAM wParam, LPARAM lParam,
	UINT_PTR subclassId, DWORD_PTR)
{
	if (message == WM_PAINT && SendMessageW(handle, SB_GETPARTS, 0, 0) == 1) {
		PaintModernStatusBar(handle);
		return 0;
	}
	if (message == WM_PRINTCLIENT && SendMessageW(handle, SB_GETPARTS, 0, 0) == 1) {
		PaintModernStatusBar(handle, reinterpret_cast<HDC>(wParam));
		return 0;
	}
	if (message == WM_THEMECHANGED || message == WM_SETTINGCHANGE) {
		InvalidateRect(handle, nullptr, TRUE);
	}
	else if (message == WM_NCDESTROY) {
		RemoveWindowSubclass(handle, ModernStatusProc, subclassId);
	}
	return DefSubclassProc(handle, message, wParam, lParam);
}

void PaintModernFrame(HWND handle)
{
	auto const extendedStyle = GetWindowLongPtrW(handle, GWL_EXSTYLE);
	auto const style = GetWindowLongPtrW(handle, GWL_STYLE);
	if (!(extendedStyle & (WS_EX_CLIENTEDGE | WS_EX_STATICEDGE)) && !(style & WS_BORDER)) {
		return;
	}

	RECT windowRect{};
	if (!GetWindowRect(handle, &windowRect)) {
		return;
	}

	int const width = windowRect.right - windowRect.left;
	int const height = windowRect.bottom - windowRect.top;
	int const edgeX = extendedStyle & WS_EX_CLIENTEDGE ? GetSystemMetrics(SM_CXEDGE) : 1;
	int const edgeY = extendedStyle & WS_EX_CLIENTEDGE ? GetSystemMetrics(SM_CYEDGE) : 1;
	auto const dc = GetWindowDC(handle);
	if (!dc) {
		return;
	}

	auto const brush = CreateSolidBrush(ToColorRef(GetInterfaceColour(interface_colour::border)));
	RECT top{0, 0, width, edgeY};
	RECT bottom{0, height - edgeY, width, height};
	RECT left{0, edgeY, edgeX, height - edgeY};
	RECT right{width - edgeX, edgeY, width, height - edgeY};
	FillRect(dc, &top, brush);
	FillRect(dc, &bottom, brush);
	FillRect(dc, &left, brush);
	FillRect(dc, &right, brush);
	DeleteObject(brush);
	ReleaseDC(handle, dc);
}

LRESULT CALLBACK ModernFrameProc(HWND handle, UINT message, WPARAM wParam, LPARAM lParam,
	UINT_PTR subclassId, DWORD_PTR)
{
	if (message == WM_NCDESTROY) {
		RemoveWindowSubclass(handle, ModernFrameProc, subclassId);
		return DefSubclassProc(handle, message, wParam, lParam);
	}

	auto const result = DefSubclassProc(handle, message, wParam, lParam);
	if (message == WM_NCPAINT || message == WM_NCACTIVATE) {
		PaintModernFrame(handle);
	}
	else if (message == WM_THEMECHANGED || message == WM_SETTINGCHANGE) {
		RedrawWindow(handle, nullptr, nullptr, RDW_INVALIDATE | RDW_FRAME);
	}
	return result;
}

void PaintMenuBottomLine(HWND handle)
{
	RECT client{};
	RECT window{};
	if (!GetClientRect(handle, &client) || !GetWindowRect(handle, &window)) {
		return;
	}

	MapWindowPoints(handle, nullptr, reinterpret_cast<POINT*>(&client), 2);
	OffsetRect(&client, -window.left, -window.top);
	RECT line{client.left, client.top - 1, client.right, client.top};
	auto const dc = GetWindowDC(handle);
	if (!dc) {
		return;
	}
	auto const brush = CreateSolidBrush(ToColorRef(GetInterfaceColour(interface_colour::border)));
	FillRect(dc, &line, brush);
	DeleteObject(brush);
	ReleaseDC(handle, dc);
}

LRESULT CALLBACK ModernMenuProc(HWND handle, UINT message, WPARAM wParam, LPARAM lParam,
	UINT_PTR subclassId, DWORD_PTR)
{
	if (message == WM_NCDESTROY) {
		RemoveWindowSubclass(handle, ModernMenuProc, subclassId);
		return DefSubclassProc(handle, message, wParam, lParam);
	}

	if (IsDarkInterface() && message == wmUahDrawMenu) {
		auto const menu = reinterpret_cast<UahMenu const*>(lParam);
		MENUBARINFO info{};
		info.cbSize = sizeof(info);
		RECT window{};
		if (menu && menu->dc && GetMenuBarInfo(handle, OBJID_MENU, 0, &info) &&
			GetWindowRect(handle, &window))
		{
			RECT rect = info.rcBar;
			OffsetRect(&rect, -window.left, -window.top);
			auto const brush = CreateSolidBrush(
				ToColorRef(GetInterfaceColour(interface_colour::panel)));
			FillRect(menu->dc, &rect, brush);
			DeleteObject(brush);
			return 1;
		}
	}
	else if (IsDarkInterface() && message == wmUahDrawMenuItem) {
		auto const item = reinterpret_cast<UahDrawMenuItem const*>(lParam);
		if (item && item->menu.dc) {
			wchar_t label[256]{};
			GetMenuStringW(item->menu.menu, item->item.position, label,
				static_cast<int>(std::size(label)), MF_BYPOSITION);

			bool const highlighted = (item->draw.itemState &
				(ODS_HOTLIGHT | ODS_SELECTED)) != 0;
			auto const background = GetInterfaceColour(highlighted ?
				interface_colour::surface_strong : interface_colour::panel);
			auto const text = GetInterfaceColour(
				item->draw.itemState & (ODS_DISABLED | ODS_GRAYED) ?
					interface_colour::muted : interface_colour::text);
			auto const brush = CreateSolidBrush(ToColorRef(background));
			FillRect(item->menu.dc, &item->draw.rcItem, brush);
			DeleteObject(brush);

			auto const oldMode = SetBkMode(item->menu.dc, TRANSPARENT);
			auto const oldColour = SetTextColor(item->menu.dc, ToColorRef(text));
			UINT format = DT_CENTER | DT_SINGLELINE | DT_VCENTER;
			if (item->draw.itemState & ODS_NOACCEL) {
				format |= DT_HIDEPREFIX;
			}
			RECT textRect = item->draw.rcItem;
			DrawTextW(item->menu.dc, label, -1, &textRect, format);
			SetTextColor(item->menu.dc, oldColour);
			SetBkMode(item->menu.dc, oldMode);
			return 1;
		}
	}

	auto const result = DefSubclassProc(handle, message, wParam, lParam);
	if (IsDarkInterface() && (message == WM_NCPAINT || message == WM_NCACTIVATE)) {
		PaintMenuBottomLine(handle);
	}
	return result;
}

void ApplyNativeWindowAppearance(wxWindow& window, bool dark)
{
	auto const handle = reinterpret_cast<HWND>(window.GetHandle());
	if (!handle) {
		return;
	}

	auto const themeLibrary = GetModuleHandleW(L"uxtheme.dll");
	auto const allowDarkMode = GetFunction<AllowDarkModeForWindowFunction>(themeLibrary, 133);
	if (allowDarkMode) {
		allowDarkMode(handle, dark ? TRUE : FALSE);
	}

	wchar_t className[64]{};
	GetClassNameW(handle, className, sizeof(className) / sizeof(*className));
	bool const isTree = wcscmp(className, WC_TREEVIEWW) == 0;
	bool const isList = wcscmp(className, WC_LISTVIEWW) == 0;
	bool const isHeader = wcscmp(className, WC_HEADERW) == 0;
	bool const isStatusBar = wcscmp(className, STATUSCLASSNAMEW) == 0;
	auto const nativeStyle = GetWindowLongPtrW(handle, GWL_STYLE);
	auto const nativeExtendedStyle = GetWindowLongPtrW(handle, GWL_EXSTYLE);
	bool const needsModernFrame = !window.IsTopLevel() && !isHeader && !isStatusBar &&
		((nativeStyle & WS_BORDER) ||
			(nativeExtendedStyle & (WS_EX_CLIENTEDGE | WS_EX_STATICEDGE)));
	auto const setWindowTheme = GetFunction<SetWindowThemeFunction>(themeLibrary, "SetWindowTheme");
	if (setWindowTheme) {
		if (isTree || isList || wcscmp(className, L"ScrollBar") == 0) {
			setWindowTheme(handle, dark ? L"DarkMode_Explorer" : L"Explorer", nullptr);
		}
		else if (isHeader) {
			setWindowTheme(handle, dark ? L"DarkMode_ItemsView" : L"Explorer", nullptr);
		}
	}
	if (isHeader) {
		SetWindowSubclass(handle, ModernHeaderProc, modernHeaderSubclassId, 0);
		InvalidateRect(handle, nullptr, TRUE);
	}
	else if (isStatusBar) {
		SetWindowSubclass(handle, ModernStatusProc, modernStatusSubclassId, 0);
		InvalidateRect(handle, nullptr, TRUE);
	}
	if (needsModernFrame) {
		SetWindowSubclass(handle, ModernFrameProc, modernFrameSubclassId, 0);
		RedrawWindow(handle, nullptr, nullptr, RDW_INVALIDATE | RDW_FRAME);
	}

	auto const inputColour = GetInterfaceColour(interface_colour::input);
	auto const textColour = GetInterfaceColour(interface_colour::text);
	COLORREF const background = RGB(inputColour.Red(), inputColour.Green(), inputColour.Blue());
	COLORREF const text = RGB(textColour.Red(), textColour.Green(), textColour.Blue());
	if (isTree) {
		TreeView_SetBkColor(handle, background);
		TreeView_SetTextColor(handle, text);
	}
	else if (isList) {
		ListView_SetBkColor(handle, background);
		ListView_SetTextBkColor(handle, background);
		ListView_SetTextColor(handle, text);

		// O cabeçalho é uma janela Win32 interna, sem um wxWindow próprio e sem wxEVT_CREATE.
		auto const header = ListView_GetHeader(handle);
		if (header) {
			if (allowDarkMode) {
				allowDarkMode(header, dark ? TRUE : FALSE);
			}
			if (setWindowTheme) {
				setWindowTheme(header, dark ? L"DarkMode_ItemsView" : L"Explorer", nullptr);
			}
			SetWindowSubclass(header, ModernHeaderProc, modernHeaderSubclassId, 0);
			InvalidateRect(header, nullptr, TRUE);
		}
	}

	if (window.IsTopLevel()) {
		SetWindowSubclass(handle, ModernMenuProc, modernMenuSubclassId, 0);
		DrawMenuBar(handle);
		auto const dwmLibrary = LoadLibraryW(L"dwmapi.dll");
		if (auto const setAttribute = GetFunction<DwmSetWindowAttributeFunction>(dwmLibrary, "DwmSetWindowAttribute")) {
			BOOL const enabled = dark ? TRUE : FALSE;
			// O atributo 20 é o nome atual; o 19 mantém compatibilidade com versões anteriores do Windows 10.
			if (FAILED(setAttribute(handle, 20, &enabled, sizeof(enabled)))) {
				setAttribute(handle, 19, &enabled, sizeof(enabled));
			}

			auto const captionColour = GetInterfaceColour(interface_colour::panel);
			auto const borderColour = GetInterfaceColour(interface_colour::border);
			auto const titleColour = GetInterfaceColour(interface_colour::text);
			COLORREF const caption = RGB(captionColour.Red(), captionColour.Green(), captionColour.Blue());
			COLORREF const border = RGB(borderColour.Red(), borderColour.Green(), borderColour.Blue());
			COLORREF const title = RGB(titleColour.Red(), titleColour.Green(), titleColour.Blue());
			setAttribute(handle, 35, &caption, sizeof(caption));
			setAttribute(handle, 34, &border, sizeof(border));
			setAttribute(handle, 36, &title, sizeof(title));
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
		case interface_colour::background:
			return wxColour(244, 244, 246);
		case interface_colour::panel:
		case interface_colour::input:
			return wxColour(255, 255, 255);
		case interface_colour::surface_strong:
			return wxColour(236, 234, 248);
		case interface_colour::border:
			return wxColour(218, 216, 232);
		case interface_colour::text:
			return wxColour(23, 22, 42);
		case interface_colour::muted:
			return wxColour(102, 101, 122);
		case interface_colour::accent:
			return wxColour(105, 65, 198);
		case interface_colour::accent_hover:
			return wxColour(123, 85, 209);
		case interface_colour::accent_text:
		default:
			return wxColour(255, 255, 255);
		}
	}

	switch (role) {
	case interface_colour::background:
		return wxColour(11, 13, 32);
	case interface_colour::input:
		return wxColour(6, 7, 16);
	case interface_colour::surface_strong:
		return wxColour(26, 31, 66);
	case interface_colour::border:
		return wxColour(39, 45, 89);
	case interface_colour::text:
		return wxColour(245, 244, 255);
	case interface_colour::muted:
		return wxColour(185, 187, 209);
	case interface_colour::accent:
		return wxColour(157, 114, 239);
	case interface_colour::accent_hover:
		return wxColour(185, 147, 255);
	case interface_colour::accent_text:
		return wxColour(11, 13, 32);
	case interface_colour::panel:
	default:
		return wxColour(18, 22, 49);
	}
}

void CInterfaceAppearance::Apply(wxWindow& window)
{
	bool const acceptsText = dynamic_cast<wxTextCtrl*>(&window) ||
		dynamic_cast<wxComboBox*>(&window) || dynamic_cast<wxChoice*>(&window);
	bool const displaysItems = dynamic_cast<wxListCtrl*>(&window) ||
		dynamic_cast<wxTreeCtrl*>(&window) || dynamic_cast<wxListBox*>(&window) ||
		dynamic_cast<wxCheckListBox*>(&window);
	bool const isButton = dynamic_cast<wxButton*>(&window) ||
		dynamic_cast<wxBitmapButton*>(&window);
	bool const isStatusBar = dynamic_cast<wxStatusBar*>(&window);
	bool const isDivider = dynamic_cast<wxStaticLine*>(&window) ||
		dynamic_cast<wxSplitterWindow*>(&window);

	interface_colour backgroundRole = interface_colour::panel;
	interface_colour foregroundRole = interface_colour::text;
	if (window.IsTopLevel()) {
		backgroundRole = interface_colour::background;
	}
	else if (acceptsText || displaysItems) {
		backgroundRole = interface_colour::input;
	}
	else if (isButton || isStatusBar) {
		backgroundRole = interface_colour::surface_strong;
	}
	else if (isDivider) {
		backgroundRole = interface_colour::border;
	}

	if (window.GetName() == L"ninja-muted-label") {
		foregroundRole = interface_colour::muted;
	}

	window.SetBackgroundColour(GetColour(backgroundRole));
	window.SetForegroundColour(GetColour(foregroundRole));
#ifdef __WXMSW__
	// Os controles comuns podem substituir a paleta do wxWidgets ao ativar o tema nativo.
	ApplyNativeWindowAppearance(window, dark_);
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

void CInterfaceAppearance::SetAppearance(interface_appearance appearance)
{
	configured_ = appearance;
	dark_ = configured_ == interface_appearance::dark ||
		(configured_ == interface_appearance::automatic && wxSystemSettings::GetAppearance().IsDark());
	ConfigureNativeAppearance();
	for (auto const window : wxTopLevelWindows) {
		if (window) {
			ApplyRecursively(*window);
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
	return wxSystemSettings::GetColour(
		role == interface_colour::text || role == interface_colour::muted ?
		wxSYS_COLOUR_WINDOWTEXT : wxSYS_COLOUR_WINDOW);
}

void ApplyInterfaceAppearance(wxWindow& window)
{
	if (active_appearance) {
		active_appearance->Apply(window);
	}
}

void SetInterfaceAppearance(interface_appearance appearance)
{
	if (active_appearance) {
		active_appearance->SetAppearance(appearance);
	}
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
