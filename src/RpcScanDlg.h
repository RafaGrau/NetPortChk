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
    CListCtrl    m_list;
    CButton      m_btnScan;
    CButton      m_btnExport;
    CProgressCtrl m_progress;
    CStatic      m_lblStatus;

    // Scanner
    RpcScanner   m_scanner;

    // Results cache (sorted by port)
    struct ScanRow { int port; DWORD latMs; };
    std::vector<ScanRow> m_rows;
    std::mutex           m_rowsMtx;

    std::wstring m_defaultIP;
    int          m_total { 0 };

    void SetScanningState(bool scanning);
    void AddRow(int port, DWORD latMs);
    void UpdateStatus(int scanned, int total);
    void ExportToCsv();
};

// Custom window messages (posted from scanner callbacks)
#define WM_RPC_RESULT   (WM_USER + 200)   // wParam=port, lParam=latMs<<32|status
#define WM_RPC_PROGRESS (WM_USER + 201)   // wParam=scanned, lParam=total
#define WM_RPC_COMPLETE (WM_USER + 202)
