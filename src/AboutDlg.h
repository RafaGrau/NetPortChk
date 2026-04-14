#pragma once
#include "resource.h"

class CAboutDlg : public CDialogEx
{
    DECLARE_DYNAMIC(CAboutDlg)
public:
    CAboutDlg(CWnd* pParent = nullptr);
    ~CAboutDlg() override { if (m_hIcon64) DestroyIcon(m_hIcon64); }
    enum { IDD = IDD_ABOUT };

protected:
    BOOL OnInitDialog() override;
    afx_msg void OnLinkGitHub(NMHDR* pNMHDR, LRESULT* pResult);
    afx_msg void OnLinkIcons8(NMHDR* pNMHDR, LRESULT* pResult);
    DECLARE_MESSAGE_MAP()
private:
    HICON m_hIcon64 { nullptr };
};
