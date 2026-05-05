#pragma once
#include "RpcScanner.h"
#include "resource.h"
#include <vector>

// ──────────────────────────────────────────────────────────────────────────────
// CRpcScanDlg
// Modeless-capable dialog for RPC range scanning.
// Used as modal from CMainFrame::OnRpcScan().
// ──────────────────────────────────────────────────────────────────────────────
class CRpcScanDlg : public CDialogEx
{
    DECLARE_DYNAMIC(CRpcScanDlg)
public:
    explicit CRpcScanDlg(const std::wstring& defaultIP = L"",
                          const std::wstring& bindIP    = L"",
                          CWnd* pParent = nullptr);
    enum { IDD = IDD_RPC_SCAN };

protected:
    void DoDataExchange(CDataExchange* pDX) override;
    BOOL OnInitDialog() override;
    void OnOK() override {}         // prevent Enter closing dialog
    void OnCancel() override;
    BOOL PreTranslateMessage(MSG* pMsg) override;

    afx_msg void OnBtnScan();
    afx_msg void OnBtnExport();
    afx_msg LRESULT OnScanResult  (WPARAM wParam, LPARAM lParam);
    afx_msg LRESULT OnScanProgress(WPARAM wParam, LPARAM lParam);
    afx_msg LRESULT OnScanComplete(WPARAM wParam, LPARAM lParam);

    DECLARE_MESSAGE_MAP()

private:
    // Controls
    CIPAddressCtrl m_edIP;
    CEdit        m_edFrom;
    CEdit        m_edTo;
    CEdit        m_edThreads;
    CComboBox    m_cbTimeout;
    CComboBox    m_cbProto;
    CListCtrl    m_list;
    CButton      m_btnScan;
    CButton      m_btnExport;
    CProgressCtrl m_progress;
    CStatic      m_lblStatus;

    // Scanner
    RpcScanner   m_scanner;

    // Estructura para pasar el resultado completo vía PostMessage.
    // Reemplaza el bit-packing en LPARAM, que desbordaba para latencias
    // > 65535 ms en builds x86 (LPARAM = 32 bits → latMs<<16 perdía bits).
    struct ScanResultPacket {
        int           port;
        ConnectStatus status;
        DWORD         latMs;
        RpcScanner::ScanProto proto;
    };

    std::wstring m_defaultIP;
    std::wstring m_bindIP;
    int          m_total { 0 };

    void SetScanningState(bool scanning);
    void AddRow(int port, DWORD latMs, bool isUdp = false);
    void UpdateStatus(int scanned, int total);
    void ExportToCsv();
};

// Mensajes WM_RPC_* declarados en AppTypes.h (rango WM_USER+200..299).
