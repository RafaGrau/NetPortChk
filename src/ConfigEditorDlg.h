#pragma once
#include "AppTypes.h"
#include "resource.h"

// ──────────────────────────────────────────────────────────────────────────────
// CPortEditorDlg
// Modal dialog for adding or editing a single PortEntry.
// Set m_portNum/m_protoSel/m_desc before DoModal() when editing;
// read them back after IDOK.
// ──────────────────────────────────────────────────────────────────────────────
class CPortEditorDlg : public CDialogEx
{
    DECLARE_DYNAMIC(CPortEditorDlg)
public:
    explicit CPortEditorDlg(bool isNew, CWnd* pParent = nullptr);
    enum { IDD = IDD_PORT_EDITOR };

    int     m_portNum  { 0 };   // port 1-65535
    int     m_protoSel { 0 };   // 0=TCP, 1=UDP
    CString m_desc;

protected:
    void DoDataExchange(CDataExchange* pDX) override;
    BOOL OnInitDialog() override;
    void OnOK() override;
    afx_msg void OnPortKillFocus();
    DECLARE_MESSAGE_MAP()

private:
    bool      m_isNew;
    CEdit     m_edPort;
    CComboBox m_cbProto;
    CEdit     m_edDesc;
};

// ──────────────────────────────────────────────────────────────────────────────
// CConfigEditorDlg
// Configuration editor: servers (tab per server) + port list per server.
// Toolbar replaces all old push-buttons.  Port add/edit use CPortEditorDlg.
// ──────────────────────────────────────────────────────────────────────────────
class CConfigEditorDlg : public CDialogEx
{
    DECLARE_DYNAMIC(CConfigEditorDlg)
public:
    explicit CConfigEditorDlg(AppConfig& cfg, CWnd* pParent = nullptr);
    enum { IDD = IDD_CONFIG_EDITOR };

protected:
    void DoDataExchange(CDataExchange* pDX) override;
    BOOL PreTranslateMessage(MSG* pMsg) override;
    BOOL OnInitDialog() override;
    void OnOK() override;

    // Server operations (invoked from toolbar)
    afx_msg void OnBtnNewServer();
    afx_msg void OnBtnAddServer();
    afx_msg void OnTbDelServer();
    afx_msg void OnBtnImportCsv();
    afx_msg void OnBtnExportCsv();

    // Port list operations
    afx_msg void OnBtnDelPort();
    afx_msg void OnTbPortAdd();
    afx_msg void OnTbPortEdit();

    // Notifications
    afx_msg void OnTabRClick    (NMHDR* pNMHDR, LRESULT* pResult);
    afx_msg void OnListRClick   (NMHDR* pNMHDR, LRESULT* pResult);
    afx_msg void OnFormChanged  ();
    afx_msg void OnIpChanged    (NMHDR* pNMHDR, LRESULT* pResult);
    afx_msg void OnTabSelChange (NMHDR* pNMHDR, LRESULT* pResult);
    afx_msg void OnListItemChange (NMHDR* pNMHDR, LRESULT* pResult);
    afx_msg void OnListColumnClick(NMHDR* pNMHDR, LRESULT* pResult);
    afx_msg void OnTbGetInfoTip (NMHDR* pNMHDR, LRESULT* pResult);
    afx_msg LRESULT OnDpiChanged(WPARAM wParam, LPARAM lParam);

    DECLARE_MESSAGE_MAP()

private:
    AppConfig&                     m_cfg;
    std::vector<DestinationConfig> m_servers;
    int                            m_curSrv      { -1 };
    bool                           m_dirty       { false };
    bool                           m_inhibitDirty{ false };

    // Controls
    CEdit        m_edName;
    CWnd         m_edIP;
    CComboBox    m_cbType;
    CTabCtrl     m_tab;
    CListCtrl    m_list;
    CComboBox    m_cbTimeout;

    // Toolbar
    CToolBarCtrl m_toolbar;
    CImageList   m_tbImgList;

    // Sort state
    int  m_sortCol{ -1 };
    bool m_sortAsc{ true };

    CToolTipCtrl m_tooltip;

    // Helpers
    void CreateToolbar();
    void RepositionFormRow();
    void RebuildToolbarImages(int iconPx);  // rebuild image list at given icon size
    void DoLayout();                         // full layout pass (toolbar + form + list)
    int  DpiScale(int logical) const;        // scale logical pixels by window DPI
    bool EditPortDlg(PortEntry& pe, bool isNew);
    void RefreshTabs();
    void SwitchToServer(int idx);
    void PopulateList(int srvIdx);
    void UpdateButtonStates();
    void PositionList();
    std::wstring GetIP() const;
    void         SetIP(const std::wstring& ip);
};
