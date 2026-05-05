#pragma once
#include "resource.h"

class CAboutDlg : public CDialogEx
{
    DECLARE_DYNAMIC(CAboutDlg)
public:
    CAboutDlg(CWnd* pParent = nullptr);
    ~CAboutDlg() override = default;
    enum { IDD = IDD_ABOUT };

protected:
    BOOL OnInitDialog() override;
    // Liberamos el icono en PostNcDestroy: el destructor se ejecuta antes
    // de que el control STATIC libere su referencia, lo que podría provocar
    // que dibujara con un HICON ya destruido.
    void PostNcDestroy() override;
    afx_msg void OnLinkGitHub(NMHDR* pNMHDR, LRESULT* pResult);
    afx_msg void OnLinkIcons8(NMHDR* pNMHDR, LRESULT* pResult);
    DECLARE_MESSAGE_MAP()
private:
    HICON m_hIcon64 { nullptr };
};
