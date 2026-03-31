#include "pch.h"
#include "AboutDlg.h"
#include "version.h"
#include <shellapi.h>

IMPLEMENT_DYNAMIC(CAboutDlg, CDialogEx)

BEGIN_MESSAGE_MAP(CAboutDlg, CDialogEx)
    ON_NOTIFY(NM_CLICK,  IDC_ABOUT_LINK_GITHUB, &CAboutDlg::OnLinkGitHub)
    ON_NOTIFY(NM_RETURN, IDC_ABOUT_LINK_GITHUB, &CAboutDlg::OnLinkGitHub)
    ON_NOTIFY(NM_CLICK,  IDC_ABOUT_LINK_ICONS8, &CAboutDlg::OnLinkIcons8)
    ON_NOTIFY(NM_RETURN, IDC_ABOUT_LINK_ICONS8, &CAboutDlg::OnLinkIcons8)
END_MESSAGE_MAP()

CAboutDlg::CAboutDlg(CWnd* pParent)
    : CDialogEx(IDD_ABOUT, pParent)
{}

BOOL CAboutDlg::OnInitDialog()
{
    CDialogEx::OnInitDialog();

    // Version string built from version.h macros
    CString verStr;
    verStr.Format(L"Versi\xf3n %d.%d.%d.%d",
                  VER_MAJOR, VER_MINOR, VER_PATCH, VER_BUILD);
    SetDlgItemText(IDC_ABOUT_VERSION, verStr);

    // SysLink text must be set at runtime — the RC compiler rejects angle brackets
    SetDlgItemText(IDC_ABOUT_LINK_GITHUB,
        L"<a href=\"https://github.com/RafaGrau/NetPortChk\">"
        L"https://github.com/RafaGrau/NetPortChk</a>");

    SetDlgItemText(IDC_ABOUT_LINK_ICONS8,
        L"<a href=\"https://icons8.com/icons\">Icons8</a>"
        L"  \x2013  Fluency Style");

    return TRUE;
}

void CAboutDlg::OnLinkGitHub(NMHDR* pNMHDR, LRESULT* pResult)
{
    auto* pLink = reinterpret_cast<NMLINK*>(pNMHDR);
    ShellExecuteW(nullptr, L"open", pLink->item.szUrl, nullptr, nullptr, SW_SHOWNORMAL);
    *pResult = 0;
}

void CAboutDlg::OnLinkIcons8(NMHDR* pNMHDR, LRESULT* pResult)
{
    auto* pLink = reinterpret_cast<NMLINK*>(pNMHDR);
    ShellExecuteW(nullptr, L"open", pLink->item.szUrl, nullptr, nullptr, SW_SHOWNORMAL);
    *pResult = 0;
}
