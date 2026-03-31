#pragma once
#include "AppTypes.h"
#include "ResultListCtrl.h"
#include "NetworkChecker.h"
#include "ConfigManager.h"
#include "HtmlExporter.h"

// ──────────────────────────────────────────────────────────────────────────────
// Toolbar image indices
// ──────────────────────────────────────────────────────────────────────────────
enum TbImg
{
    IMG_RUN = 0,
    IMG_STOP,
    IMG_HTML,
    IMG_SAVE,       // icon_save.ico    – Guardar config
    IMG_RELOAD,
    IMG_CFGEDIT,    // icon_cfgedit.ico – Editor de configuración
    IMG_VIEW_TABS,  // view_tabs.ico  – Cambiar a vista por pestañas
    IMG_VIEW_LIST,  // view_list.ico  – Cambiar a vista en lista
    IMG_HELP,       // help.ico       – Ayuda
    IMG_INFO,       // info.ico       – Acerca de / Info
    IMG_EXIT,
    IMG_COUNT
};

class CMainFrame : public CFrameWnd
{
    DECLARE_DYNAMIC(CMainFrame)

public:
    CMainFrame();
    ~CMainFrame() override;

    BOOL PreCreateWindow(CREATESTRUCT& cs) override;
    BOOL OnCmdMsg(UINT nID, int nCode, void* pExtra,
                  AFX_CMDHANDLERINFO* pHandlerInfo) override;

protected:
    int  OnCreate(LPCREATESTRUCT lpCreateStruct);
    void OnClose();
    void OnDestroy();
    afx_msg void OnSize(UINT nType, int cx, int cy);

    // ── Toolbar button handlers ──────────────────────────────────────────────
    void OnRunStop();
    void OnStop();
    void OnSaveHtml();
    void OnSaveCfg();
    void OnReloadCfg();
    void OnCfgWiz();
    void OnViewToggle();
    void OnHelp();
    void OnInfo();
    void OnAutofit();
    void OnFileExit();

    // ── Update-UI handlers ───────────────────────────────────────────────────
    void OnUpdateRunStop   (CCmdUI* pCmdUI);
    void OnUpdateSaveHtml  (CCmdUI* pCmdUI);
    void OnUpdateSaveCfg   (CCmdUI* pCmdUI);
    void OnUpdateReloadCfg (CCmdUI* pCmdUI);

    // ── Custom window messages ───────────────────────────────────────────────
    afx_msg LRESULT OnNcResult  (WPARAM wParam, LPARAM lParam);
    afx_msg LRESULT OnNcComplete(WPARAM wParam, LPARAM lParam);
    afx_msg void    OnTabSelChange(NMHDR* pNMHDR, LRESULT* pResult);
    afx_msg BOOL    OnToolTipText(UINT id, NMHDR* pNMHDR, LRESULT* pResult);
    afx_msg void    OnActivate(UINT nState, CWnd* pWndOther, BOOL bMinimized);

    DECLARE_MESSAGE_MAP()

private:
    // ── Controls ─────────────────────────────────────────────────────────────
    CToolBar        m_toolbar;
    CStatusBar      m_statusBar;
    CFont           m_sbFont;          // custom font for status bar
    CProgressCtrl   m_progress;
    CResultListCtrl m_listCtrl;
    CTabCtrl        m_tabCtrl;          // tab bar for per-server view
    std::vector<CResultListCtrl*> m_tabLists; // one list per server tab
    CImageList      m_ilNormal;
    CImageList      m_ilDisabled;

    // ── State ─────────────────────────────────────────────────────────────────
    AppConfig                      m_cfg;
    std::vector<DestinationResult> m_results;
    NetworkChecker                 m_checker;
    ConfigManager                  m_cfgMgr;
    std::wstring                   m_sourceIP;
    bool                           m_cfgDirty   { false };
    bool                           m_cfgExists  { false };
    bool                           m_hasResults { false };
    bool                           m_tabMode    { false };
    int                            m_timeoutMs  { 1000 };  // port check timeout

    // ── Helpers ───────────────────────────────────────────────────────────────
    void BuildToolbar();
    void BuildImageLists();
    void ApplyToolbarMetrics();   // reapply button size/padding + stretch separator
    void PopulateCurrentView();    // refresh list or tab view
    void ApplyViewMode();          // show/hide list vs tab+lists, resize
    void DestroyTabLists();        // free m_tabLists
    void PopulateTabView();        // fill all per-server lists
    void UpdateViewToggleBtn();    // swap icon on the toggle button
    void LayoutContent(int cx, int cy); // common resize for list/tab mode

    void DoLoadConfig(bool showSetupIfMissing = true);
    void DoReloadConfig(bool force);
    bool DoSaveCfg();
    void DoRunCheck();
    void DoStopCheck();
    void RebuildResults();
    void SetProgress(int cur, int total);
    void SetStatus(const wchar_t* text);
    void SetSourceIPPane(const std::wstring& ip);
    void SetTimeoutPane();
    void SyncToolbarRunStop(bool running);

    void ApplyBatchToggle(int destIdx, Protocol const* proto, bool enable);
    void SyncPortEnabled(int di, int pi);   // refresh checkbox in both views

    int  TotalPortCount() const;
    int  CompletedPortCount() const;
};
