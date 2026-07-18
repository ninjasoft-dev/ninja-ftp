#ifndef FILEZILLA_INTERFACE_BRANDING_HEADER
#define FILEZILLA_INTERFACE_BRANDING_HEADER

#include <wx/string.h>

namespace branding {

inline wxString const& GetProductName()
{
	static wxString const name = L"NinjaSoft FTP";
	return name;
}

inline wxString const& GetWindowTitle()
{
	static wxString const title = L"NinjaSoft FTP";
	return title;
}

inline wchar_t const* GetAppUserModelId()
{
	return L"NinjaSoft.FTP";
}

inline wxString GetInterfaceIconArtId(bool dark)
{
	return dark ? L"ART_NINJASOFTFTP_DARK" : L"ART_NINJASOFTFTP_LIGHT";
}

}

#endif
