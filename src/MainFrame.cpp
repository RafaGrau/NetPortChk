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
    ON_COMMAND(IDC_BTN_VIEW_TOGGLE, &CMainFrame::OnViewToggle)
    ON_COMMAND(IDC_BTN_CFG_WIZ,     &CMainFrame::OnCfgWiz)
    ON_COMMAND(IDC_BTN_HELP,        &CMainFrame::OnHelp)
    ON_COMMAND(IDC_BTN_INFO,        &CMainFrame::OnInfo)
    ON_COMMAND(IDC_BTN_EXIT,        &CMainFrame::OnFileExit)
    ON_COMMAND(IDC_BTN_RPC_SCAN,    &CMainFrame::OnRpcScan)
    ON_COMMAND(IDC_BTN_LISTEN_TOGGLE, &CMainFrame::OnListenToggle)
    ON_COMMAND(IDC_BTN_NIC_SELECT,    &CMainFrame::OnNicSelect)
    ON_COMMAND(IDC_BTN_BANNER_PROBE,  &CMainFrame::OnBannerProbe)

    // Update-UI handlers
    ON_UPDATE_COMMAND_UI(IDC_BTN_RUN_STOP,      &CMainFrame::OnUpdateRunStop)
    ON_UPDATE_COMMAND_UI(IDC_BTN_SAVE_HTML,     &CMainFrame::OnUpdateSaveHtml)
    ON_UPDATE_COMMAND_UI(IDC_BTN_SAVE_CFG,      &CMainFrame::OnUpdateSaveCfg)
    ON_UPDATE_COMMAND_UI(IDC_BTN_RELOAD_CFG,    &CMainFrame::OnUpdateReloadCfg)
    ON_UPDATE_COMMAND_UI(IDC_BTN_BANNER_PROBE,  &CMainFrame::OnUpdateBannerProbe)

    ON_MESSAGE(WM_NC_RESULT,   &CMainFrame::OnNcResult)
    ON_MESSAGE(WM_NC_COMPLETE, &CMainFrame::OnNcComplete)
    ON_MESSAGE(WM_LISTEN_PKT,  &CMainFrame::OnListenPkt)
END_MESSAGE_MAP()

// ──────────────────────────────────────────────────────────────────────────────
CMainFrame::CMainFrame()  = default;
CMainFrame::~CMainFrame() = default;

BOOL CMainFrame::PreCreateWindow(CREATESTRUCT& cs)
{
    if (!CFrameWnd::PreCreateWindow(cs)) return FALSE;
    // Remove the menu bar slot from the window style
    cs.style = WS_OVERLAPPEDWINDOW | FWS_ADDTOTITLE;
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
    // Pane 1: progress bar (120 px)
    // Pane 2: IP origen   (150 px)
    // Pane 3: timeout     (120 px)
    // Pane 4: NIC activa  (160 px)
    static UINT indicators[] = { ID_SEPARATOR, ID_SEPARATOR, ID_SEPARATOR, ID_SEPARATOR, ID_SEPARATOR };
    m_statusBar.Create(this);
    m_statusBar.SetIndicators(indicators, 5);
    m_statusBar.SetPaneInfo(0, ID_SEPARATOR, SBPS_STRETCH, 0);
    m_statusBar.SetPaneInfo(1, ID_SEPARATOR, SBPS_NORMAL,  120);
    m_statusBar.SetPaneInfo(2, ID_SEPARATOR, SBPS_NORMAL,  150);
    m_statusBar.SetPaneInfo(3, ID_SEPARATOR, SBPS_NORMAL,  120);
    m_statusBar.SetPaneInfo(4, ID_SEPARATOR, SBPS_NORMAL,  160);

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
    int newH = (std::max)(rcSb.Height(), 32);  // at least 32px tall
    m_statusBar.SetWindowPos(nullptr, 0, 0, rcSb.Width(), newH,
                             SWP_NOMOVE | SWP_NOZORDER | SWP_NOACTIVATE);

    // ── Toolbar ───────────────────────────────────────────────────────────────
    BuildToolbar();

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

    // ── Listen log (hidden by default – client mode) ──────────────────────────
    m_listenCtrl.Create(
        WS_CHILD | LVS_REPORT | LVS_SHOWSELALWAYS | LVS_NOSORTHEADER,
        rcClient, this, AFX_IDW_PANE_FIRST + 2);
    InitListenCtrl();
    m_listenCtrl.ShowWindow(SW_HIDE);

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
    AutoSelectNic();     // selección automática de NIC por defecto
    UpdateNicPane();
    SetSourceIPPane(m_sourceIP);

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

    // Required before TB_ADDBUTTONS
    m_toolbar.GetToolBarCtrl().SetButtonStructSize(sizeof(TBBUTTON));

    static TBBUTTON tbb[] =
    {
        // 1. Comprobar / Detener (interruptor visual por icono).
        {IMG_RUN,      IDC_BTN_RUN_STOP,   TBSTATE_ENABLED, TBSTYLE_BUTTON, {}, 0, (INT_PTR)L"Comprobar / Detener"},
        // 2. Cambiar vista
        {IMG_VIEW_TABS,IDC_BTN_VIEW_TOGGLE,TBSTATE_ENABLED, TBSTYLE_BUTTON, {}, 0, (INT_PTR)L"Cambiar vista"},
        // 3. Guardar informe HTML
        {IMG_HTML,     IDC_BTN_SAVE_HTML,  TBSTATE_ENABLED, TBSTYLE_BUTTON, {}, 0, (INT_PTR)L"Guardar informe HTML"},
        // Separador
        {0,            0,                  TBSTATE_ENABLED, TBSTYLE_SEP,    {}, 0, 0},
        // 4. Escaneo RPC dinámico
        {IMG_RPC,      IDC_BTN_RPC_SCAN,      TBSTATE_ENABLED, TBSTYLE_BUTTON, {}, 0, (INT_PTR)L"Escaneo RPC din\xe1mico"},
        // 5. Modo escucha UDP.
        {IMG_SERVER,   IDC_BTN_LISTEN_TOGGLE, TBSTATE_ENABLED, TBSTYLE_BUTTON, {}, 0, (INT_PTR)L"Activar modo escucha UDP"},
        // 6. Selección de tarjeta de red (sin separador)
        {IMG_NETCARD,  IDC_BTN_NIC_SELECT,    TBSTATE_ENABLED, TBSTYLE_BUTTON, {}, 0, (INT_PTR)L"Seleccionar tarjeta de red"},
        // 7. Activar / Desactivar Sondeo de banner TCP (interruptor visual por icono).
        //    OFF (default): icono coms_tx-rx → clic activa TX/RX (envía sondeo)
        //    ON:           icono coms_tx     → clic vuelve a solo handshake
        {IMG_COMS_TX_RX, IDC_BTN_BANNER_PROBE, TBSTATE_ENABLED, TBSTYLE_BUTTON, {}, 0, (INT_PTR)L"Activar sondeo TCP (Tx 1 byte)"},
        // Separador
        {0,            0,                  TBSTATE_ENABLED, TBSTYLE_SEP,    {}, 0, 0},
        // 7. Editor de configuración
        {IMG_CFGEDIT,  IDC_BTN_CFG_WIZ,    TBSTATE_ENABLED, TBSTYLE_BUTTON, {}, 0, (INT_PTR)L"Editor de configuraci\xf3n"},
        // 8. Guardar configuración
        {IMG_SAVE,     IDC_BTN_SAVE_CFG,   TBSTATE_ENABLED, TBSTYLE_BUTTON, {}, 0, (INT_PTR)L"Guardar configuraci\xf3n"},
        // 9. Recargar configuración
        {IMG_RELOAD,   IDC_BTN_RELOAD_CFG, TBSTATE_ENABLED, TBSTYLE_BUTTON, {}, 0, (INT_PTR)L"Recargar configuraci\xf3n"},
        // Separador
        {0,            0,                  TBSTATE_ENABLED, TBSTYLE_SEP,    {}, 0, 0},
        // 10. Salir
        {IMG_EXIT,     IDC_BTN_EXIT,       TBSTATE_ENABLED, TBSTYLE_BUTTON, {}, 0, (INT_PTR)L"Salir"},
        // Separador elástico – empuja Ayuda e Info al extremo derecho
        {0,            IDC_BTN_INFO_SEP,   TBSTATE_ENABLED, TBSTYLE_SEP,    {}, 0, 0},
        // 11. Ayuda
        {IMG_HELP,     IDC_BTN_HELP,       TBSTATE_ENABLED, TBSTYLE_BUTTON, {}, 0, (INT_PTR)L"Ayuda"},
        // 12. Acerca de
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
// Robust against GDI allocation failures: returns nullptr (with no leaks)
// if any of CreateDIBSection / CreateCompatibleDC / GetDC fail.
// ──────────────────────────────────────────────────────────────────────────────
static HBITMAP IconToBitmap(HICON hIcon, int sz, void** pBitsOut = nullptr)
{
    if (pBitsOut) *pBitsOut = nullptr;
    if (!hIcon || sz <= 0) return nullptr;

    BITMAPINFOHEADER bih {};
    bih.biSize        = sizeof(bih);
    bih.biWidth       = sz;
    bih.biHeight      = -sz;   // top-down
    bih.biPlanes      = 1;
    bih.biBitCount    = 32;
    bih.biCompression = BI_RGB;

    HDC hScreen = ::GetDC(nullptr);
    if (!hScreen) return nullptr;

    void* pBits = nullptr;
    HBITMAP hBmp = ::CreateDIBSection(hScreen,
                                       reinterpret_cast<BITMAPINFO*>(&bih),
                                       DIB_RGB_COLORS, &pBits, nullptr, 0);
    HDC hDC = ::CreateCompatibleDC(hScreen);
    ::ReleaseDC(nullptr, hScreen);

    // Limpieza ordenada si cualquier paso falla — evita crash por NULL deref.
    if (!hBmp || !hDC || !pBits)
    {
        if (hBmp) ::DeleteObject(hBmp);
        if (hDC)  ::DeleteDC(hDC);
        return nullptr;
    }

    HBITMAP hOld = static_cast<HBITMAP>(::SelectObject(hDC, hBmp));
    memset(pBits, 0, static_cast<size_t>(sz) * sz * 4);            // transparent black
    ::DrawIconEx(hDC, 0, 0, hIcon, sz, sz, 0, nullptr, DI_NORMAL); // writes pre-mult ARGB
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
    if (!hSrc || !pSrcBits)
    {
        if (hSrc) ::DeleteObject(hSrc);
        return nullptr;
    }

    // Allocate destination DIB
    BITMAPINFOHEADER bih {};
    bih.biSize        = sizeof(bih);
    bih.biWidth       = sz;
    bih.biHeight      = -sz;
    bih.biPlanes      = 1;
    bih.biBitCount    = 32;
    bih.biCompression = BI_RGB;

    HDC hScreen = ::GetDC(nullptr);
    if (!hScreen)
    {
        ::DeleteObject(hSrc);
        return nullptr;
    }

    void* pDstBits = nullptr;
    HBITMAP hDst = ::CreateDIBSection(hScreen,
                                       reinterpret_cast<BITMAPINFO*>(&bih),
                                       DIB_RGB_COLORS, &pDstBits, nullptr, 0);
    ::ReleaseDC(nullptr, hScreen);

    if (!hDst || !pDstBits)
    {
        ::DeleteObject(hSrc);
        if (hDst) ::DeleteObject(hDst);
        return nullptr;
    }

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
        IDI_ICON_SERVER,    // IMG_SERVER server.ico
        IDI_ICON_CLIENT,    // IMG_CLIENT win_client.ico
        IDI_ICON_NETCARD,    // IMG_NETCARD network_card.ico
        IDI_ICON_COMS_TX_RX, // IMG_COMS_TX_RX coms_tx-rx.ico
        IDI_ICON_COMS_TX,    // IMG_COMS_TX    coms_tx.ico
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

    // Inicializar contadores incrementales — evita recorrer m_results
    // entero en cada WM_NC_RESULT (era O(N²) durante una ejecución completa).
    m_totalPorts = TotalPortCount();
    m_donePorts  = 0;
    SetProgress(0, m_totalPorts);
    SetStatus(L"Comprobando conectividad…");
    SyncToolbarRunStop(true);
    m_hasResults = false;

    HWND hWnd = GetSafeHwnd();
    m_checker.SetTimeout(m_timeoutMs);
    m_checker.SetBindIP(m_bindIP);
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
            m_tabLists[di]->UpdateResult(0, pi, pr);
        }
        // Contadores incrementales: O(1) en lugar de O(N) por
        // CompletedPortCount/TotalPortCount.
        ++m_donePorts;
        SetProgress(m_donePorts, m_totalPorts);
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

    // NOTA: NO se elimina el ADS Zone.Identifier (Mark-of-the-Web).
    // Borrar MOTW silenciosamente es un patrón asociado al malware y
    // dispara alertas en EDR/Smart App Control. Si el CHM proviene de
    // Internet, el usuario verá el aviso estándar de Windows: éste es
    // el comportamiento correcto y esperado.
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
    std::wstring ip;

    if (!m_results.empty())
    {
        int di = -1;

        if (!m_tabMode)
        {
            // Modo lista: obtener destIdx real desde m_rowMap via GetItemDestIdx
            POSITION pos = m_listCtrl.GetFirstSelectedItemPosition();
            if (pos)
            {
                int item = m_listCtrl.GetNextSelectedItem(pos);
                di = m_listCtrl.GetItemDestIdx(item);
            }
        }
        else
        {
            // Modo pestaña: el servidor activo es la pestaña seleccionada
            di = m_tabCtrl.GetCurSel();
        }

        if (di >= 0 && di < static_cast<int>(m_results.size()))
            ip = m_results[di].config.ip;
        else
            ip = m_results[0].config.ip;   // fallback: primer servidor
    }

    CRpcScanDlg dlg(ip, m_bindIP, this);
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

// ──────────────────────────────────────────────────────────────────────────────
// WM_CLOSE – ask before exit if dirty
// ──────────────────────────────────────────────────────────────────────────────
void CMainFrame::OnClose()
{
    if (m_checker.IsRunning()) m_checker.Stop();
    if (m_listener.IsRunning()) m_listener.Stop();

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
        // 2 px de margen final para evitar que el último botón quede al borde
        int stretch = (std::max<int>)(8, rcTb.Width() - rcSep.left - rightWidth - 2);
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
    m_listener.Stop();   // join() del worker — tras esto no hay nuevos PostMessage

    // Drenar manualmente los mensajes pendientes en la cola para liberar
    // los punteros heap (ListenPacket* y std::wstring*) que ya estaban
    // posteados antes de Stop(). Si no se drenan aquí, se pierden cuando
    // Windows destruye la cola junto con el HWND.
    HWND hWnd = GetSafeHwnd();
    if (hWnd)
    {
        MSG msg;
        while (::PeekMessage(&msg, hWnd, WM_LISTEN_PKT, WM_LISTEN_PKT, PM_REMOVE))
        {
            if (msg.wParam == 1)
                delete reinterpret_cast<std::wstring*>(msg.lParam);
            else
                delete reinterpret_cast<ListenPacket*>(msg.lParam);
        }
    }

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

    // Tooltip dinámico del botón conmutador de sondeo. El texto describe
    // la *acción* que ocurrirá al pulsar (consistente con el cambio de icono).
    //   OFF (icono coms_tx-rx) → "Activar sondeo TCP (envía 1 byte tras handshake)"
    //   ON  (icono coms_tx)    → "Desactivar sondeo TCP (sólo handshake)"
    if (static_cast<UINT>(nID) == IDC_BTN_BANNER_PROBE)
    {
        const wchar_t* tip = m_bannerProbe
            ? L"Desactivar sondeo TCP (s\xf3lo handshake)"
            : L"Activar sondeo TCP (env\xeda 1 byte tras handshake)";
        if (pNMHDR->code == TTN_NEEDTEXTW)
            wcsncpy_s(pTTW->szText, tip, _TRUNCATE);
        else
            WideCharToMultiByte(CP_ACP, 0, tip, -1,
                pTTA->szText, sizeof(pTTA->szText), nullptr, nullptr);
        *pResult = 0;
        return TRUE;
    }

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
        { IDC_BTN_LISTEN_TOGGLE, L"Activar / desactivar modo escucha UDP" },
        { IDC_BTN_NIC_SELECT,   L"Seleccionar tarjeta de red"             },
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

    // Modo escucha: ocupa todo el panel
    if (m_listenMode)
    {
        if (m_listenCtrl.GetSafeHwnd())
            m_listenCtrl.MoveWindow(&rc);
        return;
    }

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

        // Populate with single-server results (vector de un elemento)
        pList->PopulateResults({ dr }, m_sourceIP);

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
    // TB_SETBUTTONINFOW copia internamente, pero pszText es LPWSTR (no const).
    // Usar buffers mutables locales evita el const_cast<LPWSTR>(L"...").
    wchar_t lblTabs[] = L"Vista por pesta\xf1""as";
    wchar_t lblList[] = L"Vista en lista";
    if (!m_tabMode)
    {
        // currently in list mode → button offers to switch TO tabs
        tbi.iImage  = IMG_VIEW_TABS;
        tbi.pszText = lblTabs;
    }
    else
    {
        // currently in tab mode → button offers to switch TO list
        tbi.iImage  = IMG_VIEW_LIST;
        tbi.pszText = lblList;
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

// ──────────────────────────────────────────────────────────────────────────────
// InitListenCtrl – columnas del log de escucha UDP
// ──────────────────────────────────────────────────────────────────────────────
void CMainFrame::InitListenCtrl()
{
    m_listenCtrl.SetExtendedStyle(LVS_EX_FULLROWSELECT | LVS_EX_GRIDLINES |
                                  LVS_EX_DOUBLEBUFFER);
    m_listenCtrl.InsertColumn(0, L"Hora",           LVCFMT_LEFT,   72);
    m_listenCtrl.InsertColumn(1, L"IP Remota",      LVCFMT_LEFT,  130);  // cliente que envió
    m_listenCtrl.InsertColumn(2, L"Puerto remoto",  LVCFMT_RIGHT,  90);  // puerto origen cliente
    m_listenCtrl.InsertColumn(3, L"Puerto local",   LVCFMT_RIGHT,  85);  // puerto local escuchado
    m_listenCtrl.InsertColumn(4, L"Bytes",          LVCFMT_RIGHT,  55);
}

// ──────────────────────────────────────────────────────────────────────────────
// CollectUdpPorts – recopila TODOS los puertos UDP conocidos del PortDB
// (todos los tipos de servidor) independientemente de la configuración activa.
// Los puertos privilegiados (<1024) solo se añaden si la app tiene permisos;
// el bind fallará silenciosamente en UdpListener y se registrará en el log.
// ──────────────────────────────────────────────────────────────────────────────
std::vector<int> CMainFrame::CollectUdpPorts() const
{
    // Usar std::set para deduplicar en O(log n) en lugar del bucle lineal
    // anterior por cada inserción (O(n²) en total).
    std::set<int> uniq;

    // Unión de todos los puertos UDP de todos los tipos de servidor del PortDB
    static const DestinationType kAllTypes[] = {
        DestinationType::DC,
        DestinationType::PrintServer,
        DestinationType::SCCM_Full,
        DestinationType::SCCM_DP,
        DestinationType::DNS,
        DestinationType::DHCP,
    };

    for (auto dt : kAllTypes)
        for (const auto& pe : PortDB::GetPorts(dt))
            if (pe.protocol == Protocol::UDP)
                uniq.insert(pe.port);

    // Puertos UDP de la tabla de referencia general (PortDefaultDesc)
    // que no están cubiertos por los tipos anteriores
    static const int kExtraUdp[] = {
        69,   // TFTP
        123,  // NTP
        137,  // NetBIOS Name
        138,  // NetBIOS Datagram
        161,  // SNMP
        162,  // SNMP Trap
        514,  // Syslog
        1434, // SQL Server Browser
    };
    for (int p : kExtraUdp) uniq.insert(p);

    return { uniq.begin(), uniq.end() };
}

// ──────────────────────────────────────────────────────────────────────────────
// DoStartListen / DoStopListen
// ──────────────────────────────────────────────────────────────────────────────
void CMainFrame::DoStartListen()
{
    // Puertos conocidos del PortDB
    auto udpPorts = CollectUdpPorts();

    // Rango RPC dinámico completo (49152-65535)
    for (int p = 49152; p <= 65535; ++p)
        udpPorts.push_back(p);

    HWND hWnd = GetSafeHwnd();

    m_listener.SetBindIP(m_bindIP);

    int n = m_listener.Start(
        udpPorts,
        // onPacket – desde hilo worker: PostMessage para no tocar la UI
        [hWnd](ListenPacket* pkt)
        {
            ::PostMessage(hWnd, WM_LISTEN_PKT, 0, reinterpret_cast<LPARAM>(pkt));
        },
        // onStatus
        [hWnd, this](const std::wstring& msg)
        {
            ::PostMessage(hWnd, WM_LISTEN_PKT, 1,
                          reinterpret_cast<LPARAM>(new std::wstring(msg)));
        }
    );

    if (n == 0) return;   // Start ya mostró mensaje de error

    m_listenMode = true;
    m_listenCtrl.DeleteAllItems();

    // Cambiar visibilidad: ocultar vista normal, mostrar log de escucha
    m_listCtrl.ShowWindow(SW_HIDE);
    m_tabCtrl.ShowWindow(SW_HIDE);
    for (auto* p : m_tabLists) if (p->GetSafeHwnd()) p->ShowWindow(SW_HIDE);
    m_listenCtrl.ShowWindow(SW_SHOW);

    CRect rc; GetClientRect(&rc);
    LayoutContent(rc.Width(), rc.Height());

    SyncListenToggleBtn();
    SyncToolbarRunStop(false);
    // Deshabilitar botón Run mientras escuchamos
    m_toolbar.GetToolBarCtrl().EnableButton(IDC_BTN_RUN_STOP, FALSE);
}

void CMainFrame::DoStopListen()
{
    m_listener.Stop();
    m_listenMode = false;

    // Restaurar vista normal
    m_listenCtrl.ShowWindow(SW_HIDE);
    if (!m_tabMode)
        m_listCtrl.ShowWindow(SW_SHOW);
    else
    {
        m_tabCtrl.ShowWindow(SW_SHOW);
        int sel = m_tabCtrl.GetCurSel();
        for (int i = 0; i < (int)m_tabLists.size(); ++i)
            m_tabLists[i]->ShowWindow(i == sel ? SW_SHOW : SW_HIDE);
    }

    CRect rc; GetClientRect(&rc);
    LayoutContent(rc.Width(), rc.Height());

    SyncListenToggleBtn();
    m_toolbar.GetToolBarCtrl().EnableButton(IDC_BTN_RUN_STOP, TRUE);
    SetStatus(L"Modo escucha desactivado.");
}

// ──────────────────────────────────────────────────────────────────────────────
// SyncListenToggleBtn – actualiza icono y tooltip del botón toggle
// ──────────────────────────────────────────────────────────────────────────────
void CMainFrame::SyncListenToggleBtn()
{
    TBBUTTONINFO tbi{};
    tbi.cbSize  = sizeof(tbi);
    tbi.dwMask  = TBIF_IMAGE;
    tbi.iImage  = m_listenMode ? IMG_CLIENT : IMG_SERVER;
    m_toolbar.GetToolBarCtrl().SendMessage(
        TB_SETBUTTONINFOW, IDC_BTN_LISTEN_TOGGLE,
        reinterpret_cast<LPARAM>(&tbi));
    m_toolbar.Invalidate();
}

// ──────────────────────────────────────────────────────────────────────────────
// OnListenToggle – handler del botón toolbar
// ──────────────────────────────────────────────────────────────────────────────
void CMainFrame::OnListenToggle()
{
    if (m_listenMode) DoStopListen();
    else              DoStartListen();
}

// ──────────────────────────────────────────────────────────────────────────────
// OnListenPkt – WM_LISTEN_PKT
// wParam == 0 → lParam = ListenPacket*   (paquete UDP recibido)
// wParam == 1 → lParam = std::wstring*   (mensaje de estado)
// ──────────────────────────────────────────────────────────────────────────────
LRESULT CMainFrame::OnListenPkt(WPARAM wParam, LPARAM lParam)
{
    if (wParam == 1)
    {
        auto* msg = reinterpret_cast<std::wstring*>(lParam);
        SetStatus(msg->c_str());
        delete msg;
        return 0;
    }

    auto* pkt = reinterpret_cast<ListenPacket*>(lParam);
    if (!pkt) return 0;

    // Formatear hora
    wchar_t ts[24];
    swprintf_s(ts, L"%02d:%02d:%02d.%03d",
               pkt->time.wHour, pkt->time.wMinute,
               pkt->time.wSecond, pkt->time.wMilliseconds);

    int row = m_listenCtrl.GetItemCount();
    m_listenCtrl.InsertItem(row, ts);
    m_listenCtrl.SetItemText(row, 1, pkt->senderIP.c_str());
    m_listenCtrl.SetItemText(row, 2, std::to_wstring(pkt->senderPort).c_str());
    m_listenCtrl.SetItemText(row, 3, std::to_wstring(pkt->port).c_str());
    m_listenCtrl.SetItemText(row, 4, std::to_wstring(pkt->bytes).c_str());

    // Sólo auto-scroll si el usuario ya estaba viendo el final de la lista.
    // Si ha hecho scroll manual hacia arriba, respetamos su posición —
    // antes el EnsureVisible() incondicional rompía el scroll del usuario.
    int topVisible = m_listenCtrl.GetTopIndex();
    int perPage    = m_listenCtrl.GetCountPerPage();
    if (row <= topVisible + perPage)   // estaba viendo el último visible
        m_listenCtrl.EnsureVisible(row, FALSE);

    // Actualizar barra de estado
    CString s;
    s.Format(L"Escucha activa  —  %d paquete(s) recibido(s)", row + 1);
    SetStatus(static_cast<LPCWSTR>(s));

    delete pkt;
    return 0;
}

// ──────────────────────────────────────────────────────────────────────────────
// OnNicSelect – abre el diálogo de selección de tarjeta de red
// ──────────────────────────────────────────────────────────────────────────────
void CMainFrame::OnNicSelect()
{
    CNicSelectDlg dlg(m_bindIP, this);
    if (dlg.DoModal() != IDOK) return;

    m_bindIP   = dlg.m_selectedIP;
    m_bindName = dlg.m_selectedName;
    m_sourceIP = m_bindIP.empty()
                 ? NetworkChecker::GetLocalIP()
                 : m_bindIP;

    SetSourceIPPane(m_sourceIP);
    UpdateNicPane();
}

// ──────────────────────────────────────────────────────────────────────────────
// OnBannerProbe – alterna el sondeo de banner TCP
// Cuando está activo, tras el handshake TCP se envía 1 byte para forzar
// que el servicio responda con su banner/greeting (LDAP, SMTP, RDP…).
// Esto rellena las columnas TX/RX con tráfico real.
// Por defecto desactivado: el sondeo puede aparecer como tráfico
// malformado en logs SIEM/IDS.
// ──────────────────────────────────────────────────────────────────────────────
void CMainFrame::OnBannerProbe()
{
    // Estado de sesión: no se persiste en NetPortChk.config y por tanto
    // no marca m_cfgDirty. Cada arranque empieza con el sondeo desactivado.
    m_bannerProbe = !m_bannerProbe;
    m_checker.SetBannerProbe(m_bannerProbe);
    SyncToolbarBannerProbe();   // refresca el icono al nuevo estado
    SetStatus(m_bannerProbe
        ? L"Sondeo de banner TCP activado (env\xeda 1 byte tras el handshake)."
        : L"Sondeo de banner TCP desactivado.");
}

void CMainFrame::OnUpdateBannerProbe(CCmdUI* pCmdUI)
{
    // Botón siempre habilitado; la pista visual del estado es el icono
    // (gestionado por SyncToolbarBannerProbe), no el aspecto hundido.
    pCmdUI->Enable(TRUE);
}

// ──────────────────────────────────────────────────────────────────────────────
// AutoSelectNic – selecciona la NIC por defecto (Ethernet/LAN o primera activa)
// Rellena m_bindIP, m_bindName, m_sourceIP
// ──────────────────────────────────────────────────────────────────────────────
void CMainFrame::AutoSelectNic()
{
    // Reutilizar la misma lógica que CNicSelectDlg::PopulateList,
    // con el patrón correcto de dos llamadas (GetAdaptersAddresses
    // puede devolver ERROR_BUFFER_OVERFLOW si hay muchos adaptadores
    // virtuales/VPN; un buffer fijo de 64 KiB no siempre basta).
    constexpr ULONG kFlags =
        GAA_FLAG_INCLUDE_PREFIX | GAA_FLAG_SKIP_ANYCAST |
        GAA_FLAG_SKIP_MULTICAST | GAA_FLAG_SKIP_DNS_SERVER;

    ULONG bufLen = 16 * 1024;
    std::vector<BYTE> buf(bufLen);
    DWORD ret = GetAdaptersAddresses(AF_INET, kFlags, nullptr,
                    reinterpret_cast<IP_ADAPTER_ADDRESSES*>(buf.data()),
                    &bufLen);
    if (ret == ERROR_BUFFER_OVERFLOW)
    {
        buf.resize(bufLen);
        ret = GetAdaptersAddresses(AF_INET, kFlags, nullptr,
                  reinterpret_cast<IP_ADAPTER_ADDRESSES*>(buf.data()),
                  &bufLen);
    }
    if (ret != NO_ERROR) return;

    std::wstring firstIP, firstName;
    std::wstring lanIP,   lanName;

    auto* p = reinterpret_cast<IP_ADAPTER_ADDRESSES*>(buf.data());
    for (; p; p = p->Next)
    {
        if (p->OperStatus != IfOperStatusUp) continue;
        if (!p->FirstUnicastAddress)         continue;

        // Obtener primera IPv4 (sin loopback)
        std::wstring ipW;
        for (auto* ua = p->FirstUnicastAddress; ua; ua = ua->Next)
        {
            if (ua->Address.lpSockaddr->sa_family != AF_INET) continue;
            char ipA[INET_ADDRSTRLEN]{};
            inet_ntop(AF_INET,
                      &reinterpret_cast<sockaddr_in*>(ua->Address.lpSockaddr)->sin_addr,
                      ipA, sizeof(ipA));
            if (strcmp(ipA, "127.0.0.1") == 0) break;
            int n = MultiByteToWideChar(CP_UTF8, 0, ipA, -1, nullptr, 0);
            if (n <= 0) break;
            ipW.resize(n); MultiByteToWideChar(CP_UTF8, 0, ipA, -1, ipW.data(), n);
            if (!ipW.empty() && ipW.back() == L'\0') ipW.pop_back();
            break;
        }
        if (ipW.empty()) continue;

        std::wstring name = p->FriendlyName ? p->FriendlyName : L"";
        if (firstIP.empty()) { firstIP = ipW; firstName = name; }

        bool isLAN = name.find(L"Ethernet") != std::wstring::npos
                  || name.find(L"LAN")       != std::wstring::npos
                  || name.find(L"Local")     != std::wstring::npos;
        if (isLAN && lanIP.empty()) { lanIP = ipW; lanName = name; }
    }

    // Preferir LAN, si no la primera activa
    m_bindIP   = lanIP.empty()   ? firstIP   : lanIP;
    m_bindName = lanName.empty() ? firstName : lanName;
    m_sourceIP = m_bindIP;
}

// ──────────────────────────────────────────────────────────────────────────────
// UpdateNicPane – pane 4 de la status bar: NIC activa
// ──────────────────────────────────────────────────────────────────────────────
void CMainFrame::UpdateNicPane()
{
    CString txt;
    txt.Format(L"NIC: %s", m_bindName.c_str());
    m_statusBar.SetPaneText(4, txt);
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

// ──────────────────────────────────────────────────────────────────────────────
// SyncToolbarBannerProbe – cambia el icono del botón de sondeo según el estado
//   OFF (m_bannerProbe == false): icono coms_tx-rx (sólo TX/RX del handshake;
//                                  pulsar lo activará)
//   ON  (m_bannerProbe == true) : icono coms_tx    (envío activo; pulsar
//                                  volverá a sólo handshake)
// El estado del estilo BUTTON (no CHECK) hace que sea un push-button puro: lo
// que el usuario ve siempre es el icono de la acción que se ejecutará al pulsar.
// El estado es de sesión: NO se persiste en config; cada arranque parte de OFF.
// ──────────────────────────────────────────────────────────────────────────────
void CMainFrame::SyncToolbarBannerProbe()
{
    TBBUTTONINFO tbi {};
    tbi.cbSize = sizeof(tbi);
    tbi.dwMask = TBIF_IMAGE;
    tbi.iImage = m_bannerProbe ? IMG_COMS_TX : IMG_COMS_TX_RX;

    m_toolbar.GetToolBarCtrl().SendMessage(
        TB_SETBUTTONINFOW, IDC_BTN_BANNER_PROBE,
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

    // Un único refresco de la vista — antes recorríamos m_results llamando
    // a SyncPortEnabled por cada puerto, que a su vez hacía búsqueda lineal
    // en m_rowMap → coste total O(N²). PopulateCurrentView es O(N).
    PopulateCurrentView();
}
// ──────────────────────────────────────────────────────────────────────────────
