#include "pch.h"
#include <fstream>
#include <algorithm>
#include "RpcScanDlg.h"

IMPLEMENT_DYNAMIC(CRpcScanDlg, CDialogEx)

BEGIN_MESSAGE_MAP(CRpcScanDlg, CDialogEx)
    ON_BN_CLICKED(IDC_RPC_BTN_SCAN,   &CRpcScanDlg::OnBtnScan)
    ON_BN_CLICKED(IDC_RPC_BTN_EXPORT, &CRpcScanDlg::OnBtnExport)
    ON_MESSAGE(WM_RPC_RESULT,   &CRpcScanDlg::OnScanResult)
    ON_MESSAGE(WM_RPC_PROGRESS, &CRpcScanDlg::OnScanProgress)
    ON_MESSAGE(WM_RPC_COMPLETE, &CRpcScanDlg::OnScanComplete)
END_MESSAGE_MAP()

CRpcScanDlg::CRpcScanDlg(const std::wstring& defaultIP, CWnd* pParent)
    : CDialogEx(IDD_RPC_SCAN, pParent)
    , m_defaultIP(defaultIP)
{}

void CRpcScanDlg::DoDataExchange(CDataExchange* pDX)
{
    CDialogEx::DoDataExchange(pDX);
    DDX_Control(pDX, IDC_RPC_IP,         m_edIP);
    DDX_Control(pDX, IDC_RPC_PORT_FROM,  m_edFrom);
    DDX_Control(pDX, IDC_RPC_PORT_TO,    m_edTo);
    DDX_Control(pDX, IDC_RPC_THREADS,    m_edThreads);
    DDX_Control(pDX, IDC_RPC_TIMEOUT,    m_cbTimeout);
    DDX_Control(pDX, IDC_RPC_LIST,       m_list);
    DDX_Control(pDX, IDC_RPC_BTN_SCAN,   m_btnScan);
    DDX_Control(pDX, IDC_RPC_BTN_EXPORT, m_btnExport);
    DDX_Control(pDX, IDC_RPC_PROGRESS,   m_progress);
    DDX_Control(pDX, IDC_RPC_STATUS,     m_lblStatus);
}

BOOL CRpcScanDlg::OnInitDialog()
{
    CDialogEx::OnInitDialog();

    // ── List columns ──────────────────────────────────────────────────────────
    m_list.SetExtendedStyle(LVS_EX_FULLROWSELECT | LVS_EX_GRIDLINES);
    m_list.InsertColumn(0, L"Puerto",        LVCFMT_RIGHT,  80);
    m_list.InsertColumn(1, L"Estado",        LVCFMT_LEFT,  100);
    m_list.InsertColumn(2, L"Latencia (ms)", LVCFMT_RIGHT,  90);

    // ── Defaults ──────────────────────────────────────────────────────────────
    // Set IP control — parse dotted string to 4 bytes
    if (!m_defaultIP.empty())
    {
        BYTE b0=0,b1=0,b2=0,b3=0;
        if (swscanf_s(m_defaultIP.c_str(), L"%hhu.%hhu.%hhu.%hhu", &b0,&b1,&b2,&b3) == 4)
            m_edIP.SetAddress(b0, b1, b2, b3);
    }
    m_edFrom.SetWindowText(L"49152");
    m_edTo.SetWindowText(L"65535");
    m_edThreads.SetWindowText(L"200");

    m_cbTimeout.AddString(L"200 ms");
    m_cbTimeout.AddString(L"500 ms");
    m_cbTimeout.AddString(L"1000 ms");
    m_cbTimeout.SetCurSel(1);

    m_progress.SetRange32(0, 1);
    m_progress.SetPos(0);
    m_lblStatus.SetWindowText(L"Listo.");
    m_btnExport.EnableWindow(FALSE);

    return TRUE;
}

void CRpcScanDlg::OnCancel()
{
    if (m_scanner.IsRunning())
    {
        m_scanner.Stop();
        SetScanningState(false);
        m_lblStatus.SetWindowText(L"Escaneo detenido.");
        return;
    }
    CDialogEx::OnCancel();
}

BOOL CRpcScanDlg::PreTranslateMessage(MSG* pMsg)
{
    if (pMsg->message == WM_KEYDOWN && pMsg->wParam == VK_ESCAPE)
    {
        OnCancel();
        return TRUE;
    }
    return CDialogEx::PreTranslateMessage(pMsg);
}

// ──────────────────────────────────────────────────────────────────────────────
// OnBtnScan
// ──────────────────────────────────────────────────────────────────────────────
void CRpcScanDlg::OnBtnScan()
{
    if (m_scanner.IsRunning())
    {
        m_scanner.Stop();
        SetScanningState(false);
        m_lblStatus.SetWindowText(L"Escaneo detenido por el usuario.");
        return;
    }

    // Read IP from IP control
    BYTE b0=0,b1=0,b2=0,b3=0;
    if (m_edIP.IsBlank())
    {
        MessageBox(L"Introduzca una direcci\xf3n IP.", L"Campo requerido", MB_ICONWARNING);
        m_edIP.SetFocus(); return;
    }
    m_edIP.GetAddress(b0, b1, b2, b3);
    CString ip;
    ip.Format(L"%d.%d.%d.%d", b0, b1, b2, b3);

    CString sFrom, sTo, sThreads;
    m_edFrom.GetWindowText(sFrom);
    m_edTo.GetWindowText(sTo);
    m_edThreads.GetWindowText(sThreads);

    int portFrom = _wtoi(sFrom);
    int portTo   = _wtoi(sTo);
    int threads  = _wtoi(sThreads);

    if (portFrom < 1 || portFrom > 65535 || portTo < portFrom || portTo > 65535)
    {
        MessageBox(L"Rango de puertos inv\xe1lido.", L"Error", MB_ICONWARNING);
        return;
    }
    if (threads < 1 || threads > 500)
    {
        MessageBox(L"N\xfamero de hilos inv\xe1lido (1\x2013""500).", L"Error", MB_ICONWARNING);
        return;
    }

    static const int kTimeouts[] = { 200, 500, 1000 };
    int tIdx    = m_cbTimeout.GetCurSel();
    int timeout = (tIdx >= 0 && tIdx < 3) ? kTimeouts[tIdx] : 500;

    // Reset
    m_list.DeleteAllItems();
    {   std::lock_guard<std::mutex> lk(m_rowsMtx); m_rows.clear(); }
    m_total = portTo - portFrom + 1;
    m_progress.SetRange32(0, m_total);
    m_progress.SetPos(0);
    m_btnExport.EnableWindow(FALSE);
    SetScanningState(true);

    CString initMsg;
    initMsg.Format(L"Escaneando %s  puertos %d \x2013 %d  (%d hilos, %d ms timeout)...",
                   (LPCWSTR)ip, portFrom, portTo, threads, timeout);
    m_lblStatus.SetWindowText(initMsg);

    m_scanner.SetTimeout(timeout);
    m_scanner.SetConcurrency(threads);

    HWND hWnd = GetSafeHwnd();
    std::wstring ipW(ip);

    m_scanner.StartAsync(
        ipW, portFrom, portTo,
        [hWnd](int port, ConnectStatus status, DWORD latMs)
        {
            ::PostMessage(hWnd, WM_RPC_RESULT,
                          static_cast<WPARAM>(port),
                          static_cast<LPARAM>((latMs << 16) | static_cast<int>(status)));
        },
        [hWnd](int scanned, int total)
        {
            ::PostMessage(hWnd, WM_RPC_PROGRESS,
                          static_cast<WPARAM>(scanned),
                          static_cast<LPARAM>(total));
        },
        [hWnd]()
        {
            ::PostMessage(hWnd, WM_RPC_COMPLETE, 0, 0);
        }
    );
}

// ──────────────────────────────────────────────────────────────────────────────
// Message handlers
// ──────────────────────────────────────────────────────────────────────────────
LRESULT CRpcScanDlg::OnScanResult(WPARAM wParam, LPARAM lParam)
{
    int   port  = static_cast<int>(wParam);
    DWORD latMs = static_cast<DWORD>(lParam >> 16);
    AddRow(port, latMs);
    return 0;
}

LRESULT CRpcScanDlg::OnScanProgress(WPARAM wParam, LPARAM lParam)
{
    int scanned = static_cast<int>(wParam);
    int total   = static_cast<int>(lParam);
    m_progress.SetPos(scanned);
    UpdateStatus(scanned, total);
    return 0;
}

LRESULT CRpcScanDlg::OnScanComplete(WPARAM, LPARAM)
{
    SetScanningState(false);
    m_progress.SetPos(m_total);

    int open = m_list.GetItemCount();
    CString msg;
    msg.Format(L"Completado. %d puerto%s abierto%s de %d comprobados.",
               open, open == 1 ? L"" : L"s",
               open == 1 ? L"" : L"s", m_total);
    m_lblStatus.SetWindowText(msg);

    m_btnExport.EnableWindow(open > 0 ? TRUE : FALSE);
    return 0;
}

// ──────────────────────────────────────────────────────────────────────────────
// Helpers
// ──────────────────────────────────────────────────────────────────────────────
void CRpcScanDlg::AddRow(int port, DWORD latMs)
{
    {   std::lock_guard<std::mutex> lk(m_rowsMtx);
        m_rows.push_back({ port, latMs }); }

    // Insert sorted
    int n   = m_list.GetItemCount();
    int pos = n;
    for (int i = 0; i < n; ++i)
    {
        if (port < _wtoi(m_list.GetItemText(i, 0))) { pos = i; break; }
    }

    int idx = m_list.InsertItem(pos, std::to_wstring(port).c_str());
    m_list.SetItemText(idx, 1, L"ABIERTO");
    m_list.SetItemText(idx, 2, std::to_wstring(latMs).c_str());
}

void CRpcScanDlg::UpdateStatus(int scanned, int total)
{
    int open = m_list.GetItemCount();
    CString s;
    s.Format(L"Comprobando... %d / %d  \x2014  Puertos abiertos: %d",
             scanned, total, open);
    m_lblStatus.SetWindowText(s);
}

void CRpcScanDlg::SetScanningState(bool scanning)
{
    m_btnScan.SetWindowText(scanning ? L"Detener" : L"Escanear");
    m_edIP.EnableWindow(!scanning);
    m_edFrom.EnableWindow(!scanning);
    m_edTo.EnableWindow(!scanning);
    m_edThreads.EnableWindow(!scanning);
    m_cbTimeout.EnableWindow(!scanning);
}

// ──────────────────────────────────────────────────────────────────────────────
// OnBtnExport
// ──────────────────────────────────────────────────────────────────────────────
void CRpcScanDlg::OnBtnExport()
{
    CFileDialog dlg(FALSE, L"csv", L"RpcScan.csv",
        OFN_OVERWRITEPROMPT,
        L"Archivos CSV (*.csv)|*.csv|Todos (*.*)|*.*||", this);
    if (dlg.DoModal() != IDOK) return;

    std::wofstream f(dlg.GetPathName().GetString());
    if (!f.is_open())
    { MessageBox(L"No se pudo crear el archivo.", L"Error", MB_ICONERROR); return; }

    BYTE b0=0,b1=0,b2=0,b3=0;
    m_edIP.GetAddress(b0, b1, b2, b3);
    CString ip;
    ip.Format(L"%d.%d.%d.%d", b0, b1, b2, b3);
    f << L"IP,Puerto,Estado,Latencia (ms)\n";
    int n = m_list.GetItemCount();
    for (int i = 0; i < n; ++i)
    {
        f << ip.GetString() << L","
          << m_list.GetItemText(i, 0).GetString() << L","
          << m_list.GetItemText(i, 1).GetString() << L","
          << m_list.GetItemText(i, 2).GetString() << L"\n";
    }

    if (!f.good())
    { MessageBox(L"Error al escribir el archivo.", L"Error", MB_ICONERROR); return; }

    m_lblStatus.SetWindowText(L"Archivo CSV exportado correctamente.");
}
