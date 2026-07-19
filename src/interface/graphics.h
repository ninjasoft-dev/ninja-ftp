#ifndef FILEZILLA_INTERFACE_GRAPHICS_HEADER
#define FILEZILLA_INTERFACE_GRAPHICS_HEADER

#include <wx/rawbmp.h>
#include <wx/window.h>
#include <wx/eventfilter.h>

#include "../commonui/site_color.h"

static inline unsigned char AlphaComposite_Over_GetAlpha(unsigned char bg_alpha, unsigned char fg_alpha)
{
	return bg_alpha + fg_alpha - bg_alpha * fg_alpha / 255;
}

// Do not call with zero new_alpha
static inline unsigned char AlphaComposite_Over(unsigned char bg, unsigned char bg_alpha, unsigned char fg, unsigned char fg_alpha, unsigned char new_alpha)
{
	return (bg * (255 - fg_alpha) * bg_alpha / 255 + fg * fg_alpha) / new_alpha;
}

// Alpha compositing of a single pixel, b gets composited over a
// (well-known over operator), result stored in a.
// All RGB and A values have range from 0 to 255, RGB values aren't
// premultiplied by A.
// Safe for multiple compositions.
static inline void AlphaComposite_Over_Inplace(
	unsigned char & bg_red, unsigned char & bg_green, unsigned char & bg_blue, unsigned char & bg_alpha,
	unsigned char & fg_red, unsigned char & fg_green, unsigned char & fg_blue, unsigned char & fg_alpha)
{
	if (!fg_alpha) {
		// Nothing to do. Also prevents zero new_alpha
		return;
	}

	unsigned char const new_alpha = AlphaComposite_Over_GetAlpha(bg_alpha, fg_alpha);
	bg_red   = AlphaComposite_Over(bg_red,   bg_alpha, fg_red,   fg_alpha, new_alpha);
	bg_green = AlphaComposite_Over(bg_green, bg_alpha, fg_green, fg_alpha, new_alpha);
	bg_blue  = AlphaComposite_Over(bg_blue,  bg_alpha, fg_blue,  fg_alpha, new_alpha);
	bg_alpha = new_alpha;
}

static inline void AlphaComposite_Over_Inplace(wxAlphaPixelData::Iterator &bg, wxAlphaPixelData::Iterator &fg)
{
	AlphaComposite_Over_Inplace(bg.Red(), bg.Green(), bg.Blue(), bg.Alpha(), fg.Red(), fg.Green(), fg.Blue(), fg.Alpha());
}

static inline wxColour AlphaComposite_Over(wxColour const& bg, wxColour const& fg) {
	if (!fg.IsOk() || !fg.Alpha()) {
		// Nothing to do. Also prevents zero new_alpha
		return bg;
	}

	unsigned char const new_alpha = AlphaComposite_Over_GetAlpha(bg.Alpha(), fg.Alpha());
	return wxColour(
		AlphaComposite_Over(bg.Red(),   bg.Alpha(), fg.Red(),   fg.Alpha(), new_alpha),
		AlphaComposite_Over(bg.Green(), bg.Alpha(), fg.Green(), fg.Alpha(), new_alpha),
		AlphaComposite_Over(bg.Blue(),  bg.Alpha(), fg.Blue(),  fg.Alpha(), new_alpha),
		new_alpha
	);
}

void Overlay(wxBitmap& bg, wxBitmap const& fg);

wxColour site_colour_to_wx(site_colour);

enum class interface_appearance
{
	automatic,
	light,
	dark
};

enum class interface_colour
{
	background,
	panel,
	input,
	surface_strong,
	border,
	text,
	muted,
	accent,
	accent_hover,
	accent_text
};

// Mantém a paleta do wxWidgets e dos controles nativos coerente em todas as janelas.
class CInterfaceAppearance final : public wxEventFilter
{
public:
	explicit CInterfaceAppearance(interface_appearance appearance);
	~CInterfaceAppearance() override;

	int FilterEvent(wxEvent& event) override;

	bool IsDark() const { return dark_; }
	wxColour GetColour(interface_colour role) const;
	void SetAppearance(interface_appearance appearance);

private:
	friend void ApplyInterfaceAppearance(wxWindow& window);

	void Apply(wxWindow& window);
	void ApplyRecursively(wxWindow& window);
	void ConfigureNativeAppearance();
	void QueueSystemAppearanceUpdate();
	void RefreshTopLevelWindows();

	interface_appearance configured_{};
	bool dark_{};
	bool update_pending_{};
};

bool IsDarkInterface();
bool IsDarkSystemTheme();
wxColour GetInterfaceColour(interface_colour role);
void ApplyInterfaceAppearance(wxWindow& window);
void SetInterfaceAppearance(interface_appearance appearance);

class CWindowTinter final
{
public:
	CWindowTinter(wxWindow& wnd);
	~CWindowTinter();

	void SetBackgroundTint(site_colour tint);

private:
	wxColour GetOriginalColor();
	void SetBackgroundTint(wxColour const& tint);
	void OnColorChange(wxSysColourChangedEvent &);

	site_colour tint_{};
	wxWindow& m_wnd;
};

#endif
