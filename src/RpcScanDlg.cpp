#include "pch.h"
#include "RpcScanDlg.h"
#include <codecvt>
#include <locale>

IMPLEMENT_DYNAMIC(CRpcScanDlg, CDialogEx)

BEGIN_MESSAGE_MAP(CRpcScanDlg, CDialogEx)
    ON_BN_CLICKED(IDC_RPC_BTN_SCAN,   &CRpcScanDlg::OnBtnScan)
    ON_BN_CLICKED(IDC_RPC_BTN_EXPORT, &CRpcScanDlg::OnBtnExport)
    ON_MESSAGE(WM_RPC_RESULT,   &CRpcScanDlg::OnScanResult)
    ON_MESSAGE(WM_RPC_PROGRESS, &CRpcScanDlg::OnScanProgress)
    ON_MESSAGE(WM_RPC_COMPLETE, &CRpcScanDlg::OnScanComplete)
END_MESSAGE_MAP()

CRpcScanDlg::CRpcScanDlg(const std::wstring& defaultIP,
                           const std::wstring& bindIP,
                           CWnd* pParent)
    : CDialogEx(IDD_RPC_SCAN, pParent)
    , m_defaultIP(defaultIP)
    , m_bindIP(bindIP)
{}

void CRpcScanDlg::DoDataExchange(CDataExchange* pDX)
{
    CDialogEx::DoDataExchange(pDX);
    DDX_Control(pDX, IDC_RPC_IP,         m_edIP);
    DDX_Control(pDX, IDC_RPC_PORT_FROM,  m_edFrom);
    DDX_Control(pDX, IDC_RPC_PORT_TO,    m_edTo);
    DDX_Control(pDX, IDC_RPC_THREADS,    m_edThreads);
    DDX_Control(pDX, IDC_RPC_TIMEOUT,    m_cbTimeout);
    DDX_Control(pDX, IDC_RPC_PROTO,      m_cbProto);
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
    m_list.InsertColumn(0, L"Puerto",        LVCFMT_RIGHT,  70);
    m_list.InsertColumn(1, L"Proto",         LVCFMT_CENTER, 52);
    m_list.InsertColumn(2, L"Estado",        LVCFMT_LEFT,   80);
    m_list.InsertColumn(3, L"Latencia (ms)", LVCFMT_RIGHT,  80);

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

    m_cbProto.AddString(L"TCP");
    m_cbProto.AddString(L"UDP");
    m_cbProto.AddString(L"TCP + UDP");
    m_cbProto.SetCurSel(0);   // TCP por defecto (uso m\xE1s habitual; UDP requiere listener)

    m_progress.SetRange32(0, 1);
    m_progress.SetPos(0);
    m_lblStatus.SetWindowText(L"Listo.");
    m_btnExport.EnableWindow(FALSE);

    // Botón Escanear como botón por defecto visual
    SendMessage(DM_SETDEFID, IDC_RPC_BTN_SCAN, 0);

    return TRUE;
}

void CRpcScanDlg::OnCancel()
{
    // Detener el scanner si está activo antes de cerrar el diálogo
    if (m_scanner.IsRunning())
        m_scanner.Stop();

    // Drenar resultados pendientes para liberar los ScanResultPacket*
    // que quedaran posteados antes de Stop().
    HWND hWnd = GetSafeHwnd();
    if (hWnd)
    {
        MSG msg;
        while (::PeekMessage(&msg, hWnd, WM_RPC_RESULT, WM_RPC_RESULT, PM_REMOVE))
            delete reinterpret_cast<ScanResultPacket*>(msg.lParam);
    }

    CDialogEx::OnCancel();   // EndDialog(IDCANCEL) → vuelve a DoModal() en MainFrame
}

BOOL CRpcScanDlg::PreTranslateMessage(MSG* pMsg)
{
    if (pMsg->message == WM_KEYDOWN)
    {
        if (pMsg->wParam == VK_ESCAPE)  { OnCancel(); return TRUE; }
        if (pMsg->wParam == VK_RETURN)  { OnBtnScan(); return TRUE; }
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

    int pIdx = m_cbProto.GetCurSel();
    RpcScanner::ScanProto proto =
        (pIdx == 1) ? RpcScanner::ScanProto::UDP :
        (pIdx == 2) ? RpcScanner::ScanProto::Both :
                      RpcScanner::ScanProto::TCP;

    static const wchar_t* kProtoNames[] = { L"TCP", L"UDP", L"TCP+UDP" };
    const wchar_t* protoName = kProtoNames[(std::max)(0, (std::min)(pIdx, 2))];

    // Reset
    m_list.DeleteAllItems();
    m_total = portTo - portFrom + 1;
    if (proto == RpcScanner::ScanProto::Both) m_total *= 2;
    m_progress.SetRange32(0, m_total);
    m_progress.SetPos(0);
    m_btnExport.EnableWindow(FALSE);
    SetScanningState(true);

    CString initMsg;
    initMsg.Format(L"Escaneando %s  puertos %d \x2013 %d  [%s]  (%d hilos, %d ms)...",
                   (LPCWSTR)ip, portFrom, portTo, protoName, threads, timeout);
    m_lblStatus.SetWindowText(initMsg);

    m_scanner.SetTimeout(timeout);
    m_scanner.SetConcurrency(threads);
    m_scanner.SetProto(proto);

    // Pasar NIC seleccionada al scanner
    if (!m_bindIP.empty())
    {
        std::string bindA;
        int n = WideCharToMultiByte(CP_UTF8, 0, m_bindIP.c_str(), -1,
                                    nullptr, 0, nullptr, nullptr);
        if (n > 1)
        {
            bindA.resize(n - 1);
            WideCharToMultiByte(CP_UTF8, 0, m_bindIP.c_str(), -1,
                                bindA.data(), n, nullptr, nullptr);
        }
        m_scanner.SetBindIP(bindA);
    }
    else
    {
        m_scanner.SetBindIP({});
    }

    HWND hWnd = GetSafeHwnd();
    std::wstring ipW(ip);

    m_scanner.StartAsync(
        ipW, portFrom, portTo,
        [hWnd](int port, ConnectStatus status, DWORD latMs, RpcScanner::ScanProto proto)
        {
            // Pasar todos los datos como struct heap; el handler libera.
            // Esto evita desbordes en LPARAM x86 con latencias grandes.
            auto* pkt = new ScanResultPacket{ port, status, latMs, proto };
            ::PostMessage(hWnd, WM_RPC_RESULT, 0,
                          reinterpret_cast<LPARAM>(pkt));
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
LRESULT CRpcScanDlg::OnScanResult(WPARAM /*wParam*/, LPARAM lParam)
{
    auto* pkt = reinterpret_cast<ScanResultPacket*>(lParam);
    if (!pkt) return 0;
    bool isUdp = (pkt->proto == RpcScanner::ScanProto::UDP);
    AddRow(pkt->port, pkt->latMs, isUdp);
    delete pkt;   // liberar struct posteado desde el worker
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

    // Ordenar por puerto al completar el escaneo. Antes ordenábamos en
    // cada inserción → O(n²); ahora el coste es O(n log n) una sola vez.
    m_list.SortItems(
        [](LPARAM a, LPARAM b, LPARAM /*ctx*/) -> int {
            int pa = static_cast<int>(a);
            int pb = static_cast<int>(b);
            return (pa < pb) ? -1 : (pa > pb ? 1 : 0);
        },
        0);

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
void CRpcScanDlg::AddRow(int port, DWORD latMs, bool isUdp)
{
    // Append O(1). El orden por puerto se aplica al completar el escaneo
    // en OnScanComplete (SortItems). Antes recorríamos la lista entera
    // por cada inserción → O(n²) con escaneos masivos.
    int idx = m_list.InsertItem(m_list.GetItemCount(),
                                std::to_wstring(port).c_str());
    m_list.SetItemText(idx, 1, isUdp ? L"UDP" : L"TCP");
    m_list.SetItemText(idx, 2, L"ABIERTO");
    m_list.SetItemText(idx, 3, std::to_wstring(latMs).c_str());
    // Guardar el puerto en ItemData para ordenar al final sin reparsear texto.
    m_list.SetItemData(idx, static_cast<DWORD_PTR>(port));
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
    m_cbProto.EnableWindow(!scanning);
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

    std::wofstream f(dlg.GetPathName().GetString(), std::ios::binary);
    if (!f.is_open())
    { MessageBox(L"No se pudo crear el archivo.", L"Error", MB_ICONERROR); return; }

    // UTF-8 + BOM para que Excel reconozca acentos correctamente.
#pragma warning(push)
#pragma warning(disable: 4996)
    f.imbue(std::locale(f.getloc(), new std::codecvt_utf8<wchar_t>));
#pragma warning(pop)
    f << L"\uFEFF";

    BYTE b0=0,b1=0,b2=0,b3=0;
    m_edIP.GetAddress(b0, b1, b2, b3);
    CString ip;
    ip.Format(L"%d.%d.%d.%d", b0, b1, b2, b3);
    f << L"IP,Puerto,Proto,Estado,Latencia (ms)\n";
    int n = m_list.GetItemCount();
    for (int i = 0; i < n; ++i)
    {
        f << ip.GetString() << L","
          << m_list.GetItemText(i, 0).GetString() << L","   // Puerto
          << m_list.GetItemText(i, 1).GetString() << L","   // Proto
          << m_list.GetItemText(i, 2).GetString() << L","   // Estado
          << m_list.GetItemText(i, 3).GetString() << L"\n"; // Latencia
    }

    if (!f.good())
    { MessageBox(L"Error al escribir el archivo.", L"Error", MB_ICONERROR); return; }

    m_lblStatus.SetWindowText(L"Archivo CSV exportado correctamente.");
}
