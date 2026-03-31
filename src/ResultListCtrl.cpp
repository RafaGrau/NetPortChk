#include "pch.h"
#include "ResultListCtrl.h"
#include "resource.h"
#include <commctrl.h>
#include <algorithm>

IMPLEMENT_DYNAMIC(CResultListCtrl, CListCtrl)

BEGIN_MESSAGE_MAP(CResultListCtrl, CListCtrl)
    ON_NOTIFY_REFLECT(NM_CUSTOMDRAW, &CResultListCtrl::OnNMCustomDraw)
    ON_NOTIFY_REFLECT(NM_CLICK,      &CResultListCtrl::OnNMClick)
    ON_WM_CONTEXTMENU()
END_MESSAGE_MAP()

// ââââââââââââââââââââââââââââââââââââââââââââââââââââââââââââââââââââââââââââââ
// Initialise  â call once after the control window is created
// ââââââââââââââââââââââââââââââââââââââââââââââââââââââââââââââââââââââââââââââ
void CResultListCtrl::Initialise()
{
    SetExtendedStyle(LVS_EX_FULLROWSELECT | LVS_EX_GRIDLINES | LVS_EX_DOUBLEBUFFER);
    EnableGroupView(TRUE);

    // Internal column order
    InsertColumn(COL_CHECK,   L"✔",           LVCFMT_CENTER,  28);
    InsertColumn(COL_DEST,    L"IP Destino",   LVCFMT_LEFT,   120);
    InsertColumn(COL_PORT,    L"Puerto",       LVCFMT_RIGHT,   52);
    InsertColumn(COL_PROTO,   L"Protocolo",    LVCFMT_CENTER,  65);
    InsertColumn(COL_DESC,    L"Descripción",  LVCFMT_LEFT,   185);
    InsertColumn(COL_STATUS,  L"Estado",       LVCFMT_CENTER,  68);
    InsertColumn(COL_LATENCY, L"Latencia ms",  LVCFMT_RIGHT,   72);
    InsertColumn(COL_TX,      L"Tx (bytes)",   LVCFMT_RIGHT,   70);
    InsertColumn(COL_RX,      L"Rx (bytes)",   LVCFMT_RIGHT,   70);

    // Visual order:  Destino | Puerto | Activo(check) | Protocolo | Desc | Estado | Latencia
    int order[COL_COUNT] = { COL_DEST, COL_PORT, COL_CHECK,
                              COL_PROTO, COL_DESC, COL_STATUS, COL_LATENCY };
    SetColumnOrderArray(COL_COUNT, order);
    AutoFitColumns();
}

// ââââââââââââââââââââââââââââââââââââââââââââââââââââââââââââââââââââââââââââââ
// PopulateResults
// Groups: one per (destination Ã protocol).  TCP group always precedes UDP.
// ââââââââââââââââââââââââââââââââââââââââââââââââââââââââââââââââââââââââââââââ
void CResultListCtrl::PopulateResults(const std::vector<DestinationResult>& results,
                                       const std::wstring& /*sourceIP*/)
{
    SetRedraw(FALSE);
    DeleteAllItems();
    m_rowMap.clear();
    RemoveAllGroups();

    int flatIdx = 0;

    for (int di = 0; di < static_cast<int>(results.size()); ++di)
    {
        const auto& dr  = results[di];
        const auto& cfg = dr.config;

        // One group per server: "Nombre   IP   [Tipo]"
        int gid = di + 1;   // simple 1-based group ID
        std::wstring hdr = cfg.name
                         + L"   " + cfg.ip
                         + L"   [" + PortDB::TypeName(cfg.type) + L"]";

        LVGROUP grp {};
        grp.cbSize    = sizeof(grp);
        grp.mask      = LVGF_HEADER | LVGF_GROUPID | LVGF_STATE;
        grp.state     = LVGS_COLLAPSIBLE;
        grp.stateMask = LVGS_COLLAPSIBLE;
        grp.iGroupId  = gid;
        grp.pszHeader = hdr.data();
        InsertGroup(-1, &grp);

        // Insert rows: TCP ports first, then UDP (mirrors check order)
        for (int pass = 0; pass < 2; ++pass)
        {
            Protocol proto = (pass == 0) ? Protocol::TCP : Protocol::UDP;
            for (int pi = 0; pi < static_cast<int>(dr.portResults.size()); ++pi)
            {
                const auto& pr = dr.portResults[pi];
                if (pr.entry.protocol != proto) continue;

                m_rowMap.push_back({ di, pi });

                LVITEM li {};
                li.mask     = LVIF_TEXT | LVIF_GROUPID | LVIF_PARAM;
                li.iItem    = flatIdx;
                li.iSubItem = 0;
                li.lParam   = EncodeLParam(flatIdx, pr.enabled);
                li.iGroupId = gid;
                li.pszText  = const_cast<LPWSTR>(L"");
                InsertItem(&li);

                SetItemText(flatIdx, COL_DEST,   cfg.ip.c_str());
                SetItemText(flatIdx, COL_PORT,   std::to_wstring(pr.entry.port).c_str());
                SetItemText(flatIdx, COL_PROTO,  StrUtil::ProtoText(pr.entry.protocol));
                SetItemText(flatIdx, COL_DESC,   pr.entry.description.c_str());
                SetItemText(flatIdx, COL_STATUS, StrUtil::StatusText(pr.status));
                SetItemText(flatIdx, COL_LATENCY,
                    (pr.status == ConnectStatus::OK)
                        ? std::to_wstring(pr.latencyMs).c_str()
                        : L"—");
                SetItemText(flatIdx, COL_TX,
                    pr.bytesSent > 0 ? std::to_wstring(pr.bytesSent).c_str() : L"—");
                SetItemText(flatIdx, COL_RX,
                    pr.bytesRecv > 0 ? std::to_wstring(pr.bytesRecv).c_str() : L"—");
                ++flatIdx;
            }
        }
    }

    SetRedraw(TRUE);
    Invalidate();
    AutoFitColumns();
}

// ââââââââââââââââââââââââââââââââââââââââââââââââââââââââââââââââââââââââââââââ
// UpdateResult  â refresh one row after a check completes
// ââââââââââââââââââââââââââââââââââââââââââââââââââââââââââââââââââââââââââââââ
void CResultListCtrl::UpdateResult(int destIdx, int portIdx, const PortResult& pr)
{
    for (int i = 0; i < static_cast<int>(m_rowMap.size()); ++i)
    {
        if (m_rowMap[i].destIdx == destIdx && m_rowMap[i].portIdx == portIdx)
        {
            SetItemText(i, COL_STATUS,
                StrUtil::StatusText(pr.status));
            SetItemText(i, COL_LATENCY,
                (pr.status == ConnectStatus::OK)
                    ? std::to_wstring(pr.latencyMs).c_str()
                    : L"—");
            SetItemText(i, COL_TX,
                pr.bytesSent > 0 ? std::to_wstring(pr.bytesSent).c_str() : L"—");
            SetItemText(i, COL_RX,
                pr.bytesRecv > 0 ? std::to_wstring(pr.bytesRecv).c_str() : L"—");
            RedrawItems(i, i);
            break;
        }
    }
}

// ââââââââââââââââââââââââââââââââââââââââââââââââââââââââââââââââââââââââââââââ
// SyncCheckState  â update checkbox visual from external state
// ââââââââââââââââââââââââââââââââââââââââââââââââââââââââââââââââââââââââââââââ
void CResultListCtrl::SyncCheckState(int destIdx, int portIdx, bool enabled)
{
    for (int i = 0; i < static_cast<int>(m_rowMap.size()); ++i)
    {
        if (m_rowMap[i].destIdx == destIdx && m_rowMap[i].portIdx == portIdx)
        {
            LVITEM li {}; li.mask = LVIF_PARAM; li.iItem = i;
            GetItem(&li);
            li.lParam = EncodeLParam(LParamIdx(li.lParam), enabled);
            SetItem(&li);
            RedrawItems(i, i);
            break;
        }
    }
}

// ââââââââââââââââââââââââââââââââââââââââââââââââââââââââââââââââââââââââââââââ
// NM_CUSTOMDRAW  â draw checkbox column + colour status text
// ââââââââââââââââââââââââââââââââââââââââââââââââââââââââââââââââââââââââââââââ
void CResultListCtrl::OnNMCustomDraw(NMHDR* pNMHDR, LRESULT* pResult)
{
    auto* pCD = reinterpret_cast<NMLVCUSTOMDRAW*>(pNMHDR);
    *pResult = CDRF_DODEFAULT;

    switch (pCD->nmcd.dwDrawStage)
    {
    case CDDS_PREPAINT:
        *pResult = CDRF_NOTIFYITEMDRAW;
        return;

    case CDDS_ITEMPREPAINT:
        // Request per-sub-item notifications for ALL columns (including col 0)
        *pResult = CDRF_NOTIFYSUBITEMDRAW | CDRF_NEWFONT;
        return;

    case CDDS_SUBITEM | CDDS_ITEMPREPAINT:
    {
        int  item = static_cast<int>(pCD->nmcd.dwItemSpec);
        int  sub  = pCD->iSubItem;
        CDC* dc   = CDC::FromHandle(pCD->nmcd.hdc);

        if (sub == COL_CHECK)
        {
            // Use pCD->nmcd.rc directly – it holds the exact sub-item rect.
            // GetSubItemRect(item, 0, LVIR_BOUNDS) returns the FULL ROW rect
            // for column 0 (documented Win32 ListView quirk) and must NOT be used.
            CRect r(pCD->nmcd.rc);

            bool enabled = false;
            if (item < static_cast<int>(m_rowMap.size()))
            {
                LVITEM li {}; li.mask = LVIF_PARAM; li.iItem = item;
                GetItem(&li);
                enabled = LParamEnabled(li.lParam);
            }
            DrawCheckCell(dc, r, enabled, false);
            *pResult = CDRF_SKIPDEFAULT;
            return;
        }

        if (sub == COL_STATUS)
        {
            CString txt = GetItemText(item, COL_STATUS);
            ConnectStatus cs = ConnectStatus::PENDING;
            if      (txt == L"OK")            cs = ConnectStatus::OK;
            else if (txt == L"CERRADO")       cs = ConnectStatus::FAILED;
            else if (txt == L"SIN RESPUESTA") cs = ConnectStatus::NO_RESPONSE;
            else if (txt == L"DESCONOCIDO")   cs = ConnectStatus::UNKNOWN;
            else if (txt == L"\u2014")         cs = ConnectStatus::DISABLED;
            pCD->clrText = StatusColor(cs);
            *pResult = CDRF_NEWFONT;
            return;
        }
        break;
    }
    default: break;
    }
}

// ââââââââââââââââââââââââââââââââââââââââââââââââââââââââââââââââââââââââââââââ
// DrawCheckCell  â Windows-themed checkbox centred in the cell
// ââââââââââââââââââââââââââââââââââââââââââââââââââââââââââââââââââââââââââââââ
void CResultListCtrl::DrawCheckCell(CDC* dc, const CRect& cellRect,
                                     bool checked, bool /*disabled*/)
{
    dc->FillSolidRect(cellRect, ::GetSysColor(COLOR_WINDOW));

    constexpr int SZ = 13;
    int x = cellRect.left + (cellRect.Width()  - SZ) / 2;
    int y = cellRect.top  + (cellRect.Height() - SZ) / 2;
    CRect box(x, y, x + SZ, y + SZ);

    UINT state = DFCS_BUTTONCHECK | DFCS_FLAT | (checked ? DFCS_CHECKED : 0);
    ::DrawFrameControl(dc->m_hDC, &box, DFC_BUTTON, state);
}

// ââââââââââââââââââââââââââââââââââââââââââââââââââââââââââââââââââââââââââââââ
// NM_CLICK  â toggle the checkbox of a single row
// ââââââââââââââââââââââââââââââââââââââââââââââââââââââââââââââââââââââââââââââ
void CResultListCtrl::OnNMClick(NMHDR* pNMHDR, LRESULT* pResult)
{
    *pResult = 0;
    auto* pNI = reinterpret_cast<NMITEMACTIVATE*>(pNMHDR);

    LVHITTESTINFO ht {};
    ht.pt = pNI->ptAction;
    SubItemHitTest(&ht);
    if (ht.iItem < 0 || ht.iSubItem != COL_CHECK) return;

    int item = ht.iItem;
    if (item >= static_cast<int>(m_rowMap.size())) return;

    LVITEM li {}; li.mask = LVIF_PARAM; li.iItem = item;
    GetItem(&li);
    bool newEnabled = !LParamEnabled(li.lParam);
    li.lParam = EncodeLParam(LParamIdx(li.lParam), newEnabled);
    SetItem(&li);
    RedrawItems(item, item);

    auto [di, pi] = m_rowMap[item];
    if (m_toggleCb) m_toggleCb(di, pi, newEnabled);
}

// ââââââââââââââââââââââââââââââââââââââââââââââââââââââââââââââââââââââââââââââ
// WM_CONTEXTMENU  â right-click â popup menu
// ââââââââââââââââââââââââââââââââââââââââââââââââââââââââââââââââââââââââââââââ
void CResultListCtrl::OnContextMenu(CWnd* /*pWnd*/, CPoint screenPt)
{
    // Determine which destination was right-clicked (if any)
    CPoint clientPt = screenPt;
    ScreenToClient(&clientPt);

    LVHITTESTINFO ht {};
    ht.pt = clientPt;
    HitTest(&ht);


    // Load the popup from resources
    CMenu menu;
    if (!menu.LoadMenu(IDR_CONTEXT_LIST)) return;
    CMenu* pPopup = menu.GetSubMenu(0);
    if (!pPopup) return;

    // Show the menu
    UINT cmd = static_cast<UINT>(pPopup->TrackPopupMenu(
        TPM_LEFTALIGN | TPM_RIGHTBUTTON | TPM_RETURNCMD | TPM_NONOTIFY,
        screenPt.x, screenPt.y, GetParent()));

    if (cmd == 0) return;

    // Build lambda helper: apply batch toggle
    auto ApplyBatch = [&](int destFilter, Protocol const* protoFilter, bool enable)
    {
        for (int i = 0; i < static_cast<int>(m_rowMap.size()); ++i)
        {
            int di = m_rowMap[i].destIdx;
            int pi = m_rowMap[i].portIdx;
            if (destFilter >= 0 && di != destFilter) continue;

            // Need to know protocol of this row â read from col text
            CString protoTxt = GetItemText(i, COL_PROTO);
            Protocol rowProto = (protoTxt == L"UDP") ? Protocol::UDP : Protocol::TCP;
            if (protoFilter && rowProto != *protoFilter) continue;

            LVITEM li {}; li.mask = LVIF_PARAM; li.iItem = i;
            GetItem(&li);
            li.lParam = EncodeLParam(LParamIdx(li.lParam), enable);
            SetItem(&li);
        }
        Invalidate();

        if (m_batchCb) m_batchCb(destFilter, protoFilter, enable);
    };

    Protocol tcp = Protocol::TCP, udp = Protocol::UDP;

    switch (cmd)
    {
    case ID_CTX_ENABLE_ALL:      ApplyBatch(-1,            nullptr, true);  break;
    case ID_CTX_DISABLE_ALL:     ApplyBatch(-1,            nullptr, false); break;
    case ID_CTX_ENABLE_TCP:      ApplyBatch(-1,            &tcp,    true);  break;
    case ID_CTX_DISABLE_TCP:     ApplyBatch(-1,            &tcp,    false); break;
    case ID_CTX_ENABLE_UDP:      ApplyBatch(-1,            &udp,    true);  break;
    case ID_CTX_DISABLE_UDP:     ApplyBatch(-1,            &udp,    false); break;
    default: break;
    }
}

// ââââââââââââââââââââââââââââââââââââââââââââââââââââââââââââââââââââââââââââââ

// ──────────────────────────────────────────────────────────────────────────────
// AutoFitColumns
//   - Centres all column HEADERS via CHeaderCtrl (content alignment unchanged)
//   - Sets each column width = max(header text, widest content cell)
//     LVSCW_AUTOSIZE_USEHEADER does exactly that.
// ──────────────────────────────────────────────────────────────────────────────
void CResultListCtrl::AutoFitColumns()
{
    CHeaderCtrl* pHdr = GetHeaderCtrl();

    // 1. Centre every column header
    if (pHdr)
    {
        int nHdr = pHdr->GetItemCount();
        for (int c = 0; c < nHdr; ++c)
        {
            HDITEM hdi {};
            hdi.mask = HDI_FORMAT;
            pHdr->GetItem(c, &hdi);
            hdi.fmt = (hdi.fmt & ~(HDF_LEFT | HDF_RIGHT)) | HDF_CENTER;
            pHdr->SetItem(c, &hdi);
        }
    }

    // 2. Robust column sizing:
    //    a) LVSCW_AUTOSIZE         → width of widest CONTENT cell
    //    b) Measure header text manually via HDC
    //    c) Final width = max(a, b, minimum)
    //
    // We cannot rely solely on LVSCW_AUTOSIZE_USEHEADER because:
    //  - In group-view mode some ComCtl versions ignore the header part
    //  - Columns with custom-drawn cells (COL_CHECK) report 0 content width
    CDC* pDC = GetDC();
    CFont* pOldFont = pDC ? pDC->SelectObject(GetFont()) : nullptr;

    for (int c = 0; c < COL_COUNT; ++c)
    {
        // a) Content width
        SetColumnWidth(c, LVSCW_AUTOSIZE);
        int wContent = GetColumnWidth(c);
        if (wContent < 0) wContent = 0;

        // b) Header text width
        int wHeader = 0;
        if (pHdr && pDC)
        {
            HDITEM hdi {};
            wchar_t buf[128] = {};
            hdi.mask       = HDI_TEXT;
            hdi.pszText    = buf;
            hdi.cchTextMax = 128;
            if (pHdr->GetItem(c, &hdi))
            {
                CSize sz = pDC->GetTextExtent(buf);
                // Add sort-arrow + padding allowance (≈ 26 px)
                wHeader = sz.cx + 26;
            }
        }

        // c) Pick the larger, enforce per-column minimums
        int wFinal = max(wContent, wHeader);

        if (c == COL_CHECK)
            wFinal = max(wFinal, 32);   // checkbox needs room
        else
            wFinal = max(wFinal, 40);   // general minimum

        SetColumnWidth(c, wFinal);
    }

    if (pDC)
    {
        if (pOldFont) pDC->SelectObject(pOldFont);
        ReleaseDC(pDC);
    }
}

COLORREF CResultListCtrl::StatusColor(ConnectStatus s)
{
    switch (s) {
    case ConnectStatus::OK:          return CLR_OK;
    case ConnectStatus::FAILED:      return CLR_FAIL;
    case ConnectStatus::NO_RESPONSE: return CLR_NO_RESPONSE;
    case ConnectStatus::UNKNOWN:     return CLR_UNKNOWN;
    case ConnectStatus::DISABLED:    return CLR_DISABLED;
    default:                      return CLR_PENDING;
    }
}
