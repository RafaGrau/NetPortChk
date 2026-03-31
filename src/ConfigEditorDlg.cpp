#include "pch.h"
#include <algorithm>
#include <fstream>
#include "ConfigEditorDlg.h"

// ============================================================================
// CPortEditorDlg
// ============================================================================
IMPLEMENT_DYNAMIC(CPortEditorDlg, CDialogEx)

BEGIN_MESSAGE_MAP(CPortEditorDlg, CDialogEx)
    ON_EN_KILLFOCUS(IDC_PORT_ED_PORT, &CPortEditorDlg::OnPortKillFocus)
END_MESSAGE_MAP()

CPortEditorDlg::CPortEditorDlg(bool isNew, CWnd* pParent)
    : CDialogEx(IDD_PORT_EDITOR, pParent)
    , m_isNew(isNew)
{
    // Title is set in OnInitDialog once the window exists.
}

void CPortEditorDlg::DoDataExchange(CDataExchange* pDX)
{
    CDialogEx::DoDataExchange(pDX);
    DDX_Control(pDX, IDC_PORT_ED_PORT,  m_edPort);
    DDX_Control(pDX, IDC_PORT_ED_PROTO, m_cbProto);
    DDX_Control(pDX, IDC_PORT_ED_DESC,  m_edDesc);
}

BOOL CPortEditorDlg::OnInitDialog()
{
    CDialogEx::OnInitDialog();

    SetWindowText(m_isNew ? L"Agregar puerto" : L"Editar puerto");

    m_cbProto.AddString(L"TCP");
    m_cbProto.AddString(L"UDP");

    if (!m_isNew && m_portNum > 0)
    {
        CString s;
        s.Format(L"%d", m_portNum);
        m_edPort.SetWindowText(s);
        m_cbProto.SetCurSel(m_protoSel);
        m_edDesc.SetWindowText(m_desc);
    }
    else
    {
        m_cbProto.SetCurSel(0);
    }
    return TRUE;
}

void CPortEditorDlg::OnPortKillFocus()
{
    // Auto-fill description from port DB when field is empty
    CString descText;
    m_edDesc.GetWindowText(descText);
    if (!descText.IsEmpty()) return;

    CString portStr;
    m_edPort.GetWindowText(portStr);
    portStr.Trim();
    int pnum = _wtoi(portStr);
    if (pnum < 1 || pnum > 65535) return;

    Protocol proto = (m_cbProto.GetCurSel() == 0) ? Protocol::TCP : Protocol::UDP;
    const wchar_t* desc = PortDB::PortDefaultDesc(pnum, proto);
    if (desc) m_edDesc.SetWindowText(desc);
}

void CPortEditorDlg::OnOK()
{
    CString portStr;
    m_edPort.GetWindowText(portStr);
    portStr.Trim();
    if (portStr.IsEmpty())
    {
        MessageBox(L"Introduzca un n\xfamero de puerto.", L"Campo requerido", MB_ICONWARNING);
        m_edPort.SetFocus();
        return;
    }
    int pnum = _wtoi(portStr);
    if (pnum < 1 || pnum > 65535)
    {
        MessageBox(L"El puerto debe estar entre 1 y 65535.", L"Puerto inv\xe1lido", MB_ICONWARNING);
        m_edPort.SetFocus();
        return;
    }
    m_portNum  = pnum;
    m_protoSel = m_cbProto.GetCurSel();
    m_edDesc.GetWindowText(m_desc);
    CDialogEx::OnOK();
}

// ============================================================================
// CConfigEditorDlg
// ============================================================================
IMPLEMENT_DYNAMIC(CConfigEditorDlg, CDialogEx)

BEGIN_MESSAGE_MAP(CConfigEditorDlg, CDialogEx)
    // Tab / list
    ON_NOTIFY(NM_RCLICK,     IDC_CFG_TAB,  &CConfigEditorDlg::OnTabRClick)
    ON_NOTIFY(NM_RCLICK,     IDC_CFG_LIST, &CConfigEditorDlg::OnListRClick)
    ON_NOTIFY(TCN_SELCHANGE, IDC_CFG_TAB,  &CConfigEditorDlg::OnTabSelChange)
    ON_NOTIFY(LVN_ITEMCHANGED, IDC_CFG_LIST, &CConfigEditorDlg::OnListItemChange)
    ON_NOTIFY(LVN_COLUMNCLICK, IDC_CFG_LIST, &CConfigEditorDlg::OnListColumnClick)

    // Form fields
    ON_EN_CHANGE(IDC_CFG_EDIT_NAME,          &CConfigEditorDlg::OnFormChanged)
    ON_NOTIFY(IPN_FIELDCHANGED, IDC_CFG_EDIT_IP, &CConfigEditorDlg::OnIpChanged)
    ON_CBN_SELCHANGE(IDC_CFG_COMBO_TYPE,     &CConfigEditorDlg::OnFormChanged)

    // Toolbar
    ON_COMMAND(TB_NEW_SRV,   &CConfigEditorDlg::OnBtnNewServer)
    ON_COMMAND(TB_SAVE_SRV,  &CConfigEditorDlg::OnBtnAddServer)
    ON_COMMAND(TB_DEL_SRV,   &CConfigEditorDlg::OnTbDelServer)
    ON_COMMAND(TB_CSV_IN,    &CConfigEditorDlg::OnBtnImportCsv)
    ON_COMMAND(TB_CSV_OUT,   &CConfigEditorDlg::OnBtnExportCsv)
    ON_COMMAND(TB_PORT_ADD,  &CConfigEditorDlg::OnTbPortAdd)
    ON_COMMAND(TB_PORT_EDIT, &CConfigEditorDlg::OnTbPortEdit)
    ON_COMMAND(TB_PORT_DEL,  &CConfigEditorDlg::OnBtnDelPort)

    ON_NOTIFY(TBN_GETINFOTIP, IDC_CFG_TOOLBAR, &CConfigEditorDlg::OnTbGetInfoTip)
END_MESSAGE_MAP()

// ──────────────────────────────────────────────────────────────────────────────
CConfigEditorDlg::CConfigEditorDlg(AppConfig& cfg, CWnd* pParent)
    : CDialogEx(IDD_CONFIG_EDITOR, pParent)
    , m_cfg(cfg)
{
    m_servers = cfg.destinations;
}

void CConfigEditorDlg::DoDataExchange(CDataExchange* pDX)
{
    CDialogEx::DoDataExchange(pDX);
    DDX_Control(pDX, IDC_CFG_EDIT_NAME,    m_edName);
    DDX_Control(pDX, IDC_CFG_EDIT_IP,      m_edIP);
    DDX_Control(pDX, IDC_CFG_COMBO_TYPE,   m_cbType);
    DDX_Control(pDX, IDC_CFG_TAB,          m_tab);
    DDX_Control(pDX, IDC_CFG_LIST,         m_list);
    DDX_Control(pDX, IDC_CFG_COMBO_TIMEOUT, m_cbTimeout);
}

// ──────────────────────────────────────────────────────────────────────────────
// CreateToolbar
// Builds the CToolBarCtrl docked at the top of the dialog.
// Icon size: 32×32 (buttons 38×38 px).  TBSTYLE_TOOLTIPS enables TBN_GETINFOTIP.
// ──────────────────────────────────────────────────────────────────────────────
void CConfigEditorDlg::CreateToolbar()
{
    CRect rcDlg;
    GetClientRect(&rcDlg);
    CRect rcTb(0, 0, rcDlg.Width(), 40);   // ~38px button + 2px padding; AutoSize ajusta

    DWORD tbStyle = WS_CHILD | WS_VISIBLE | WS_CLIPCHILDREN | WS_CLIPSIBLINGS
                  | CCS_TOP | CCS_NODIVIDER | CCS_NORESIZE
                  | TBSTYLE_FLAT | TBSTYLE_TOOLTIPS | TBSTYLE_LIST;

    m_toolbar.Create(tbStyle, rcTb, this, IDC_CFG_TOOLBAR);
    m_toolbar.SetButtonStructSize(sizeof(TBBUTTON));
    m_toolbar.SendMessage(TB_SETEXTENDEDSTYLE, 0, TBSTYLE_EX_MIXEDBUTTONS);

    // ── Image list – 32×32 icons (ILC_COLOR32 para calidad óptima) ──────────────
    m_tbImgList.Create(32, 32, ILC_COLOR32 | ILC_MASK, 7, 1);

    auto addIcon = [&](UINT iconId) {
        HICON h = static_cast<HICON>(
            LoadImage(AfxGetInstanceHandle(),
                      MAKEINTRESOURCE(iconId),
                      IMAGE_ICON, 32, 32, LR_DEFAULTCOLOR));
        if (h) { m_tbImgList.Add(h); DestroyIcon(h); }
        else     m_tbImgList.Add(static_cast<HICON>(nullptr));
    };

    // Image indices must match iImage values in the TBBUTTON array below:
    // 0: srv_add  1: save2  2: srv_del  3: csv_in  4: csv_out  5: port_add  6: port_edit  7: port_del
    addIcon(IDI_ICON_SRV_ADD);    // 0 – Nuevo servidor
    addIcon(IDI_ICON_SAVE2);      // 1 – Guardar cambios
    addIcon(IDI_ICON_SRV_DEL);    // 2 – Borrar servidor
    addIcon(IDI_ICON_CSV_IN);     // 3 – Importar CSV
    addIcon(IDI_ICON_CSV_OUT);    // 4 – Exportar CSV
    addIcon(IDI_ICON_PORT_ADD);   // 5 – Agregar puerto  (port_add.ico)
    addIcon(IDI_ICON_PORT_EDIT);  // 6 – Editar puerto   (port_edit.ico)
    addIcon(IDI_ICON_PORT_DEL);   // 7 – Borrar puerto   (port_del.ico)

    m_toolbar.SetImageList(&m_tbImgList);

    // Fijar tamaños explícitos: bitmap 32×32, botón 38×38 (32 + 3px padding c/lado)
    m_toolbar.SendMessage(TB_SETBITMAPSIZE, 0, MAKELPARAM(32, 32));
    m_toolbar.SendMessage(TB_SETBUTTONSIZE, 0, MAKELPARAM(38, 38));

    // ── Button definitions ────────────────────────────────────────────────────
    TBBUTTON tbb[12]{};
    int n = 0;

    auto btn = [&](UINT id, int img) {
        tbb[n].idCommand = id;
        tbb[n].iBitmap   = img;
        tbb[n].fsState   = TBSTATE_ENABLED;
        tbb[n].fsStyle   = BTNS_BUTTON | BTNS_AUTOSIZE;
        ++n;
    };
    auto sep = [&]() {
        tbb[n].fsStyle = BTNS_SEP;
        tbb[n].iBitmap = 6;   // separator width in pixels
        ++n;
    };

    btn(TB_NEW_SRV,  0);  // Nuevo servidor
    btn(TB_SAVE_SRV, 1);  // Guardar cambios
    btn(TB_DEL_SRV,  2);  // Borrar servidor
    sep();
    btn(TB_CSV_IN,   3);  // Importar CSV
    btn(TB_CSV_OUT,  4);  // Exportar CSV
    sep();
    btn(TB_PORT_ADD,  5); // Agregar puerto
    btn(TB_PORT_EDIT, 6); // Editar puerto
    btn(TB_PORT_DEL,  7); // Borrar puerto

    m_toolbar.AddButtons(n, tbb);
    m_toolbar.AutoSize();
}

// ──────────────────────────────────────────────────────────────────────────────
// RepositionFormRow
// Snaps the server-form controls flush below the toolbar's actual bottom edge,
// independent of DPI or system font.  X-positions match the RC layout (left-
// aligned).  Labels are vertically centred relative to their paired control.
// Called once from OnInitDialog after CreateToolbar().
// ──────────────────────────────────────────────────────────────────────────────
void CConfigEditorDlg::RepositionFormRow()
{
    // ── 1. Toolbar bottom in client pixels ────────────────────────────────────
    CRect rcTb;
    m_toolbar.GetWindowRect(&rcTb);
    ScreenToClient(&rcTb);
    const int tbBottom = rcTb.bottom;

    // ── 2. Map one DU to pixels via MapDialogRect ────────────────────────────
    //    We'll use it to convert the fixed margin (2 DU) to pixels.
    CRect duRef(0, 0, 4, 8);   // 4 DU wide, 8 DU tall (base unit reference)
    MapDialogRect(&duRef);
    const int duW = duRef.Width()  / 4;  // pixels per horizontal DU
    const int duH = duRef.Height() / 8;  // pixels per vertical DU

    // ── 3. Compute row top in pixels (toolbar bottom + 2px gap) ──────────────
    const int editTop = tbBottom + 2 * duH;

    // Helper: get a control's current pixel rect in client coords
    auto getRC = [&](int id) -> CRect {
        CRect r;
        if (auto* p = GetDlgItem(id)) { p->GetWindowRect(&r); ScreenToClient(&r); }
        return r;
    };

    // Helper: move a control to (x, y) keeping its current size
    auto moveTo = [&](int id, int xPx, int yPx) {
        if (auto* p = GetDlgItem(id)) {
            CRect r; p->GetWindowRect(&r); ScreenToClient(&r);
            p->SetWindowPos(nullptr, xPx, yPx, r.Width(), r.Height(),
                            SWP_NOZORDER | SWP_NOACTIVATE);
        }
    };

    // Helper: centre a label vertically relative to a paired edit/combo height
    auto labelTop = [&](int editH, int lblH) -> int {
        return editTop + (editH - lblH) / 2;
    };

    // ── 4. Get reference heights from actual controls ─────────────────────────
    CRect rcName   = getRC(IDC_CFG_EDIT_NAME);
    CRect rcIP     = getRC(IDC_CFG_EDIT_IP);
    CRect rcCombo  = getRC(IDC_CFG_COMBO_TYPE);
    CRect rcLblN   = getRC(IDC_CFG_LBL_NAME);
    CRect rcLblIP  = getRC(IDC_CFG_LBL_IP);
    CRect rcLblT   = getRC(IDC_CFG_LBL_TYPE);

    const int lblH = rcLblN.Height();

    // ── 5. X-positions from RC layout (DU → px):
    //   "Nombre:"  x=7  edit  x=45 w=100
    //   "IP:"      x=153 ctrl x=169 w=88
    //   "Tipo:"    x=265 combo x=291 w=110
    //   (all left-aligned, separator to x=490)
    const int xLblNom = 7  * duW;
    const int xEdtNom = 45 * duW;
    const int xLblIP  = 153 * duW;
    const int xCtrlIP = 169 * duW;
    const int xLblTyp = 265 * duW;
    const int xCboTyp = 291 * duW;

    // Edits/combos all share the same top
    moveTo(IDC_CFG_EDIT_NAME,   xEdtNom, editTop);
    moveTo(IDC_CFG_EDIT_IP,     xCtrlIP, editTop);
    moveTo(IDC_CFG_COMBO_TYPE,  xCboTyp, editTop);

    // Labels vertically centred relative to their paired control
    moveTo(IDC_CFG_LBL_NAME,  xLblNom, labelTop(rcName.Height(),  lblH));
    moveTo(IDC_CFG_LBL_IP,    xLblIP,  labelTop(rcIP.Height(),    lblH));
    moveTo(IDC_CFG_LBL_TYPE,  xLblTyp, labelTop(rcCombo.Height(), lblH));

    // ── 6. Separator: 2px below edit bottom, full client width ───────────────
    const int editBottom = editTop + rcName.Height();
    const int sepTop     = editBottom + 2;
    if (auto* p = GetDlgItem(IDC_CFG_TOOLBAR_SEP))
    {
        CRect rcDlg; GetClientRect(&rcDlg);
        p->SetWindowPos(nullptr, 0, sepTop, rcDlg.Width(), 1,
                        SWP_NOZORDER | SWP_NOACTIVATE);
    }

    // ── 7. Tab + list: shift down by delta from original tab top ─────────────
    const int tabTop = sepTop + 2;

    CRect rcTab, rcList;
    m_tab.GetWindowRect(&rcTab);   ScreenToClient(&rcTab);
    m_list.GetWindowRect(&rcList); ScreenToClient(&rcList);

    const int delta = tabTop - rcTab.top;
    if (delta != 0)
    {
        m_tab.SetWindowPos(nullptr, rcTab.left, tabTop,
                           rcTab.Width(), rcTab.Height() - delta,
                           SWP_NOZORDER | SWP_NOACTIVATE);
        m_list.SetWindowPos(nullptr, rcList.left, rcList.top + delta,
                            rcList.Width(), rcList.Height() - delta,
                            SWP_NOZORDER | SWP_NOACTIVATE);
    }
}

// ──────────────────────────────────────────────────────────────────────────────
// OnInitDialog
// ──────────────────────────────────────────────────────────────────────────────
BOOL CConfigEditorDlg::OnInitDialog()
{
    CDialogEx::OnInitDialog();

    // ── Toolbar ───────────────────────────────────────────────────────────────
    CreateToolbar();
    RepositionFormRow();   // flush form row immediately below actual toolbar bottom

    // ── Server type combo ─────────────────────────────────────────────────────
    m_cbType.AddString(PortDB::TypeName(DestinationType::DC));
    m_cbType.AddString(PortDB::TypeName(DestinationType::PrintServer));
    m_cbType.AddString(PortDB::TypeName(DestinationType::SCCM_Full));
    m_cbType.AddString(PortDB::TypeName(DestinationType::SCCM_DP));
    m_cbType.AddString(PortDB::TypeName(DestinationType::DNS));
    m_cbType.AddString(PortDB::TypeName(DestinationType::DHCP));
    m_cbType.AddString(PortDB::TypeName(DestinationType::Custom));
    m_cbType.SetCurSel(0);

    // ── Tooltips (dialog-level, for form controls) ────────────────────────────
    m_tooltip.Create(this);
    m_tooltip.SetDelayTime(TTDT_INITIAL, 600);
    if (auto* p = GetDlgItem(IDC_CFG_EDIT_NAME))
        m_tooltip.AddTool(p, L"Nombre del servidor");
    if (auto* p = GetDlgItem(IDC_CFG_EDIT_IP))
        m_tooltip.AddTool(p, L"Direcci\xf3n IP del servidor");

    // ── Timeout combo ─────────────────────────────────────────────────────────
    m_cbTimeout.AddString(L"500 ms");
    m_cbTimeout.AddString(L"1000 ms");
    m_cbTimeout.AddString(L"2000 ms");
    int tIdx = 1;
    if      (m_cfg.timeoutMs == 500)  tIdx = 0;
    else if (m_cfg.timeoutMs == 1000) tIdx = 1;
    else if (m_cfg.timeoutMs == 2000) tIdx = 2;
    m_cbTimeout.SetCurSel(tIdx);

    // ── Port list columns ─────────────────────────────────────────────────────
    m_list.SetExtendedStyle(LVS_EX_FULLROWSELECT | LVS_EX_GRIDLINES | LVS_EX_HEADERDRAGDROP);
    m_list.InsertColumn(0, L"Puerto",         LVCFMT_RIGHT, 60);
    m_list.InsertColumn(1, L"Protocolo",      LVCFMT_LEFT,  75);
    m_list.InsertColumn(2, L"Descripci\xf3n", LVCFMT_LEFT, 330);

    // ── Load servers ──────────────────────────────────────────────────────────
    RefreshTabs();
    if (!m_servers.empty())
        SwitchToServer(0);

    UpdateButtonStates();
    return TRUE;
}

// ──────────────────────────────────────────────────────────────────────────────
// IP helpers
// ──────────────────────────────────────────────────────────────────────────────
std::wstring CConfigEditorDlg::GetIP() const
{
    DWORD addr = 0;
    const_cast<CWnd&>(m_edIP).SendMessage(IPM_GETADDRESS, 0, (LPARAM)&addr);
    if (addr == 0) return L"";
    wchar_t buf[20]{};
    swprintf_s(buf, L"%u.%u.%u.%u",
        FIRST_IPADDRESS(addr),  SECOND_IPADDRESS(addr),
        THIRD_IPADDRESS(addr),  FOURTH_IPADDRESS(addr));
    return buf;
}

void CConfigEditorDlg::SetIP(const std::wstring& ip)
{
    if (ip.empty()) { m_edIP.SendMessage(IPM_CLEARADDRESS, 0, 0); return; }
    unsigned a = 0, b = 0, c = 0, d = 0;
    if (swscanf_s(ip.c_str(), L"%u.%u.%u.%u", &a, &b, &c, &d) == 4)
        m_edIP.SendMessage(IPM_SETADDRESS, 0, MAKEIPADDRESS(a, b, c, d));
}

// ──────────────────────────────────────────────────────────────────────────────
// Tab management
// ──────────────────────────────────────────────────────────────────────────────
void CConfigEditorDlg::RefreshTabs()
{
    m_tab.DeleteAllItems();
    for (int i = 0; i < (int)m_servers.size(); ++i)
    {
        TCITEM ti{};
        ti.mask    = TCIF_TEXT;
        CString lbl(m_servers[i].name.c_str());
        ti.pszText = lbl.GetBuffer();
        m_tab.InsertItem(i, &ti);
        lbl.ReleaseBuffer();
    }
    PositionList();
}

void CConfigEditorDlg::SwitchToServer(int idx)
{
    m_curSrv       = idx;
    m_sortCol      = -1;
    m_sortAsc      = true;
    m_dirty        = false;
    m_inhibitDirty = true;

    // Clear header sort arrows
    if (m_list.GetSafeHwnd())
    {
        HDITEM hdi{};
        hdi.mask = HDI_FORMAT;
        CHeaderCtrl* pH = m_list.GetHeaderCtrl();
        if (pH) for (int i = 0; i < 2; ++i)
        {
            pH->GetItem(i, &hdi);
            hdi.fmt &= ~(HDF_SORTUP | HDF_SORTDOWN);
            pH->SetItem(i, &hdi);
        }
    }
    if (idx >= 0 && idx < (int)m_servers.size())
    {
        m_tab.SetCurSel(idx);
        PopulateList(idx);

        const auto& srv = m_servers[idx];
        m_edName.SetWindowText(srv.name.c_str());
        SetIP(srv.ip);

        static const DestinationType kTypes[] =
        {
            DestinationType::DC, DestinationType::PrintServer,
            DestinationType::SCCM_Full, DestinationType::SCCM_DP,
            DestinationType::DNS, DestinationType::DHCP, DestinationType::Custom
        };
        for (int t = 0; t < 7; ++t)
            if (kTypes[t] == srv.type) { m_cbType.SetCurSel(t); break; }
    }
    else
    {
        m_list.DeleteAllItems();
        m_edName.SetWindowText(L"");
        m_edIP.SendMessage(IPM_CLEARADDRESS, 0, 0);
        m_cbType.SetCurSel(0);
    }
    m_inhibitDirty = false;
    UpdateButtonStates();
}

// ──────────────────────────────────────────────────────────────────────────────
// Port list
// ──────────────────────────────────────────────────────────────────────────────
void CConfigEditorDlg::PopulateList(int srvIdx)
{
    m_list.DeleteAllItems();
    if (srvIdx < 0 || srvIdx >= (int)m_servers.size()) return;

    const auto& ports = m_servers[srvIdx].ports;
    for (int i = 0; i < (int)ports.size(); ++i)
    {
        const auto& pe = ports[i];
        m_list.InsertItem(i, std::to_wstring(pe.port).c_str());
        m_list.SetItemText(i, 1, pe.protocol == Protocol::TCP ? L"TCP" : L"UDP");
        m_list.SetItemText(i, 2, pe.description.c_str());
    }
}

// ──────────────────────────────────────────────────────────────────────────────
// EditPortDlg – open CPortEditorDlg for add or edit, return true on IDOK
// ──────────────────────────────────────────────────────────────────────────────
bool CConfigEditorDlg::EditPortDlg(PortEntry& pe, bool isNew)
{
    CPortEditorDlg dlg(isNew, this);

    if (!isNew)
    {
        dlg.m_portNum  = pe.port;
        dlg.m_protoSel = (pe.protocol == Protocol::UDP) ? 1 : 0;
        dlg.m_desc     = pe.description.c_str();
    }

    if (dlg.DoModal() != IDOK) return false;

    pe.port        = dlg.m_portNum;
    pe.protocol    = (dlg.m_protoSel == 1) ? Protocol::UDP : Protocol::TCP;
    pe.description = dlg.m_desc.GetString();
    pe.enabled     = true;
    return true;
}

// ──────────────────────────────────────────────────────────────────────────────
// Button / toolbar handlers
// ──────────────────────────────────────────────────────────────────────────────
void CConfigEditorDlg::OnBtnNewServer()
{
    m_curSrv = -1;
    m_dirty  = false;
    m_edName.SetWindowText(L"");
    m_edIP.SendMessage(IPM_CLEARADDRESS, 0, 0);
    m_cbType.SetCurSel(0);
    UpdateButtonStates();
    m_edName.SetFocus();
}

void CConfigEditorDlg::OnTbDelServer()
{
    if (m_curSrv < 0 || m_curSrv >= (int)m_servers.size()) return;

    CString msg;
    msg.Format(L"\xbfEliminar el servidor \x2018%s\x2019?", m_servers[m_curSrv].name.c_str());
    if (MessageBox(msg, L"Confirmar eliminaci\xf3n", MB_YESNO | MB_ICONQUESTION) != IDYES) return;

    m_servers.erase(m_servers.begin() + m_curSrv);
    m_tab.DeleteItem(m_curSrv);

    int newSel = min(m_curSrv, (int)m_servers.size() - 1);
    m_curSrv = -1;
    if (newSel >= 0)
        SwitchToServer(newSel);
    else
    {
        m_list.DeleteAllItems();
        m_edName.SetWindowText(L"");
        m_edIP.SendMessage(IPM_CLEARADDRESS, 0, 0);
        m_cbType.SetCurSel(0);
        m_dirty = false;
        UpdateButtonStates();
    }
}

void CConfigEditorDlg::OnBtnAddServer()
{
    CString name;
    m_edName.GetWindowText(name);
    name.Trim();
    if (name.IsEmpty())
    {
        MessageBox(L"Introduzca un nombre de servidor.", L"Campo requerido", MB_ICONWARNING);
        m_edName.SetFocus();
        return;
    }
    DWORD ipVal = 0;
    m_edIP.SendMessage(IPM_GETADDRESS, 0, (LPARAM)&ipVal);
    if (ipVal == 0)
    {
        MessageBox(L"Introduzca una direcci\xf3n IP v\xe1lida.", L"Campo requerido", MB_ICONWARNING);
        m_edIP.SetFocus();
        return;
    }

    static const DestinationType kTypes[] =
    {
        DestinationType::DC, DestinationType::PrintServer,
        DestinationType::SCCM_Full, DestinationType::SCCM_DP,
        DestinationType::DNS, DestinationType::DHCP, DestinationType::Custom
    };
    int typeIdx = m_cbType.GetCurSel();
    if (typeIdx < 0) typeIdx = 0;
    DestinationType dt = kTypes[typeIdx];

    if (m_curSrv >= 0 && m_curSrv < (int)m_servers.size())
    {
        // Update existing
        auto& srv = m_servers[m_curSrv];
        srv.name = name.GetString();
        srv.ip   = GetIP();
        srv.type = dt;

        TCITEM ti{};
        ti.mask    = TCIF_TEXT;
        CString lbl(name);
        ti.pszText = lbl.GetBuffer();
        m_tab.SetItem(m_curSrv, &ti);
        lbl.ReleaseBuffer();

        PopulateList(m_curSrv);
        m_dirty = false;
        UpdateButtonStates();
        return;
    }

    // Add new server
    DestinationConfig dc;
    dc.name  = name.GetString();
    dc.ip    = GetIP();
    dc.type  = dt;
    dc.ports = PortDB::GetPorts(dt);
    for (auto& pe : dc.ports) pe.enabled = true;

    m_servers.push_back(std::move(dc));
    int newIdx = (int)m_servers.size() - 1;

    TCITEM ti{};
    ti.mask    = TCIF_TEXT;
    CString lbl(name);
    ti.pszText = lbl.GetBuffer();
    m_tab.InsertItem(newIdx, &ti);
    lbl.ReleaseBuffer();

    PositionList();
    m_dirty = false;
    SwitchToServer(newIdx);
}

// ──────────────────────────────────────────────────────────────────────────────
// Toolbar port handlers  (call EditPortDlg instead of inline form)
// ──────────────────────────────────────────────────────────────────────────────
void CConfigEditorDlg::OnTbPortAdd()
{
    if (m_curSrv < 0) return;

    PortEntry pe{};
    if (!EditPortDlg(pe, true)) return;

    m_servers[m_curSrv].ports.push_back(pe);

    int row = m_list.GetItemCount();
    m_list.InsertItem(row, std::to_wstring(pe.port).c_str());
    m_list.SetItemText(row, 1, pe.protocol == Protocol::TCP ? L"TCP" : L"UDP");
    m_list.SetItemText(row, 2, pe.description.c_str());

    // Select the newly added row
    m_list.SetItemState(row, LVIS_SELECTED | LVIS_FOCUSED, LVIS_SELECTED | LVIS_FOCUSED);
    m_list.EnsureVisible(row, FALSE);
    UpdateButtonStates();
}

void CConfigEditorDlg::OnTbPortEdit()
{
    int sel = m_list.GetNextItem(-1, LVNI_SELECTED);
    if (sel < 0 || m_curSrv < 0) return;
    if (sel >= (int)m_servers[m_curSrv].ports.size()) return;

    PortEntry pe = m_servers[m_curSrv].ports[sel];
    if (!EditPortDlg(pe, false)) return;

    m_servers[m_curSrv].ports[sel] = pe;

    m_list.SetItemText(sel, 0, std::to_wstring(pe.port).c_str());
    m_list.SetItemText(sel, 1, pe.protocol == Protocol::TCP ? L"TCP" : L"UDP");
    m_list.SetItemText(sel, 2, pe.description.c_str());
    UpdateButtonStates();
}

void CConfigEditorDlg::OnBtnDelPort()
{
    if (m_curSrv < 0) return;
    int sel = m_list.GetNextItem(-1, LVNI_SELECTED);
    if (sel < 0) return;

    if (sel < (int)m_servers[m_curSrv].ports.size())
        m_servers[m_curSrv].ports.erase(m_servers[m_curSrv].ports.begin() + sel);
    m_list.DeleteItem(sel);

    int count = m_list.GetItemCount();
    if (count > 0)
    {
        int next = min(sel, count - 1);
        m_list.SetItemState(next, LVIS_SELECTED | LVIS_FOCUSED,
                                  LVIS_SELECTED | LVIS_FOCUSED);
    }
    UpdateButtonStates();
}

// ──────────────────────────────────────────────────────────────────────────────
// List right-click context menu
// ──────────────────────────────────────────────────────────────────────────────
void CConfigEditorDlg::OnListRClick(NMHDR* /*pNMHDR*/, LRESULT* pResult)
{
    if (m_curSrv < 0) { *pResult = 0; return; }

    int sel = m_list.GetNextItem(-1, LVNI_SELECTED);

    CMenu menu;
    menu.CreatePopupMenu();

    if (sel >= 0)
    {
        // Right-click on existing row
        menu.AppendMenu(MF_STRING, TB_PORT_EDIT, L"Editar puerto");
        menu.AppendMenu(MF_STRING, TB_PORT_DEL,  L"Borrar puerto");
    }
    else
    {
        // Right-click on empty area
        menu.AppendMenu(MF_STRING, TB_PORT_ADD, L"Agregar puerto");
    }

    CPoint pt;
    ::GetCursorPos(&pt);
    int cmd = menu.TrackPopupMenu(
        TPM_LEFTALIGN | TPM_RIGHTBUTTON | TPM_RETURNCMD,
        pt.x, pt.y, this);

    switch (cmd)
    {
    case TB_PORT_ADD:  OnTbPortAdd();  break;
    case TB_PORT_EDIT: OnTbPortEdit(); break;
    case TB_PORT_DEL:  OnBtnDelPort(); break;
    }
    *pResult = 0;
}

// ──────────────────────────────────────────────────────────────────────────────
// Toolbar tooltips (TBN_GETINFOTIP)
// ──────────────────────────────────────────────────────────────────────────────
void CConfigEditorDlg::OnTbGetInfoTip(NMHDR* pNMHDR, LRESULT* pResult)
{
    auto* pTip = reinterpret_cast<NMTBGETINFOTIP*>(pNMHDR);

    const wchar_t* tip = L"";
    switch (pTip->iItem)
    {
    case TB_NEW_SRV:   tip = L"Nuevo servidor";     break;
    case TB_SAVE_SRV:  tip = L"Guardar cambios";    break;
    case TB_DEL_SRV:   tip = L"Borrar servidor";    break;
    case TB_CSV_IN:    tip = L"Importar CSV";        break;
    case TB_CSV_OUT:   tip = L"Exportar CSV";        break;
    case TB_PORT_ADD:  tip = L"Agregar puerto";      break;
    case TB_PORT_EDIT: tip = L"Editar puerto";       break;
    case TB_PORT_DEL:  tip = L"Borrar puerto";       break;
    }
    wcsncpy_s(pTip->pszText, pTip->cchTextMax, tip, _TRUNCATE);
    *pResult = 0;
}

// ──────────────────────────────────────────────────────────────────────────────
// Tab right-click (delete server)
// ──────────────────────────────────────────────────────────────────────────────
void CConfigEditorDlg::OnTabRClick(NMHDR* /*pNMHDR*/, LRESULT* pResult)
{
    TCHITTESTINFO ht{};
    ::GetCursorPos(&ht.pt);
    m_tab.ScreenToClient(&ht.pt);
    int clickedTab = m_tab.HitTest(&ht);

    if (clickedTab >= 0 && clickedTab < (int)m_servers.size())
    {
        SwitchToServer(clickedTab);

        CMenu menu;
        menu.CreatePopupMenu();
        CString lbl;
        lbl.Format(L"Eliminar servidor '%s'", m_servers[clickedTab].name.c_str());
        menu.AppendMenu(MF_STRING, 1, lbl);

        CPoint pt;
        ::GetCursorPos(&pt);
        int cmd = menu.TrackPopupMenu(
            TPM_LEFTALIGN | TPM_RIGHTBUTTON | TPM_RETURNCMD,
            pt.x, pt.y, this);

        if (cmd == 1)
        {
            m_servers.erase(m_servers.begin() + m_curSrv);
            m_tab.DeleteItem(m_curSrv);
            int newSel = min(m_curSrv, (int)m_servers.size() - 1);
            m_curSrv = -1;
            if (newSel >= 0)
                SwitchToServer(newSel);
            else
            {
                m_list.DeleteAllItems();
                m_edName.SetWindowText(L"");
                m_edIP.SendMessage(IPM_CLEARADDRESS, 0, 0);
                m_cbType.SetCurSel(0);
                m_dirty = false;
                UpdateButtonStates();
            }
        }
    }
    *pResult = 0;
}

// ──────────────────────────────────────────────────────────────────────────────
// Form change tracking
// ──────────────────────────────────────────────────────────────────────────────
void CConfigEditorDlg::OnFormChanged()
{
    if (m_inhibitDirty) return;
    m_dirty = true;
    UpdateButtonStates();
}

void CConfigEditorDlg::OnIpChanged(NMHDR* /*pNMHDR*/, LRESULT* pResult)
{
    if (!m_inhibitDirty) { m_dirty = true; UpdateButtonStates(); }
    *pResult = 0;
}

// ──────────────────────────────────────────────────────────────────────────────
// OnOK
// ──────────────────────────────────────────────────────────────────────────────
void CConfigEditorDlg::OnOK()
{
    if (m_servers.empty())
    {
        MessageBox(L"A\xf1""ada al menos un servidor antes de guardar.",
                   L"Configuraci\xf3n vac\xeda", MB_ICONWARNING);
        return;
    }
    static const int kTimeouts[] = { 500, 1000, 2000 };
    int tIdx = m_cbTimeout.GetCurSel();
    m_cfg.timeoutMs = (tIdx >= 0 && tIdx < 3) ? kTimeouts[tIdx] : 1000;
    m_cfg.destinations = m_servers;
    CDialogEx::OnOK();
}

// ──────────────────────────────────────────────────────────────────────────────
// List / tab notifications
// ──────────────────────────────────────────────────────────────────────────────
void CConfigEditorDlg::OnTabSelChange(NMHDR* /*pNMHDR*/, LRESULT* pResult)
{
    SwitchToServer(m_tab.GetCurSel());
    *pResult = 0;
}

void CConfigEditorDlg::OnListItemChange(NMHDR* pNMHDR, LRESULT* pResult)
{
    auto* pNM = reinterpret_cast<NMLISTVIEW*>(pNMHDR);
    (void)pNM;
    UpdateButtonStates();
    *pResult = 0;
}

void CConfigEditorDlg::OnListColumnClick(NMHDR* pNMHDR, LRESULT* pResult)
{
    auto* pNM = reinterpret_cast<NMLISTVIEW*>(pNMHDR);
    int col = pNM->iSubItem;
    if (col != 0 && col != 1) { *pResult = 0; return; }

    if (m_sortCol == col)
        m_sortAsc = !m_sortAsc;
    else { m_sortCol = col; m_sortAsc = true; }

    if (m_curSrv < 0 || m_curSrv >= (int)m_servers.size()) { *pResult = 0; return; }

    auto& ports = m_servers[m_curSrv].ports;
    if (col == 0)
    {
        std::stable_sort(ports.begin(), ports.end(),
            [asc = m_sortAsc](const PortEntry& a, const PortEntry& b)
            { return asc ? a.port < b.port : a.port > b.port; });
    }
    else
    {
        std::stable_sort(ports.begin(), ports.end(),
            [asc = m_sortAsc](const PortEntry& a, const PortEntry& b)
            {
                if (a.protocol != b.protocol)
                    return asc ? (a.protocol < b.protocol) : (a.protocol > b.protocol);
                return a.port < b.port;
            });
    }

    HDITEM hdi{};
    hdi.mask = HDI_FORMAT;
    CHeaderCtrl* pH = m_list.GetHeaderCtrl();
    for (int i = 0; i < 2; ++i)
    {
        pH->GetItem(i, &hdi);
        hdi.fmt &= ~(HDF_SORTUP | HDF_SORTDOWN);
        if (i == col)
            hdi.fmt |= (m_sortAsc ? HDF_SORTUP : HDF_SORTDOWN);
        pH->SetItem(i, &hdi);
    }

    PopulateList(m_curSrv);
    UpdateButtonStates();
    *pResult = 0;
}

// ──────────────────────────────────────────────────────────────────────────────
// Layout helpers
// ──────────────────────────────────────────────────────────────────────────────
void CConfigEditorDlg::PositionList()
{
    if (!m_tab.GetSafeHwnd() || !m_list.GetSafeHwnd()) return;

    CRect rc;
    m_tab.GetWindowRect(&rc);
    ScreenToClient(&rc);
    m_tab.AdjustRect(FALSE, &rc);
    rc.DeflateRect(2, 2);
    m_list.MoveWindow(&rc);
}

void CConfigEditorDlg::UpdateButtonStates()
{
    bool hasSrv = (m_curSrv >= 0 && m_curSrv < (int)m_servers.size());
    bool hasSel = hasSrv && (m_list.GetNextItem(-1, LVNI_SELECTED) >= 0);

    CString name; m_edName.GetWindowText(name); name.Trim();
    DWORD ipVal = 0;
    m_edIP.SendMessage(IPM_GETADDRESS, 0, (LPARAM)&ipVal);
    bool hasInput   = !name.IsEmpty() && ipVal != 0;
    bool saveEnabled = hasSrv ? (hasInput && m_dirty) : hasInput;

    if (m_toolbar.GetSafeHwnd())
    {
        m_toolbar.EnableButton(TB_SAVE_SRV,  saveEnabled ? TRUE : FALSE);
        m_toolbar.EnableButton(TB_DEL_SRV,   hasSrv      ? TRUE : FALSE);
        m_toolbar.EnableButton(TB_PORT_ADD,  hasSrv      ? TRUE : FALSE);
        m_toolbar.EnableButton(TB_PORT_EDIT, hasSel      ? TRUE : FALSE);
        m_toolbar.EnableButton(TB_PORT_DEL,  hasSel      ? TRUE : FALSE);
    }
}

// ──────────────────────────────────────────────────────────────────────────────
// PreTranslateMessage
// ──────────────────────────────────────────────────────────────────────────────
BOOL CConfigEditorDlg::PreTranslateMessage(MSG* pMsg)
{
    m_tooltip.RelayEvent(pMsg);
    return CDialogEx::PreTranslateMessage(pMsg);
}

// ──────────────────────────────────────────────────────────────────────────────
// CSV helpers (unchanged logic, same as previous version)
// ──────────────────────────────────────────────────────────────────────────────
static DestinationType CsvParseType(const std::wstring& s)
{
    if (s == L"DC")          return DestinationType::DC;
    if (s == L"PrintServer") return DestinationType::PrintServer;
    if (s == L"SCCM")        return DestinationType::SCCM_Full;
    if (s == L"SCCM_DP")     return DestinationType::SCCM_DP;
    if (s == L"DNS")         return DestinationType::DNS;
    if (s == L"DHCP")        return DestinationType::DHCP;
    if (s == L"Custom")      return DestinationType::Custom;
    return DestinationType::DC;
}
static const wchar_t* CsvTypeName(DestinationType t)
{
    switch (t)
    {
    case DestinationType::DC:          return L"DC";
    case DestinationType::PrintServer: return L"PrintServer";
    case DestinationType::SCCM_Full:   return L"SCCM";
    case DestinationType::SCCM_DP:     return L"SCCM_DP";
    case DestinationType::DNS:         return L"DNS";
    case DestinationType::DHCP:        return L"DHCP";
    case DestinationType::Custom:      return L"Custom";
    }
    return L"DC";
}
static std::vector<std::wstring> CsvSplit(const std::wstring& line)
{
    std::vector<std::wstring> cols;
    std::wstring cur;
    for (wchar_t c : line)
    {
        if (c == L',') { cols.push_back(cur); cur.clear(); }
        else            cur += c;
    }
    cols.push_back(cur);
    return cols;
}

void CConfigEditorDlg::OnBtnExportCsv()
{
    if (m_servers.empty())
    {
        MessageBox(L"No hay servidores que exportar.", L"Exportar CSV", MB_ICONINFORMATION);
        return;
    }
    CFileDialog dlg(FALSE, L"csv", L"NetPortChk_servers.csv",
        OFN_OVERWRITEPROMPT,
        L"Archivos CSV (*.csv)|*.csv|Todos (*.*)|*.*||", this);
    if (dlg.DoModal() != IDOK) { RepositionFormRow(); return; }

    std::wofstream f(dlg.GetPathName().GetString());
    if (!f.is_open()) { MessageBox(L"No se pudo crear el archivo.", L"Error", MB_ICONERROR); return; }

    f << L"hostname,ip,type,ports (port/proto)\n";
    for (const auto& srv : m_servers)
    {
        f << srv.name << L"," << srv.ip << L"," << CsvTypeName(srv.type);
        for (const auto& pe : srv.ports)
            f << L"," << pe.port << L"/" << (pe.protocol == Protocol::TCP ? L"TCP" : L"UDP");
        f << L"\n";
    }
    if (!f.good()) { MessageBox(L"Error al escribir el archivo.", L"Error", MB_ICONERROR); RepositionFormRow(); return; }
    MessageBox(L"Exportaci\xf3n completada.", L"Exportar CSV", MB_ICONINFORMATION);
    RepositionFormRow();
}

void CConfigEditorDlg::OnBtnImportCsv()
{
    CFileDialog dlg(TRUE, L"csv", nullptr,
        OFN_FILEMUSTEXIST,
        L"Archivos CSV (*.csv)|*.csv|Todos (*.*)|*.*||", this);
    if (dlg.DoModal() != IDOK) { RepositionFormRow(); return; }

    std::wifstream f(dlg.GetPathName().GetString());
    if (!f.is_open()) { MessageBox(L"No se pudo abrir el archivo.", L"Error", MB_ICONERROR); return; }

    int imported = 0, skipped = 0;
    std::wstring line;
    bool firstLine = true;

    while (std::getline(f, line))
    {
        if (!line.empty() && line.back() == L'\r') line.pop_back();
        if (line.empty()) continue;

        if (firstLine)
        {
            firstLine = false;
            std::wstring low = line;
            for (auto& c : low) c = towlower(c);
            if (low.find(L"hostname") != std::wstring::npos) continue;
        }

        auto cols = CsvSplit(line);
        if (cols.size() < 4) { ++skipped; continue; }

        DestinationConfig dc;
        dc.name = cols[0];
        dc.ip   = cols[1];
        dc.type = CsvParseType(cols[2]);

        for (size_t i = 3; i < cols.size(); ++i)
        {
            auto& token = cols[i];
            auto slash = token.find(L'/');
            if (slash == std::wstring::npos) continue;
            int port = _wtoi(token.substr(0, slash).c_str());
            if (port < 1 || port > 65535) continue;
            Protocol proto = (token.substr(slash + 1) == L"UDP")
                             ? Protocol::UDP : Protocol::TCP;
            PortEntry pe{port, proto, L"", true};
            const wchar_t* desc = PortDB::PortDefaultDesc(port, proto);
            if (desc) pe.description = desc;
            dc.ports.push_back(pe);
        }

        if (dc.name.empty() || dc.ip.empty() || dc.ports.empty()) { ++skipped; continue; }
        m_servers.push_back(std::move(dc));
        ++imported;
    }

    if (imported == 0)
    {
        CString msg; msg.Format(L"No se import\xf3 ning\xfan servidor. L\xedneas omitidas: %d", skipped);
        MessageBox(msg, L"Importar CSV", MB_ICONWARNING);
        RepositionFormRow();
        return;
    }

    RefreshTabs();
    SwitchToServer((int)m_servers.size() - 1);

    CString msg;
    msg.Format(L"Importados: %d servidor(es). Omitidos: %d", imported, skipped);
    MessageBox(msg, L"Importar CSV", MB_ICONINFORMATION);
    RepositionFormRow();
}
