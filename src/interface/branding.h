#ifndef FILEZILLA_INTERFACE_BRANDING_HEADER
#define FILEZILLA_INTERFACE_BRANDING_HEADER

#include <wx/string.h>

namespace branding {

inline wxString const& GetProductName()
{
	static wxString const name = L"Ninja Transfer";
	return name;
}

inline wxString const& GetWindowTitle()
{
	static wxString const title = L"Ninja Transfer · NinjaSoft";
	return title;
}

inline wchar_t const* GetAppUserModelId()
{
	return L"NinjaSoft.NinjaTransfer";
}

inline wxString GetInterfaceIconArtId(bool dark)
{
	return dark ? L"ART_NINJATRANSFER_DARK" : L"ART_NINJATRANSFER_LIGHT";
}

}

#endif
