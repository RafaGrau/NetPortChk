#pragma once
#include "resource.h"
#include <string>
#include <vector>

// ──────────────────────────────────────────────────────────────────────────────
// NicInfo – información de un adaptador de red IPv4
// ──────────────────────────────────────────────────────────────────────────────
struct NicInfo
{
    std::wstring friendlyName;
    std::wstring description;
    std::wstring ipAddress;
    std::wstring macAddress;
};

// ──────────────────────────────────────────────────────────────────────────────
// CNicSelectDlg
// Modal dialog: muestra los adaptadores IPv4 activos y deja seleccionar uno.
// Tras IDOK, leer m_selectedIP (vacío = dejar que el SO elija).
// ──────────────────────────────────────────────────────────────────────────────
class CNicSelectDlg : public CDialogEx
{
    DECLARE_DYNAMIC(CNicSelectDlg)
public:
    explicit CNicSelectDlg(const std::wstring& currentIP = L"",
                           CWnd* pParent = nullptr);
    enum { IDD = IDD_NIC_SELECT };

    std::wstring m_selectedIP;     // resultado: IP del adaptador elegido (vacío = auto)
    std::wstring m_selectedName;   // resultado: nombre amigable del adaptador

protected:
    void DoDataExchange(CDataExchange* pDX) override;
    BOOL OnInitDialog() override;
    void OnOK() override;
    afx_msg void OnListDblClick(NMHDR* pNMHDR, LRESULT* pResult);
    DECLARE_MESSAGE_MAP()

private:
    CListCtrl            m_list;
    std::vector<NicInfo> m_nics;
    std::wstring         m_currentIP;

    void EnumerateNics();
    void PopulateList();
};
