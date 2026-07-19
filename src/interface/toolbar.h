#ifndef FILEZILLA_INTERFACE_TOOLBAR_HEADER
#define FILEZILLA_INTERFACE_TOOLBAR_HEADER

#include "state.h"

#include "option_change_event_handler.h"

#include <memory>

#include <wx/panel.h>
#include <wx/toolbar.h>

class CMainFrame;
class wxImageList;

class CToolBar final : public wxPanel, public CGlobalStateEventHandler, public COptionChangeEventHandler
{
public:
	CToolBar(CMainFrame& mainFrame, COptions& options);
	virtual ~CToolBar();

	void UpdateToolbarState();
	void ToggleTool(int id, bool toggle);
	void EnableTool(int id, bool enable);
	wxToolBar* GetToolBarForTool(int id) const;

	bool ShowTool(int id);
	bool HideTool(int id);

protected:
	void MakeTool(wxToolBar& toolbar, char const* id, std::wstring const& art, wxString const& tooltip, wxString const& help = wxString(), wxItemKind type = wxITEM_NORMAL);
	void MakeTools();
	wxToolBar* FindToolBar(int id) const;
	void RefreshHoverImages();
	void RefreshToolbars();

#ifdef __WXMSW__
	void RefreshHoverImages(
		wxToolBar& toolbar, std::unique_ptr<wxImageList>& images);
	WXLRESULT MSWWindowProc(WXUINT message, WXWPARAM wParam, WXLPARAM lParam) override;
#endif

	virtual void OnStateChange(CState* pState, t_statechange_notifications notification, std::wstring const& data, const void* data2) override;
	virtual void OnOptionsChanged(watched_options const& options);

	CMainFrame & mainFrame_;
	COptions & options_;

	struct HiddenTool final
	{
		wxToolBar* toolbar{};
		int position{};
		wxToolBarToolBase* tool{};
	};

	wxToolBar* localToolBar_{};
	wxToolBar* remoteToolBar_{};
	std::map<int, HiddenTool> hiddenTools_;
	std::map<int, bool> selectedTools_;

#ifdef __WXMSW__
	std::unique_ptr<wxImageList> localHoverImages_;
	std::unique_ptr<wxImageList> remoteHoverImages_;
#endif

	wxSize iconSize_;
};

#endif
