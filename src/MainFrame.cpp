#include "pch.h"
#include "MainFrame.h"
#include "ConfigEditorDlg.h"
#include "AboutDlg.h"
#include "RpcScanDlg.h"
#include "resource.h"

IMPLEMENT_DYNAMIC(CMainFrame, CFrameWnd)

BEGIN_MESSAGE_MAP(CMainFrame, CFrameWnd)
    ON_WM_CREATE()
    ON_WM_CLOSE()
    ON_WM_DESTROY()
    ON_WM_SIZE()
    ON_WM_ACTIVATE()
    ON_NOTIFY(TCN_SELCHANGE, AFX_IDW_PANE_FIRST + 1, &CMainFrame::OnTabSelChange)
    ON_NOTIFY_EX(TTN_NEEDTEXTW, 0, &CMainFrame::OnToolTipText)
    ON_NOTIFY_EX(TTN_NEEDTEXTA, 0, &CMainFrame::OnToolTipText)

    // Toolbar button commands
    ON_COMMAND(IDC_BTN_RUN_STOP,    &CMainFrame::OnRunStop)
    ON_COMMAND(IDC_BTN_SAVE_HTML,   &CMainFrame::OnSaveHtml)
    ON_COMMAND(IDC_BTN_SAVE_CFG,    &CMainFrame::OnSaveCfg)
    ON_COMMAND(IDC_BTN_RELOAD_CFG,  &CMainFrame::OnReloadCfg)
    ON_COMMAND(IDC_BTN_AUTOFIT,     &CMainFrame::OnAutofit)
    ON_COMMAND(IDC_BTN_VIEW_TOGGLE, &CMainFrame::OnViewToggle)
    ON_COMMAND(IDC_BTN_CFG_WIZ,     &CMainFrame::OnCfgWiz)
    ON_COMMAND(IDC_BTN_HELP,        &CMainFrame::OnHelp)
    ON_COMMAND(IDC_BTN_INFO,        &CMainFrame::OnInfo)
    ON_COMMAND(IDC_BTN_EXIT,        &CMainFrame::OnFileExit)
    ON_COMMAND(IDC_BTN_RPC_SCAN,    &CMainFrame::OnRpcScan)

    // Update-UI handlers
    ON_UPDATE_COMMAND_UI(IDC_BTN_RUN_STOP,   &CMainFrame::OnUpdateRunStop)
    ON_UPDATE_COMMAND_UI(IDC_BTN_SAVE_HTML,  &CMainFrame::OnUpdateSaveHtml)
    ON_UPDATE_COMMAND_UI(IDC_BTN_SAVE_CFG,   &CMainFrame::OnUpdateSaveCfg)
    ON_UPDATE_COMMAND_UI(IDC_BTN_RELOAD_CFG, &CMainFrame::OnUpdateReloadCfg)

    ON_MESSAGE(WM_NC_RESULT,   &CMainFrame::OnNcResult)
    ON_MESSAGE(WM_NC_COMPLETE, &CMainFrame::OnNcComplete)
END_MESSAGE_MAP()

// ──────────────────────────────────────────────────────────────────────────────
CMainFrame::CMainFrame()  = default;
CMainFrame::~CMainFrame() = default;

BOOL CMainFrame::PreCreateWindow(CREATESTRUCT& cs)
{
    if (!CFrameWnd::PreCreateWindow(cs)) return FALSE;
    // Remove the menu bar slot from the window style
    cs.style = (WS_OVERLAPPEDWINDOW & ~WS_SYSMENU) | WS_SYSMENU | FWS_ADDTOTITLE;
    cs.hMenu = nullptr;
    cs.cx = 1024; cs.cy = 640;
    return TRUE;
}

// ──────────────────────────────────────────────────────────────────────────────
// WM_CREATE
// ──────────────────────────────────────────────────────────────────────────────
int CMainFrame::OnCreate(LPCREATESTRUCT lpcs)
{
    if (CFrameWnd::OnCreate(lpcs) == -1) return -1;

    // ── Status bar ────────────────────────────────────────────────────────────
    // Pane 0: status text (stretchy)
    // Pane 1: progress bar (fixed 200 px)
    // Pane 2: source IP    (fixed 160 px)
    static UINT indicators[] = { ID_SEPARATOR, ID_SEPARATOR, ID_SEPARATOR, ID_SEPARATOR };
    m_statusBar.Create(this);
    m_statusBar.SetIndicators(indicators, 4);
    m_statusBar.SetPaneInfo(0, ID_SEPARATOR, SBPS_STRETCH, 0);
    m_statusBar.SetPaneInfo(1, ID_SEPARATOR, SBPS_NORMAL,  200);
    m_statusBar.SetPaneInfo(2, ID_SEPARATOR, SBPS_NORMAL,  150);
    m_statusBar.SetPaneInfo(3, ID_SEPARATOR, SBPS_NORMAL,  120);

    // Taller status bar with a slightly larger font (10pt Segoe UI)
    m_sbFont.CreateFont(
        -14,                    // height (negative = character height, ~10.5pt at 96dpi)
        0, 0, 0, FW_NORMAL,
        FALSE, FALSE, FALSE,
        DEFAULT_CHARSET, OUT_DEFAULT_PRECIS, CLIP_DEFAULT_PRECIS,
        DEFAULT_QUALITY, DEFAULT_PITCH | FF_SWISS,
        L"Segoe UI");
    m_statusBar.SetFont(&m_sbFont);

    // Force minimum height: get current size rect and expand
    CRect rcSb;
    m_statusBar.GetWindowRect(&rcSb);
    int newH = max(rcSb.Height(), 32);  // at least 32px tall
    m_statusBar.SetWindowPos(nullptr, 0, 0, rcSb.Width(), newH,
                             SWP_NOMOVE | SWP_NOZORDER | SWP_NOACTIVATE);

    // ── Toolbar ───────────────────────────────────────────────────────────────
    BuildToolbar();

    // Apply persisted toolbar visibility (will be set after config load)
    // Done in DoLoadConfig once m_cfg is populated.

    // ── ListView ─────────────────────────────────────────────────────────────
    CRect rcClient;
    GetClientRect(&rcClient);
    m_listCtrl.Create(
        WS_CHILD | WS_VISIBLE | LVS_REPORT | LVS_SHOWSELALWAYS,
        rcClient, this, AFX_IDW_PANE_FIRST);
    m_listCtrl.Initialise();

    // ── Tab control (hidden by default – list mode) ───────────────────────────
    m_tabCtrl.Create(WS_CHILD | TCS_TABS | TCS_FOCUSNEVER,
                     rcClient, this, AFX_IDW_PANE_FIRST + 1);
    m_tabCtrl.ShowWindow(SW_HIDE);

    // Checkbox toggle callback
    m_listCtrl.SetCheckToggleCb([this](int di, int pi, bool enabled)
    {
        if (di < static_cast<int>(m_cfg.destinations.size()) &&
            pi < static_cast<int>(m_cfg.destinations[di].ports.size()))
        {
            m_cfg.destinations[di].ports[pi].enabled = enabled;
            if (di < static_cast<int>(m_results.size()) &&
                pi < static_cast<int>(m_results[di].portResults.size()))
                m_results[di].portResults[pi].enabled = enabled;
            m_cfgDirty = true;
            SyncPortEnabled(di, pi);
        }
    });

    // Batch toggle callback (from right-click context menu)
    m_listCtrl.SetBatchToggleCb([this](int destIdx, Protocol const* proto, bool enable)
    {
        ApplyBatchToggle(destIdx, proto, enable);
    });

    // ── Progress bar (inside pane 1 of the status bar) ────────────────────────
    CRect rcPane;
    m_statusBar.GetItemRect(1, &rcPane);
    // GetItemRect returns client coords of the status bar — use directly
    m_progress.Create(WS_CHILD | PBS_SMOOTH, rcPane, &m_statusBar, 100);
    m_progress.SetRange(0, 100);
    m_progress.ShowWindow(SW_HIDE);


    SetStatus(L"Listo");
    SetTimeoutPane();

    // ── Load config ───────────────────────────────────────────────────────────
    DoLoadConfig(true);

    RecalcLayout();

    return 0;
}



// ──────────────────────────────────────────────────────────────────────────────
// Toolbar construction with Unicode symbol bitmaps
// ──────────────────────────────────────────────────────────────────────────────
void CMainFrame::BuildToolbar()
{
    m_toolbar.CreateEx(this, TBSTYLE_FLAT,
        WS_CHILD | WS_VISIBLE | CBRS_TOP | CBRS_TOOLTIPS | CBRS_FLYBY);
    // Prevent icon pixel-shift on button press (doublebuffer suppresses offset drawing)
    m_toolbar.GetToolBarCtrl().SendMessage(
        TB_SETEXTENDEDSTYLE, 0,
        TBSTYLE_EX_DOUBLEBUFFER | m_toolbar.GetToolBarCtrl().SendMessage(TB_GETEXTENDEDSTYLE, 0, 0));

    static TBBUTTON tbb[] =
    {
        // 1. Comprobar / Detener
        {IMG_RUN,      IDC_BTN_RUN_STOP,   TBSTATE_ENABLED, TBSTYLE_BUTTON, {}, 0, (INT_PTR)L"Comprobar / Detener"},
        {0,            0,                  TBSTATE_ENABLED, TBSTYLE_SEP,    {}, 0, 0},
        // 2. Editor de configuración
        {IMG_CFGEDIT,  IDC_BTN_CFG_WIZ,    TBSTATE_ENABLED, TBSTYLE_BUTTON, {}, 0, (INT_PTR)L"Editor de configuraci\xf3n"},
        // 3. Recargar configuración
        {IMG_RELOAD,   IDC_BTN_RELOAD_CFG, TBSTATE_ENABLED, TBSTYLE_BUTTON, {}, 0, (INT_PTR)L"Recargar configuraci\xf3n"},
        // 4. Guardar configuración
        {IMG_SAVE,     IDC_BTN_SAVE_CFG,   TBSTATE_ENABLED, TBSTYLE_BUTTON, {}, 0, (INT_PTR)L"Guardar configuraci\xf3n"},
        {0,            0,                  TBSTATE_ENABLED, TBSTYLE_SEP,    {}, 0, 0},
        // 5. Guardar informe HTML
        {IMG_HTML,     IDC_BTN_SAVE_HTML,  TBSTATE_ENABLED, TBSTYLE_BUTTON, {}, 0, (INT_PTR)L"Guardar informe HTML"},
        {0,            0,                  TBSTATE_ENABLED, TBSTYLE_SEP,    {}, 0, 0},
        // 6. Escaneo RPC dinámico
        {IMG_RPC,      IDC_BTN_RPC_SCAN,   TBSTATE_ENABLED, TBSTYLE_BUTTON, {}, 0, (INT_PTR)L"Escaneo RPC din\xe1mico"},
        {0,            0,                  TBSTATE_ENABLED, TBSTYLE_SEP,    {}, 0, 0},
        // 7. Cambiar vista
        {IMG_VIEW_TABS,IDC_BTN_VIEW_TOGGLE,TBSTATE_ENABLED, TBSTYLE_BUTTON, {}, 0, (INT_PTR)L"Cambiar vista"},
        {0,            0,                  TBSTATE_ENABLED, TBSTYLE_SEP,    {}, 0, 0},
        // 8. Salir
        {IMG_EXIT,     IDC_BTN_EXIT,       TBSTATE_ENABLED, TBSTYLE_BUTTON, {}, 0, (INT_PTR)L"Salir"},
        // separador elástico – empuja Ayuda e Info al extremo derecho
        {0,            IDC_BTN_INFO_SEP,   TBSTATE_ENABLED, TBSTYLE_SEP,    {}, 0, 0},
        // 9. Ayuda
        {IMG_HELP,     IDC_BTN_HELP,       TBSTATE_ENABLED, TBSTYLE_BUTTON, {}, 0, (INT_PTR)L"Ayuda"},
        // 10. Acerca de
        {IMG_INFO,     IDC_BTN_INFO,       TBSTATE_ENABLED, TBSTYLE_BUTTON, {}, 0, (INT_PTR)L"Acerca de"},
    };

    m_toolbar.GetToolBarCtrl().AddButtons(
        static_cast<int>(_countof(tbb)), tbb);
    m_toolbar.GetToolBarCtrl().SetButtonSize(CSize(40, 40));
    m_toolbar.GetToolBarCtrl().SendMessage(TB_SETPADDING, 0, MAKELPARAM(10, 6));

    BuildImageLists();
}
// ──────────────────────────────────────────────────────────────────────────────
// IconToBitmap
// Draws an HICON into a 32-bpp top-down DIB section and returns both the
// HBITMAP and a direct pointer to the pixel buffer.
// The caller MUST ::DeleteObject(hBmp) when done.
// pBitsOut may be nullptr if the caller doesn't need raw pixel access.
// ──────────────────────────────────────────────────────────────────────────────
static HBITMAP IconToBitmap(HICON hIcon, int sz, void** pBitsOut = nullptr)
{
    BITMAPINFOHEADER bih {};
    bih.biSize        = sizeof(bih);
    bih.biWidth       = sz;
    bih.biHeight      = -sz;   // top-down
    bih.biPlanes      = 1;
    bih.biBitCount    = 32;
    bih.biCompression = BI_RGB;

    void* pBits = nullptr;
    HDC hScreen = ::GetDC(nullptr);
    HBITMAP hBmp = ::CreateDIBSection(hScreen,
                                       reinterpret_cast<BITMAPINFO*>(&bih),
                                       DIB_RGB_COLORS, &pBits, nullptr, 0);
    HDC hDC = ::CreateCompatibleDC(hScreen);
    ::ReleaseDC(nullptr, hScreen);

    HBITMAP hOld = static_cast<HBITMAP>(::SelectObject(hDC, hBmp));
    memset(pBits, 0, sz * sz * 4);                                // transparent black
    ::DrawIconEx(hDC, 0, 0, hIcon, sz, sz, 0, nullptr, DI_NORMAL);// writes pre-mult ARGB
    ::SelectObject(hDC, hOld);
    ::DeleteDC(hDC);

    if (pBitsOut) *pBitsOut = pBits;
    return hBmp;
}

// ──────────────────────────────────────────────────────────────────────────────
// MakeDisabledBitmap
// Produces a greyscale + 40 % opacity version of a normal icon bitmap by
// working directly on the DIB pixel buffer — no GDI+ needed, no background
// colour baked in.  The result is a 32-bpp ARGB DIB with full transparency.
//
// Each pixel in the source is pre-multiplied ARGB (what DrawIconEx writes).
// We un-premultiply, desaturate, scale alpha, then re-premultiply.
// ──────────────────────────────────────────────────────────────────────────────
static HBITMAP MakeDisabledBitmap(HICON hIcon, int sz)
{
    // Render the icon into a source DIB
    void* pSrcBits = nullptr;
    HBITMAP hSrc = IconToBitmap(hIcon, sz, &pSrcBits);
    if (!hSrc) return nullptr;

    // Allocate destination DIB
    BITMAPINFOHEADER bih {};
    bih.biSize        = sizeof(bih);
    bih.biWidth       = sz;
    bih.biHeight      = -sz;
    bih.biPlanes      = 1;
    bih.biBitCount    = 32;
    bih.biCompression = BI_RGB;

    void* pDstBits = nullptr;
    HDC hScreen = ::GetDC(nullptr);
    HBITMAP hDst = ::CreateDIBSection(hScreen,
                                       reinterpret_cast<BITMAPINFO*>(&bih),
                                       DIB_RGB_COLORS, &pDstBits, nullptr, 0);
    ::ReleaseDC(nullptr, hScreen);

    // Per-pixel: un-premultiply → desaturate → 40 % alpha → re-premultiply
    const DWORD nPixels = static_cast<DWORD>(sz * sz);
    const BYTE* src = static_cast<const BYTE*>(pSrcBits);
          BYTE* dst = static_cast<      BYTE*>(pDstBits);

    for (DWORD p = 0; p < nPixels; ++p, src += 4, dst += 4)
    {
        BYTE srcA = src[3];   // alpha (straight after DrawIconEx on Win10+)
        if (srcA == 0)
        {
            dst[0] = dst[1] = dst[2] = dst[3] = 0;   // fully transparent
            continue;
        }

        // Un-premultiply to get straight RGB (DrawIconEx may or may not
        // premultiply depending on Windows version; guard with clamp)
        BYTE r = static_cast<BYTE>(min(255u, (UINT)src[2] * 255u / srcA));
        BYTE g = static_cast<BYTE>(min(255u, (UINT)src[1] * 255u / srcA));
        BYTE b = static_cast<BYTE>(min(255u, (UINT)src[0] * 255u / srcA));

        // Luminance (BT.601 weights)
        BYTE lum = static_cast<BYTE>(r * 299u / 1000u +
                                      g * 587u / 1000u +
                                      b * 114u / 1000u);

        // New alpha = 40 % of original
        BYTE newA = static_cast<BYTE>(srcA * 40u / 100u);

        // Re-premultiply
        dst[2] = static_cast<BYTE>((UINT)lum * newA / 255u);   // R
        dst[1] = static_cast<BYTE>((UINT)lum * newA / 255u);   // G
        dst[0] = static_cast<BYTE>((UINT)lum * newA / 255u);   // B
        dst[3] = newA;                                           // A
    }

    ::DeleteObject(hSrc);
    return hDst;
}

void CMainFrame::BuildImageLists()
{
    constexpr int SZ = 32;   // toolbar icon size
    HINSTANCE hInst = AfxGetInstanceHandle();

    // ICON resource IDs in toolbar button order
    static const UINT resIds[IMG_COUNT] =
    {
        IDI_ICON_RUN,       // IMG_RUN
        IDI_ICON_STOP,      // IMG_STOP
        IDI_ICON_HTML,      // IMG_HTML
        IDI_ICON_SAVE,      // IMG_SAVE
        IDI_ICON_RELOAD,    // IMG_RELOAD
        IDI_ICON_CFGEDIT,   // IMG_CFGEDIT
        IDI_ICON_VIEWTABS,  // IMG_VIEW_TABS
        IDI_ICON_VIEWLIST,  // IMG_VIEW_LIST
        IDI_ICON_HELP,      // IMG_HELP
        IDI_ICON_INFO,      // IMG_INFO
        IDI_ICON_EXIT,      // IMG_EXIT
        IDI_ICON_RPC_SCAN,  // IMG_RPC  coms_range.ico
    };

    m_ilNormal  .Create(SZ, SZ, ILC_COLOR32, IMG_COUNT, 0);
    m_ilDisabled.Create(SZ, SZ, ILC_COLOR32, IMG_COUNT, 0);

    for (int i = 0; i < IMG_COUNT; ++i)
    {
        HICON hIcon = static_cast<HICON>(
            ::LoadImageW(hInst, MAKEINTRESOURCEW(resIds[i]),
                         IMAGE_ICON, SZ, SZ, LR_DEFAULTCOLOR));
        if (!hIcon) continue;

        // Normal: full colour, full alpha
        HBITMAP hNormal = IconToBitmap(hIcon, SZ);
        if (hNormal)
        {
            m_ilNormal.Add(CBitmap::FromHandle(hNormal), static_cast<CBitmap*>(nullptr));
            ::DeleteObject(hNormal);
        }

        // Disabled: greyscale + 40 % alpha, transparent background
        HBITMAP hDisabled = MakeDisabledBitmap(hIcon, SZ);
        if (hDisabled)
        {
            m_ilDisabled.Add(CBitmap::FromHandle(hDisabled), static_cast<CBitmap*>(nullptr));
            ::DeleteObject(hDisabled);
        }

        ::DestroyIcon(hIcon);
    }

    m_toolbar.GetToolBarCtrl().SendMessage(
        TB_SETIMAGELIST, 0,
        reinterpret_cast<LPARAM>(m_ilNormal.m_hImageList));
    m_toolbar.GetToolBarCtrl().SendMessage(
        TB_SETDISABLEDIMAGELIST, 0,
        reinterpret_cast<LPARAM>(m_ilDisabled.m_hImageList));
}


// ──────────────────────────────────────────────────────────────────────────────
// Config helpers
// ──────────────────────────────────────────────────────────────────────────────
void CMainFrame::DoLoadConfig(bool showSetupIfMissing)
{
    if (m_cfgMgr.Load(CONFIG_FILE, m_cfg))
    {
        m_sourceIP = NetworkChecker::GetLocalIP();
        m_cfgExists = true;
        m_cfgDirty  = false;
        RebuildResults();
        PopulateCurrentView();
        SetSourceIPPane(m_sourceIP);
        m_timeoutMs = m_cfg.timeoutMs;
        SetTimeoutPane();
        SetStatus(L"Configuración cargada.");
    }
    else if (showSetupIfMissing)
    {
        // No config file – open editor so user can create one
        AppConfig workCfg;
        CConfigEditorDlg dlg(workCfg, this);
        if (dlg.DoModal() == IDOK && !workCfg.destinations.empty())
        {
            m_cfg = std::move(workCfg);
            m_sourceIP = NetworkChecker::GetLocalIP();
            m_timeoutMs = m_cfg.timeoutMs;
            m_cfgMgr.Save(CONFIG_FILE, m_cfg);
            m_cfgExists = true;
            m_cfgDirty  = false;
            RebuildResults();
            PopulateCurrentView();
            SetSourceIPPane(m_sourceIP);
            SetTimeoutPane();
        }
    }
}

void CMainFrame::DoReloadConfig(bool force)
{
    if (!force && m_cfgDirty)
    {
        int r = MessageBox(
            L"Hay cambios sin guardar. ¿Recargar igualmente?",
            L"Recargar configuración", MB_YESNO | MB_ICONQUESTION);
        if (r != IDYES) return;
    }
    AppConfig tmp;
    if (m_cfgMgr.Load(CONFIG_FILE, tmp)) {
        m_cfg      = std::move(tmp);
        m_cfgExists = true;
        m_cfgDirty  = false;
        RebuildResults();
        PopulateCurrentView();
        SetSourceIPPane(m_sourceIP);
        SetStatus(L"Configuración recargada.");
    } else {
        MessageBox(m_cfgMgr.LastError().c_str(), L"Error", MB_ICONERROR);
    }
}

bool CMainFrame::DoSaveCfg()
{
    if (m_cfgMgr.Save(CONFIG_FILE, m_cfg)) {
        m_cfgDirty = false;
        SetStatus(L"Configuración guardada.");
        return true;
    }
    MessageBox(m_cfgMgr.LastError().c_str(), L"Error al guardar", MB_ICONERROR);
    return false;
}

// ──────────────────────────────────────────────────────────────────────────────
// Build m_results from m_cfg
// ──────────────────────────────────────────────────────────────────────────────
void CMainFrame::RebuildResults()
{
    m_results.clear();
    for (const auto& dc : m_cfg.destinations)
    {
        DestinationResult dr;
        dr.config = dc;
        for (const auto& pe : dc.ports)
            dr.portResults.emplace_back(pe);
        m_results.push_back(std::move(dr));
    }
    m_hasResults = false;
}

// ──────────────────────────────────────────────────────────────────────────────
// Run / Stop
// ──────────────────────────────────────────────────────────────────────────────
void CMainFrame::DoRunCheck()
{
    if (m_results.empty()) return;

    // Reset status
    for (auto& dr : m_results)
        for (auto& pr : dr.portResults)
            pr.status = ConnectStatus::PENDING;

    PopulateCurrentView();
    SetSourceIPPane(m_sourceIP);
    SetProgress(0, TotalPortCount());
    SetStatus(L"Comprobando conectividad…");
    SyncToolbarRunStop(true);
    m_hasResults = false;

    HWND hWnd = GetSafeHwnd();
    m_checker.SetTimeout(m_timeoutMs);
    m_checker.StartAsync(
        m_results,
        [hWnd](int di, int pi)
        {
            ::PostMessage(hWnd, WM_NC_RESULT,
                          static_cast<WPARAM>(di),
                          static_cast<LPARAM>(pi));
        },
        [hWnd]()
        {
            ::PostMessage(hWnd, WM_NC_COMPLETE, 0, 0);
        });
}

void CMainFrame::DoStopCheck()
{
    m_checker.Stop();
    SyncToolbarRunStop(false);
    SetStatus(L"Detenido.");
    m_progress.ShowWindow(SW_HIDE);
}

// ──────────────────────────────────────────────────────────────────────────────
// Message handlers
// ──────────────────────────────────────────────────────────────────────────────
LRESULT CMainFrame::OnNcResult(WPARAM wParam, LPARAM lParam)
{
    int di = static_cast<int>(wParam);
    int pi = static_cast<int>(lParam);
    if (di < static_cast<int>(m_results.size()) &&
        pi < static_cast<int>(m_results[di].portResults.size()))
    {
        const auto& pr  = m_results[di].portResults[pi];
        const auto& cfg = m_results[di].config;
        m_listCtrl.UpdateResult(di, pi, pr);
        // Also update the matching tab-list if in tab mode
        if (m_tabMode && di < (int)m_tabLists.size())
        {
            std::vector<DestinationResult> single = { m_results[di] };
            m_tabLists[di]->UpdateResult(0, pi, pr);
        }
        int done  = CompletedPortCount();
        int total = TotalPortCount();
        SetProgress(done, total);
        CString s;
        const wchar_t* proto = (pr.entry.protocol == Protocol::TCP) ? L"TCP" : L"UDP";
        s.Format(L"Comprobando %s:%d (%s)",
                 cfg.ip.c_str(), pr.entry.port, proto);
        SetStatus(static_cast<LPCWSTR>(s));
    }
    return 0;
}

LRESULT CMainFrame::OnNcComplete(WPARAM, LPARAM)
{
    SyncToolbarRunStop(false);
    m_progress.ShowWindow(SW_HIDE);
    m_hasResults = true;
    SetStatus(L"Comprobación completada.");
    return 0;
}

// ──────────────────────────────────────────────────────────────────────────────
// Command handlers
// ──────────────────────────────────────────────────────────────────────────────
void CMainFrame::OnRunStop()
{
    if (m_checker.IsRunning()) DoStopCheck();
    else                       DoRunCheck();
}

void CMainFrame::OnStop()         { DoStopCheck(); }
void CMainFrame::OnReloadCfg()    { DoReloadConfig(true); }
void CMainFrame::OnSaveCfg()      { DoSaveCfg(); }

void CMainFrame::OnSaveHtml()
{
    // Default filename: NetPortChk_AAAAMMDD_HHMM.html
    time_t now = time(nullptr);
    struct tm lt{};
    localtime_s(&lt, &now);
    wchar_t defaultName[64];
    wcsftime(defaultName, _countof(defaultName), L"NetPortChk_%Y%m%d_%H%M.html", &lt);

    CFileDialog dlg(FALSE, L"html", defaultName,
        OFN_OVERWRITEPROMPT,
        L"Archivos HTML (*.html)|*.html|Todos (*.*)|*.*||");
    if (dlg.DoModal() != IDOK) return;

    HtmlExporter exp;
    if (!exp.Export(dlg.GetPathName(), m_results, m_sourceIP, m_timeoutMs))
        MessageBox(L"Error al guardar el informe HTML.", L"Error", MB_ICONERROR);
    else
        SetStatus(L"Informe HTML guardado.");
}

void CMainFrame::OnCfgWiz()
{
    // Works for both creating a new config and editing the existing one.
    // The dialog receives a working copy and only commits on IDOK.
    AppConfig workCfg = m_cfg;
    CConfigEditorDlg dlg(workCfg, this);
    if (dlg.DoModal() == IDOK && !workCfg.destinations.empty())
    {
        m_cfg = std::move(workCfg);
        if (m_cfgMgr.Save(CONFIG_FILE, m_cfg))
        {
            m_cfgExists = true;
            m_cfgDirty  = false;
            RebuildResults();
            PopulateCurrentView();
            SetSourceIPPane(m_sourceIP);
            m_timeoutMs = m_cfg.timeoutMs;
            SetTimeoutPane();
            SetStatus(L"Configuración guardada.");
        }
        else
        {
            MessageBox(m_cfgMgr.LastError().c_str(), L"Error al guardar", MB_ICONERROR);
        }
    }
}

void CMainFrame::OnFileExit() { SendMessage(WM_CLOSE); }

void CMainFrame::OnHelp()
{
    wchar_t exePath[MAX_PATH] = {};
    GetModuleFileNameW(nullptr, exePath, MAX_PATH);
    wchar_t* lastSlash = wcsrchr(exePath, L'\\');
    if (lastSlash) *(lastSlash + 1) = L'\0';
    std::wstring helpPath = exePath;
    helpPath += L"NetPortChk.chm";

    // Remove Mark-of-the-Web (Zone.Identifier ADS) so the CHM opens
    // without the "This file came from the Internet" block.
    std::wstring zone = helpPath + L":Zone.Identifier";
    ::DeleteFileW(zone.c_str());   // no-op if the stream doesn't exist

    HWND hw = ::HtmlHelp(GetSafeHwnd(), helpPath.c_str(), HH_DISPLAY_TOC, 0);
    if (!hw)
    {
        HINSTANCE r = ShellExecuteW(nullptr, L"open",
            helpPath.c_str(), nullptr, nullptr, SW_SHOWNORMAL);
        if (reinterpret_cast<INT_PTR>(r) <= 32)
            MessageBox(L"No se encontr\xf3 NetPortChk.chm.\n"
                       L"Comp\xedlelo con HTML Help Workshop (hhc.exe NetPortChk.hhp)\n"
                       L"y col\xf3quelo junto al ejecutable.",
                       L"Ayuda", MB_ICONINFORMATION);
    }
}

void CMainFrame::OnInfo()
{
    CAboutDlg dlg;
    dlg.DoModal();
}

void CMainFrame::OnRpcScan()
{
    // Pre-fill IP from the first selected destination or the first result
    std::wstring ip;
    if (!m_results.empty())
    {
        POSITION pos = m_listCtrl.GetFirstSelectedItemPosition();
        if (pos)
        {
            int item = m_listCtrl.GetNextSelectedItem(pos);
            int di   = static_cast<int>(m_listCtrl.GetItemData(item));
            if (di >= 0 && di < static_cast<int>(m_results.size()))
                ip = m_results[di].config.ip;
        }
        if (ip.empty())
            ip = m_results[0].config.ip;
    }

    CRpcScanDlg dlg(ip);
    dlg.DoModal();
}

// ──────────────────────────────────────────────────────────────────────────────
// Update-UI
// ──────────────────────────────────────────────────────────────────────────────
void CMainFrame::OnUpdateRunStop(CCmdUI* pCmdUI)
{
    // Disable if no results loaded OR if every port is unchecked
    bool anyEnabled = false;
    for (const auto& dr : m_results) {
        for (const auto& pr : dr.portResults)
            if (pr.enabled) { anyEnabled = true; break; }
        if (anyEnabled) break;
    }
    pCmdUI->Enable(!m_results.empty() && anyEnabled);
}

void CMainFrame::OnUpdateSaveHtml(CCmdUI* pCmdUI)
{
    pCmdUI->Enable(m_hasResults);
}

void CMainFrame::OnUpdateSaveCfg(CCmdUI* pCmdUI)
{
    // Active only when a config was loaded AND there are unsaved changes
    pCmdUI->Enable(m_cfgExists && m_cfgDirty);
}

void CMainFrame::OnUpdateReloadCfg(CCmdUI* pCmdUI)
{
    // Active only when a config file has been successfully loaded at least once
    pCmdUI->Enable(m_cfgExists);
}

void CMainFrame::OnAutofit()
{
    m_listCtrl.AutoFitColumns();
}

// ──────────────────────────────────────────────────────────────────────────────
// WM_CLOSE – ask before exit if dirty
// ──────────────────────────────────────────────────────────────────────────────
void CMainFrame::OnClose()
{
    if (m_checker.IsRunning()) m_checker.Stop();

    if (m_cfgDirty)
    {
        int r = MessageBox(
            L"Hay cambios sin guardar en la configuración.\n"
            L"¿Desea guardar antes de salir?",
            L"Cambios pendientes",
            MB_YESNOCANCEL | MB_ICONQUESTION);
        if (r == IDCANCEL) return;
        if (r == IDYES && !DoSaveCfg()) return;
    }
    CFrameWnd::OnClose();
}

// ──────────────────────────────────────────────────────────────────────────────
// ApplyToolbarMetrics
// Reapplies button size/padding and recalculates the stretch separator that
// pushes HELP+INFO to the right edge.  Called from OnSize AND OnActivate so
// that the toolbar looks correct after RecalcLayout fires without a resize
// (e.g. when a modal dialog opens or closes).
// ──────────────────────────────────────────────────────────────────────────────
void CMainFrame::ApplyToolbarMetrics()
{
    if (!m_toolbar.GetSafeHwnd()) return;

    CToolBarCtrl& tb = m_toolbar.GetToolBarCtrl();

    // RecalcLayout (triggered by MFC on activate/resize) resets these; reapply.
    tb.SetButtonSize(CSize(40, 40));
    tb.SendMessage(TB_SETPADDING, 0, MAKELPARAM(10, 6));

    // Stretch separator so INFO+HELP stay right-aligned ──────────────────────
    int btnInfo = -1, btnHelp = -1, btnSep = -1;
    int count = tb.GetButtonCount();
    for (int i = 0; i < count; ++i)
    {
        TBBUTTON tbb{};
        tb.GetButton(i, &tbb);
        if (tbb.idCommand == IDC_BTN_INFO)     btnInfo = i;
        if (tbb.idCommand == IDC_BTN_HELP)     btnHelp = i;
        if (tbb.idCommand == IDC_BTN_INFO_SEP) btnSep  = i;
    }
    if (btnSep >= 0 && btnInfo >= 0 && btnHelp >= 0)
    {
        CRect rcInfo, rcHelp, rcSep, rcTb;
        tb.GetItemRect(btnInfo, &rcInfo);
        tb.GetItemRect(btnHelp, &rcHelp);
        tb.GetItemRect(btnSep,  &rcSep);
        tb.GetClientRect(&rcTb);
        int rightWidth = rcInfo.Width() + rcHelp.Width();
        int stretch = max(8, rcTb.Width() - rcSep.left - rightWidth - 4);
        TBBUTTONINFO tbi{}; tbi.cbSize = sizeof(tbi);
        tbi.dwMask = TBIF_SIZE; tbi.cx = static_cast<WORD>(stretch);
        tb.SetButtonInfo(IDC_BTN_INFO_SEP, &tbi);
    }
}

void CMainFrame::OnSize(UINT nType, int cx, int cy)
{
    CFrameWnd::OnSize(nType, cx, cy);
    if (!m_toolbar.GetSafeHwnd() || cx <= 0) return;

    ApplyToolbarMetrics();

    // ── Resize content area ───────────────────────────────────────────────────
    LayoutContent(cx, cy);
}

void CMainFrame::OnActivate(UINT nState, CWnd* pWndOther, BOOL bMinimized)
{
    CFrameWnd::OnActivate(nState, pWndOther, bMinimized);
    // When a modal dialog (e.g. ConfigEditorDlg) closes and the frame regains
    // focus, MFC calls RecalcLayout() which resets button size and padding.
    // Restore them here so the toolbar always looks consistent.
    if (nState == WA_ACTIVE || nState == WA_CLICKACTIVE)
        ApplyToolbarMetrics();
}

void CMainFrame::OnDestroy()
{
    DestroyTabLists();
    m_checker.Stop();
    CFrameWnd::OnDestroy();
}

// ──────────────────────────────────────────────────────────────────────────────
// OnToolTipText – supply tooltip text for toolbar buttons
// ──────────────────────────────────────────────────────────────────────────────
BOOL CMainFrame::OnToolTipText(UINT /*id*/, NMHDR* pNMHDR, LRESULT* pResult)
{
    // Works for both ANSI and Unicode tooltip notifications
    TOOLTIPTEXTA* pTTA = reinterpret_cast<TOOLTIPTEXTA*>(pNMHDR);
    TOOLTIPTEXTW* pTTW = reinterpret_cast<TOOLTIPTEXTW*>(pNMHDR);

    UINT_PTR nID = pNMHDR->idFrom;
    if (pNMHDR->code == TTN_NEEDTEXTW)
        pTTW->hinst = nullptr;
    else
        pTTA->hinst = nullptr;

    // Map command IDs to tooltip strings
    struct TipEntry { UINT id; const wchar_t* tip; };
    static const TipEntry kTips[] =
    {
        { IDC_BTN_RUN_STOP,    L"Comprobar / Detener"          },
        { IDC_BTN_CFG_WIZ,     L"Editor de configuraci\xf3n"   },
        { IDC_BTN_RELOAD_CFG,  L"Recargar configuraci\xf3n"    },
        { IDC_BTN_SAVE_CFG,    L"Guardar configuraci\xf3n"     },
        { IDC_BTN_SAVE_HTML,   L"Guardar informe HTML"          },
        { IDC_BTN_RPC_SCAN,    L"Escaneo RPC din\xe1mico"      },
        { IDC_BTN_VIEW_TOGGLE, L"Cambiar vista"                 },
        { IDC_BTN_EXIT,        L"Salir"                         },
        { IDC_BTN_HELP,        L"Ayuda"                         },
        { IDC_BTN_INFO,        L"Acerca de NetPortChk"          },
    };

    for (const auto& e : kTips)
    {
        if (e.id == static_cast<UINT>(nID))
        {
            if (pNMHDR->code == TTN_NEEDTEXTW)
                wcsncpy_s(pTTW->szText, e.tip, _TRUNCATE);
            else
            {
                // Convert to narrow for ANSI notification
                WideCharToMultiByte(CP_ACP, 0, e.tip, -1,
                    pTTA->szText, sizeof(pTTA->szText), nullptr, nullptr);
            }
            *pResult = 0;
            return TRUE;
        }
    }
    return FALSE;
}

// ──────────────────────────────────────────────────────────────────────────────
void CMainFrame::LayoutContent(int /*cx*/, int /*cy*/)
{
    if (!m_listCtrl.GetSafeHwnd()) return;

    CRect rc;
    GetClientRect(&rc);
    RepositionBars(AFX_IDW_CONTROLBAR_FIRST, AFX_IDW_CONTROLBAR_LAST,
                   AFX_IDW_PANE_FIRST, reposQuery, &rc, &rc);

    if (!m_tabMode)
    {
        m_listCtrl.MoveWindow(&rc);
    }
    else
    {
        if (!m_tabCtrl.GetSafeHwnd()) return;
        m_tabCtrl.MoveWindow(&rc);

        // AdjustRect returns body in FRAME client coords (rc origin).
        // Child lists are parented to m_tabCtrl, so subtract m_tabCtrl origin.
        CRect rcBody = rc;
        m_tabCtrl.AdjustRect(FALSE, &rcBody);
        rcBody.OffsetRect(-rc.left, -rc.top);   // → m_tabCtrl client coords
        rcBody.DeflateRect(1, 0);               // 1px side margin, no top gap

        int sel = m_tabCtrl.GetCurSel();
        for (int i = 0; i < (int)m_tabLists.size(); ++i)
        {
            if (!m_tabLists[i]->GetSafeHwnd()) continue;
            m_tabLists[i]->MoveWindow(&rcBody);
            m_tabLists[i]->ShowWindow(i == sel ? SW_SHOW : SW_HIDE);
        }
    }
}

// ──────────────────────────────────────────────────────────────────────────────
// DestroyTabLists – delete all per-server CResultListCtrl instances
// ──────────────────────────────────────────────────────────────────────────────
void CMainFrame::DestroyTabLists()
{
    for (auto* p : m_tabLists)
    {
        if (p && p->GetSafeHwnd()) p->DestroyWindow();
        delete p;
    }
    m_tabLists.clear();
    if (m_tabCtrl.GetSafeHwnd()) m_tabCtrl.DeleteAllItems();
}

// ──────────────────────────────────────────────────────────────────────────────
// PopulateTabView – build one tab + one CResultListCtrl per destination
// ──────────────────────────────────────────────────────────────────────────────
void CMainFrame::PopulateTabView()
{
    DestroyTabLists();
    if (m_results.empty()) return;

    CRect rcBody;
    m_tabCtrl.GetClientRect(&rcBody);
    m_tabCtrl.AdjustRect(FALSE, &rcBody);
    // rcBody is already in m_tabCtrl client coords (GetClientRect origin = 0,0)
    rcBody.DeflateRect(1, 0);

    for (int i = 0; i < (int)m_results.size(); ++i)
    {
        const auto& dr = m_results[i];

        // Add tab
        TCITEM ti{};
        ti.mask = TCIF_TEXT;
        CString lbl(dr.config.name.c_str());
        ti.pszText = lbl.GetBuffer();
        m_tabCtrl.InsertItem(i, &ti);
        lbl.ReleaseBuffer();

        // Create per-server list
        auto* pList = new CResultListCtrl();
        pList->Create(
            WS_CHILD | LVS_REPORT | LVS_SHOWSELALWAYS,
            rcBody, &m_tabCtrl, 2000 + i);
        pList->Initialise();

        // Wire callbacks (same logic as m_listCtrl but with correct destIdx=i)
        int destIdx = i;
        pList->SetCheckToggleCb([this, destIdx](int /*di*/, int pi, bool enabled)
        {
            if (destIdx < (int)m_cfg.destinations.size() &&
                pi < (int)m_cfg.destinations[destIdx].ports.size())
            {
                m_cfg.destinations[destIdx].ports[pi].enabled = enabled;
                if (destIdx < (int)m_results.size() &&
                    pi < (int)m_results[destIdx].portResults.size())
                    m_results[destIdx].portResults[pi].enabled = enabled;
                m_cfgDirty = true;
                SyncPortEnabled(destIdx, pi);
            }
        });
        pList->SetBatchToggleCb([this, destIdx](int /*di*/, Protocol const* proto, bool enable)
        {
            ApplyBatchToggle(destIdx, proto, enable);
        });

        // Populate with single-server results
        std::vector<DestinationResult> single = { dr };
        pList->PopulateResults(single, m_sourceIP);

        pList->ShowWindow(i == 0 ? SW_SHOW : SW_HIDE);
        m_tabLists.push_back(pList);
    }
    if (!m_tabLists.empty()) m_tabCtrl.SetCurSel(0);
}

// ──────────────────────────────────────────────────────────────────────────────
// PopulateCurrentView – refresh whichever view is active
// ──────────────────────────────────────────────────────────────────────────────
void CMainFrame::PopulateCurrentView()
{
    if (!m_tabMode)
    {
        m_listCtrl.PopulateResults(m_results, m_sourceIP);
    }
    else
    {
        PopulateTabView();
        CRect rc; GetClientRect(&rc);
        LayoutContent(rc.Width(), rc.Height());
    }
}

// ──────────────────────────────────────────────────────────────────────────────
// UpdateViewToggleBtn – swap image to reflect current mode
// ──────────────────────────────────────────────────────────────────────────────
void CMainFrame::UpdateViewToggleBtn()
{
    TBBUTTONINFO tbi{};
    tbi.cbSize = sizeof(tbi);
    tbi.dwMask = TBIF_IMAGE | TBIF_TEXT;
    if (!m_tabMode)
    {
        // currently in list mode → button offers to switch TO tabs
        tbi.iImage  = IMG_VIEW_TABS;
        tbi.pszText = const_cast<LPWSTR>(L"Vista por pesta\xf1""as");
    }
    else
    {
        // currently in tab mode → button offers to switch TO list
        tbi.iImage  = IMG_VIEW_LIST;
        tbi.pszText = const_cast<LPWSTR>(L"Vista en lista");
    }
    m_toolbar.GetToolBarCtrl().SendMessage(
        TB_SETBUTTONINFOW, IDC_BTN_VIEW_TOGGLE,
        reinterpret_cast<LPARAM>(&tbi));
    m_toolbar.Invalidate();
}

// ──────────────────────────────────────────────────────────────────────────────
// ApplyViewMode – show/hide controls according to m_tabMode
// ──────────────────────────────────────────────────────────────────────────────
void CMainFrame::ApplyViewMode()
{
    if (!m_tabMode)
    {
        // Switch to list
        m_tabCtrl.ShowWindow(SW_HIDE);
        for (auto* p : m_tabLists) if (p->GetSafeHwnd()) p->ShowWindow(SW_HIDE);
        m_listCtrl.ShowWindow(SW_SHOW);
    }
    else
    {
        // Switch to tabs
        m_listCtrl.ShowWindow(SW_HIDE);
        PopulateTabView();
        m_tabCtrl.ShowWindow(SW_SHOW);
    }

    UpdateViewToggleBtn();

    CRect rc; GetClientRect(&rc);
    LayoutContent(rc.Width(), rc.Height());
}

// ──────────────────────────────────────────────────────────────────────────────
// OnViewToggle – toolbar button handler
// ──────────────────────────────────────────────────────────────────────────────
void CMainFrame::OnViewToggle()
{
    m_tabMode = !m_tabMode;
    ApplyViewMode();
}

// ──────────────────────────────────────────────────────────────────────────────
// OnTabSelChange – show the list for the selected tab
// ──────────────────────────────────────────────────────────────────────────────
void CMainFrame::OnTabSelChange(NMHDR* /*pNMHDR*/, LRESULT* pResult)
{
    int sel = m_tabCtrl.GetCurSel();
    for (int i = 0; i < (int)m_tabLists.size(); ++i)
        m_tabLists[i]->ShowWindow(i == sel ? SW_SHOW : SW_HIDE);
    *pResult = 0;
}

BOOL CMainFrame::OnCmdMsg(UINT nID, int nCode, void* pExtra,
                           AFX_CMDHANDLERINFO* pHandlerInfo)
{
    return CFrameWnd::OnCmdMsg(nID, nCode, pExtra, pHandlerInfo);
}

// ──────────────────────────────────────────────────────────────────────────────
// Helpers
// ──────────────────────────────────────────────────────────────────────────────
void CMainFrame::SyncToolbarRunStop(bool running)
{
    // Only swap the image index – changing text triggers TB_AUTOSIZE which
    // alters button spacing.  Tooltip text comes from the iString set in
    // BuildToolbar and does not need updating here.
    TBBUTTONINFO tbi {};
    tbi.cbSize  = sizeof(tbi);
    tbi.dwMask  = TBIF_IMAGE;
    tbi.iImage  = running ? IMG_STOP : IMG_RUN;

    m_toolbar.GetToolBarCtrl().SendMessage(
        TB_SETBUTTONINFOW, IDC_BTN_RUN_STOP,
        reinterpret_cast<LPARAM>(&tbi));
    m_toolbar.Invalidate();
}

void CMainFrame::SetStatus(const wchar_t* text)
{
    m_statusBar.SetPaneText(0, text);
}

void CMainFrame::SetSourceIPPane(const std::wstring& ip)
{
    std::wstring txt = ip.empty() ? L"Origen: --" : L"Origen: " + ip;
    m_statusBar.SetPaneText(2, txt.c_str());
}

void CMainFrame::SetTimeoutPane()
{
    CString txt;
    txt.Format(L"Timeout: %d ms", m_timeoutMs);
    m_statusBar.SetPaneText(3, txt);
}


void CMainFrame::SetProgress(int cur, int total)
{
    if (total <= 0) { m_progress.ShowWindow(SW_HIDE); return; }
    CRect rc;
    m_statusBar.GetItemRect(1, &rc);  // already in status bar client coords
    m_progress.MoveWindow(rc);
    m_progress.ShowWindow(SW_SHOW);
    m_progress.SetRange(0, total);
    m_progress.SetPos(cur);
}

int CMainFrame::TotalPortCount() const
{
    int n = 0;
    for (const auto& dr : m_results) n += static_cast<int>(dr.portResults.size());
    return n;
}

int CMainFrame::CompletedPortCount() const
{
    int n = 0;
    for (const auto& dr : m_results)
        for (const auto& pr : dr.portResults)
            if (pr.status != ConnectStatus::PENDING) ++n;
    return n;
}

// ──────────────────────────────────────────────────────────────────────────────
// SyncPortEnabled – update checkbox visual in BOTH views for one port
// ──────────────────────────────────────────────────────────────────────────────
void CMainFrame::SyncPortEnabled(int di, int pi)
{
    if (di >= (int)m_results.size() ||
        pi >= (int)m_results[di].portResults.size()) return;
    const PortResult& pr = m_results[di].portResults[pi];
    m_listCtrl.UpdateResult(di, pi, pr);
    if (di < (int)m_tabLists.size() && m_tabLists[di]->GetSafeHwnd())
        m_tabLists[di]->UpdateResult(0, pi, pr);
}

// ──────────────────────────────────────────────────────────────────────────────
// ApplyBatchToggle – propagate context-menu batch operation into m_cfg/m_results
// destIdx == -1  → all destinations   proto == nullptr → all protocols
// ──────────────────────────────────────────────────────────────────────────────
void CMainFrame::ApplyBatchToggle(int destIdx, Protocol const* proto, bool enable)
{
    for (int di = 0; di < static_cast<int>(m_cfg.destinations.size()); ++di)
    {
        if (destIdx >= 0 && di != destIdx) continue;
        for (int pi = 0; pi < static_cast<int>(m_cfg.destinations[di].ports.size()); ++pi)
        {
            if (proto && m_cfg.destinations[di].ports[pi].protocol != *proto) continue;
            m_cfg.destinations[di].ports[pi].enabled = enable;
            if (di < static_cast<int>(m_results.size()) &&
                pi < static_cast<int>(m_results[di].portResults.size()))
                m_results[di].portResults[pi].enabled = enable;
        }
    }
    m_cfgDirty = true;
    for (int di2 = 0; di2 < (int)m_results.size(); ++di2)
        if (destIdx < 0 || di2 == destIdx)
            for (int pi2 = 0; pi2 < (int)m_results[di2].portResults.size(); ++pi2)
                SyncPortEnabled(di2, pi2);
}
// ──────────────────────────────────────────────────────────────────────────────
