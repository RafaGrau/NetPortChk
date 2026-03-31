#include "pch.h"
#include "NetPortChk.h"
#include "MainFrame.h"

// ──────────────────────────────────────────────────────────────────────────────
// Single global application object – MFC entry point
// ──────────────────────────────────────────────────────────────────────────────
CNetPortChkApp theApp;

BEGIN_MESSAGE_MAP(CNetPortChkApp, CWinApp)
END_MESSAGE_MAP()

CNetPortChkApp::CNetPortChkApp()
{
    // Enable Visual Styles / Common Controls v6 via manifest
    m_dwRestartManagerSupportFlags = AFX_RESTART_MANAGER_SUPPORT_RESTART;
}

// ──────────────────────────────────────────────────────────────────────────────
BOOL CNetPortChkApp::InitInstance()
{
    // ── Common Controls (ComCtl32 v6) ─────────────────────────────────────────
    INITCOMMONCONTROLSEX icc { sizeof(icc),
        ICC_WIN95_CLASSES | ICC_LISTVIEW_CLASSES | ICC_PROGRESS_CLASS |
        ICC_BAR_CLASSES   | ICC_TAB_CLASSES };
    InitCommonControlsEx(&icc);

    CWinApp::InitInstance();

    // ── Winsock 2.2 ───────────────────────────────────────────────────────────
    WSADATA wsa {};
    if (WSAStartup(MAKEWORD(2, 2), &wsa) == 0)
        m_wsaInit = true;
    else
    {
        AfxMessageBox(L"Error al inicializar Winsock2. La aplicación no puede continuar.",
                      MB_ICONERROR);
        return FALSE;
    }

    // ── Enable 3D controls ────────────────────────────────────────────────────
    EnableShellOpen();

    // ── Create main frame ─────────────────────────────────────────────────────
    auto* pFrame = new CMainFrame();
    m_pMainWnd   = pFrame;

    if (!pFrame->LoadFrame(IDR_MAINFRAME,
                           WS_OVERLAPPEDWINDOW | FWS_ADDTOTITLE,
                           nullptr, nullptr))
    {
        delete pFrame;
        return FALSE;
    }

    // No menu bar – toolbar-only UI
    pFrame->SetMenu(nullptr);

    pFrame->ShowWindow(SW_SHOW);
    pFrame->UpdateWindow();
    return TRUE;
}

// ──────────────────────────────────────────────────────────────────────────────
int CNetPortChkApp::ExitInstance()
{
    if (m_wsaInit) WSACleanup();
    return CWinApp::ExitInstance();
}
