#pragma once
#include "AppTypes.h"
#include "ResultListCtrl.h"
#include "NetworkChecker.h"
#include "ConfigManager.h"
#include "HtmlExporter.h"
#include "UdpListener.h"
#include "NicSelectDlg.h"

// ──────────────────────────────────────────────────────────────────────────────
// Toolbar image indices
// ──────────────────────────────────────────────────────────────────────────────
enum TbImg
{
    IMG_RUN = 0,
    IMG_STOP,
    IMG_HTML,
    IMG_SAVE,       // cfg_save.ico   – Guardar config
    IMG_RELOAD,     // cfg_reload.ico – Recargar config
    IMG_CFGEDIT,    // cfg_edit.ico   – Editor de configuración
    IMG_VIEW_TABS,  // view_tabs.ico  – Cambiar a vista por pestañas
    IMG_VIEW_LIST,  // view_list.ico  – Cambiar a vista en lista
    IMG_HELP,       // help.ico       – Ayuda
    IMG_INFO,       // info.ico       – Acerca de
    IMG_EXIT,
    IMG_RPC,        // coms_range.ico – Escaneo RPC
    IMG_SERVER,     // server.ico     – Activar modo escucha
    IMG_CLIENT,     // win_client.ico – Volver a modo cliente
    IMG_NETCARD,    // network_card.ico – Selección NIC
    IMG_COMS_TX_RX, // coms_tx-rx.ico   – Banner-probe OFF (clic activa TX/RX)
    IMG_COMS_TX,    // coms_tx.ico      – Banner-probe ON  (clic vuelve a solo TX)
    IMG_COUNT
};

class CMainFrame : public CFrameWnd
{
    DECLARE_DYNAMIC(CMainFrame)

public:
    CMainFrame();
    ~CMainFrame() override;

    BOOL PreCreateWindow(CREATESTRUCT& cs) override;

protected:
    int  OnCreate(LPCREATESTRUCT lpCreateStruct);
    void OnClose();
    void OnDestroy();
    afx_msg void OnSize(UINT nType, int cx, int cy);

    // ── Toolbar button handlers ──────────────────────────────────────────────
    void OnRunStop();
    void OnSaveHtml();
    void OnSaveCfg();
    void OnReloadCfg();
    void OnCfgWiz();
    void OnViewToggle();
    void OnHelp();
    void OnInfo();
    void OnFileExit();
    void OnRpcScan();
    void OnListenToggle();
    void OnNicSelect();
    void OnBannerProbe();

    // ── Update-UI handlers ───────────────────────────────────────────────────
    void OnUpdateRunStop     (CCmdUI* pCmdUI);
    void OnUpdateSaveHtml    (CCmdUI* pCmdUI);
    void OnUpdateSaveCfg     (CCmdUI* pCmdUI);
    void OnUpdateReloadCfg   (CCmdUI* pCmdUI);
    void OnUpdateBannerProbe (CCmdUI* pCmdUI);

    // ── Custom window messages ───────────────────────────────────────────────
    afx_msg LRESULT OnNcResult  (WPARAM wParam, LPARAM lParam);
    afx_msg LRESULT OnNcComplete(WPARAM wParam, LPARAM lParam);
    afx_msg LRESULT OnListenPkt (WPARAM wParam, LPARAM lParam);
    afx_msg void    OnTabSelChange(NMHDR* pNMHDR, LRESULT* pResult);
    afx_msg BOOL    OnToolTipText(UINT id, NMHDR* pNMHDR, LRESULT* pResult);
    afx_msg void    OnActivate(UINT nState, CWnd* pWndOther, BOOL bMinimized);

    DECLARE_MESSAGE_MAP()

private:
    // ── Controls ─────────────────────────────────────────────────────────────
    CToolBar        m_toolbar;
    CStatusBar      m_statusBar;
    CFont           m_sbFont;
    CProgressCtrl   m_progress;
    CResultListCtrl m_listCtrl;
    CListCtrl       m_listenCtrl;   // lista de paquetes UDP recibidos
    CTabCtrl        m_tabCtrl;          // tab bar for per-server view
    std::vector<CResultListCtrl*> m_tabLists; // one list per server tab
    CImageList      m_ilNormal;
    CImageList      m_ilDisabled;

    // ── State ─────────────────────────────────────────────────────────────────
    AppConfig                      m_cfg;
    std::vector<DestinationResult> m_results;
    NetworkChecker                 m_checker;
    ConfigManager                  m_cfgMgr;
    UdpListener                    m_listener;
    std::wstring                   m_sourceIP;
    bool                           m_cfgDirty   { false };
    bool                           m_cfgExists  { false };
    bool                           m_hasResults { false };
    bool                           m_tabMode    { false };
    bool                           m_listenMode { false };
    int                            m_timeoutMs  { 1000 };
    std::wstring                   m_bindIP;             // NIC seleccionada (vacío = auto)
    std::wstring                   m_bindName;           // nombre amigable de la NIC seleccionada

    // Contadores incrementales para evitar O(N) en CompletedPortCount
    // y TotalPortCount cada vez que llega un WM_NC_RESULT.
    int                            m_totalPorts { 0 };
    int                            m_donePorts  { 0 };

    // Estado del sondeo de banner TCP — sólo en memoria, NO se persiste
    // en el fichero de configuración. Cada arranque empieza en OFF.
    bool                           m_bannerProbe { false };

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
    void SyncToolbarBannerProbe();   // sincroniza icono según m_bannerProbe (sesión)

    void SyncListenToggleBtn();
    void DoStartListen();
    void DoStopListen();
    void InitListenCtrl();
    std::vector<int> CollectUdpPorts() const;
    void UpdateNicPane();
    void AutoSelectNic();  // selección automática de NIC por defecto al arrancar
    void SyncPortEnabled(int di, int pi);   // refresh checkbox in both views
    void ApplyBatchToggle(int destIdx, Protocol const* proto, bool enable);

    int  TotalPortCount() const;
    int  CompletedPortCount() const;
};
