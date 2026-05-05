#include "pch.h"
#include "NicSelectDlg.h"
#include <iphlpapi.h>

IMPLEMENT_DYNAMIC(CNicSelectDlg, CDialogEx)

BEGIN_MESSAGE_MAP(CNicSelectDlg, CDialogEx)
    ON_NOTIFY(NM_DBLCLK, IDC_NIC_LIST, &CNicSelectDlg::OnListDblClick)
END_MESSAGE_MAP()

CNicSelectDlg::CNicSelectDlg(const std::wstring& currentIP, CWnd* pParent)
    : CDialogEx(IDD_NIC_SELECT, pParent)
    , m_currentIP(currentIP)
{}

void CNicSelectDlg::DoDataExchange(CDataExchange* pDX)
{
    CDialogEx::DoDataExchange(pDX);
    DDX_Control(pDX, IDC_NIC_LIST, m_list);
}

// ──────────────────────────────────────────────────────────────────────────────
// EnumerateNics – usa GetAdaptersAddresses (requiere iphlpapi)
// ──────────────────────────────────────────────────────────────────────────────
void CNicSelectDlg::EnumerateNics()
{
    m_nics.clear();

    // Obtener adaptadores con GetAdaptersAddresses
    ULONG bufLen = 64 * 1024;
    std::vector<BYTE> buf(bufLen);

    DWORD ret = GetAdaptersAddresses(
        AF_INET,
        GAA_FLAG_INCLUDE_PREFIX | GAA_FLAG_SKIP_ANYCAST |
        GAA_FLAG_SKIP_MULTICAST | GAA_FLAG_SKIP_DNS_SERVER,
        nullptr,
        reinterpret_cast<IP_ADAPTER_ADDRESSES*>(buf.data()),
        &bufLen);

    if (ret == ERROR_BUFFER_OVERFLOW)
    {
        buf.resize(bufLen);
        ret = GetAdaptersAddresses(
            AF_INET,
            GAA_FLAG_INCLUDE_PREFIX | GAA_FLAG_SKIP_ANYCAST |
            GAA_FLAG_SKIP_MULTICAST | GAA_FLAG_SKIP_DNS_SERVER,
            nullptr,
            reinterpret_cast<IP_ADAPTER_ADDRESSES*>(buf.data()),
            &bufLen);
    }

    if (ret != NO_ERROR) return;

    auto* pAdap = reinterpret_cast<IP_ADAPTER_ADDRESSES*>(buf.data());
    for (; pAdap; pAdap = pAdap->Next)
    {
        // Solo adaptadores activos (Up) con al menos una dirección IPv4
        if (pAdap->OperStatus != IfOperStatusUp) continue;
        if (!pAdap->FirstUnicastAddress)         continue;

        // Buscar la primera dirección IPv4
        std::wstring ipAddr;
        for (auto* ua = pAdap->FirstUnicastAddress; ua; ua = ua->Next)
        {
            if (ua->Address.lpSockaddr->sa_family != AF_INET) continue;
            char ipA[INET_ADDRSTRLEN]{};
            inet_ntop(AF_INET,
                      &reinterpret_cast<sockaddr_in*>(ua->Address.lpSockaddr)->sin_addr,
                      ipA, sizeof(ipA));
            int n = MultiByteToWideChar(CP_UTF8, 0, ipA, -1, nullptr, 0);
            if (n <= 0) break;
            ipAddr.resize(n);
            MultiByteToWideChar(CP_UTF8, 0, ipA, -1, ipAddr.data(), n);
            if (!ipAddr.empty() && ipAddr.back() == L'\0') ipAddr.pop_back();
            break;
        }
        if (ipAddr.empty()) continue;

        // Omitir loopback
        if (ipAddr == L"127.0.0.1") continue;

        // MAC
        wchar_t macBuf[32]{};
        if (pAdap->PhysicalAddressLength == 6)
        {
            swprintf_s(macBuf, L"%02X-%02X-%02X-%02X-%02X-%02X",
                       pAdap->PhysicalAddress[0], pAdap->PhysicalAddress[1],
                       pAdap->PhysicalAddress[2], pAdap->PhysicalAddress[3],
                       pAdap->PhysicalAddress[4], pAdap->PhysicalAddress[5]);
        }
        else
        {
            wcscpy_s(macBuf, L"—");
        }

        NicInfo ni{};
        ni.friendlyName = pAdap->FriendlyName  ? pAdap->FriendlyName  : L"";
        ni.description  = pAdap->Description   ? pAdap->Description   : L"";
        ni.ipAddress    = ipAddr;
        ni.macAddress   = macBuf;
        m_nics.push_back(std::move(ni));
    }
}

// ──────────────────────────────────────────────────────────────────────────────
// PopulateList
// ──────────────────────────────────────────────────────────────────────────────
void CNicSelectDlg::PopulateList()
{
    m_list.DeleteAllItems();

    for (int i = 0; i < static_cast<int>(m_nics.size()); ++i)
    {
        const auto& ni = m_nics[i];
        m_list.InsertItem(i, ni.friendlyName.c_str());
        m_list.SetItemText(i, 1, ni.ipAddress.c_str());
        m_list.SetItemText(i, 2, ni.macAddress.c_str());
        m_list.SetItemText(i, 3, ni.description.c_str());
    }

    // ── Selección por defecto ─────────────────────────────────────────────────
    // Prioridad: 1) IP que ya estaba configurada, 2) LAN (Ethernet), 3) primera
    int defaultIdx = -1;

    // 1. Mantener la selección actual si coincide con alguna NIC
    if (!m_currentIP.empty())
        for (int i = 0; i < static_cast<int>(m_nics.size()); ++i)
            if (m_nics[i].ipAddress == m_currentIP) { defaultIdx = i; break; }

    // 2. Si solo hay una NIC, seleccionarla directamente
    if (defaultIdx < 0 && m_nics.size() == 1)
        defaultIdx = 0;

    // 3. Buscar la primera NIC de tipo Ethernet/LAN
    if (defaultIdx < 0)
    {
        for (int i = 0; i < static_cast<int>(m_nics.size()); ++i)
        {
            const std::wstring& name = m_nics[i].friendlyName;
            const std::wstring& desc = m_nics[i].description;
            bool isEthernet = (name.find(L"Ethernet") != std::wstring::npos)
                           || (name.find(L"LAN")      != std::wstring::npos)
                           || (name.find(L"Local")    != std::wstring::npos)
                           || (desc.find(L"Ethernet") != std::wstring::npos);
            if (isEthernet) { defaultIdx = i; break; }
        }
    }

    // 4. Fallback: primera NIC disponible
    if (defaultIdx < 0 && !m_nics.empty())
        defaultIdx = 0;

    if (defaultIdx >= 0)
    {
        m_list.SetItemState(defaultIdx, LVIS_SELECTED | LVIS_FOCUSED,
                                        LVIS_SELECTED | LVIS_FOCUSED);
        m_list.EnsureVisible(defaultIdx, FALSE);
    }
}

// ──────────────────────────────────────────────────────────────────────────────
// OnInitDialog
// ──────────────────────────────────────────────────────────────────────────────
BOOL CNicSelectDlg::OnInitDialog()
{
    CDialogEx::OnInitDialog();

    m_list.SetExtendedStyle(LVS_EX_FULLROWSELECT | LVS_EX_GRIDLINES |
                            LVS_EX_DOUBLEBUFFER);
    m_list.InsertColumn(0, L"Adaptador",     LVCFMT_LEFT,  130);
    m_list.InsertColumn(1, L"Dirección IP",  LVCFMT_LEFT,  110);
    m_list.InsertColumn(2, L"MAC",           LVCFMT_LEFT,  130);
    m_list.InsertColumn(3, L"Descripción",   LVCFMT_LEFT,  200);

    EnumerateNics();
    PopulateList();

    return TRUE;
}

// ──────────────────────────────────────────────────────────────────────────────
// OnOK
// ──────────────────────────────────────────────────────────────────────────────
void CNicSelectDlg::OnOK()
{
    int sel = m_list.GetNextItem(-1, LVNI_SELECTED);
    if (sel >= 0 && sel < static_cast<int>(m_nics.size()))
    {
        m_selectedIP   = m_nics[sel].ipAddress;
        m_selectedName = m_nics[sel].friendlyName;
    }
    else if (!m_nics.empty())
    {
        m_selectedIP   = m_nics[0].ipAddress;
        m_selectedName = m_nics[0].friendlyName;
    }
    CDialogEx::OnOK();
}

// ──────────────────────────────────────────────────────────────────────────────
// OnListDblClick – doble clic confirma la selección
// ──────────────────────────────────────────────────────────────────────────────
void CNicSelectDlg::OnListDblClick(NMHDR* /*pNMHDR*/, LRESULT* pResult)
{
    OnOK();
    *pResult = 0;
}
