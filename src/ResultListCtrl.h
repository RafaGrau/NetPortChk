#pragma once
#include "AppTypes.h"
#include <vector>
#include <functional>

// ──────────────────────────────────────────────────────────────────────────────
// Column indices (internal storage order)
// Visual order:  Activo | Destino/IP | Puerto | Protocolo | Descripción | Estado | Latencia ms
// ──────────────────────────────────────────────────────────────────────────────
enum ListCol
{
    COL_CHECK   = 0,   // Activo   – checkbox manual
    COL_DEST    = 1,   // Destino/IP
    COL_PORT    = 2,   // Puerto
    COL_PROTO   = 3,   // Protocolo
    COL_DESC    = 4,   // Descripción
    COL_STATUS  = 5,   // Estado
    COL_LATENCY = 6,   // Latencia ms
    COL_TX      = 7,   // Bytes enviados
    COL_RX      = 8,   // Bytes recibidos
    COL_COUNT   = 9
};

// ──────────────────────────────────────────────────────────────────────────────
// Callbacks
// ──────────────────────────────────────────────────────────────────────────────
// Single toggle: (destIdx, portIdx, newEnabled)
using CheckToggleCb = std::function<void(int destIdx, int portIdx, bool newEnabled)>;

// Batch: apply newEnabled to all ports matching filter
//   destIdx == -1  → all destinations
//   proto   == nullptr → all protocols
using BatchToggleCb = std::function<void(int destIdx, Protocol const* proto, bool newEnabled)>;

class CResultListCtrl : public CListCtrl
{
    DECLARE_DYNAMIC(CResultListCtrl)

public:
    CResultListCtrl() = default;

    // Called once after Create / subclassing
    void Initialise();

    // (Re-)populate from results – rebuilds groups and all rows
    void PopulateResults(const std::vector<DestinationResult>& results,
                         const std::wstring& sourceIP = L"");

    // Update a single row's status/latency after WM_NC_RESULT
    void UpdateResult(int destIdx, int portIdx, const PortResult& pr);

    // Sync the checkbox state of a row from external code
    void SyncCheckState(int destIdx, int portIdx, bool enabled);

    // Auto-fit all columns: centre headers, width = max(header, content)
    void AutoFitColumns();

    // Callbacks
    void SetCheckToggleCb(CheckToggleCb cb) { m_toggleCb  = cb; }
    void SetBatchToggleCb(BatchToggleCb cb) { m_batchCb   = cb; }

protected:
    afx_msg void OnNMCustomDraw (NMHDR* pNMHDR, LRESULT* pResult);
    afx_msg void OnNMClick      (NMHDR* pNMHDR, LRESULT* pResult);
    afx_msg void OnContextMenu  (CWnd* pWnd, CPoint point);

    DECLARE_MESSAGE_MAP()

private:
    // ── Row map ──────────────────────────────────────────────────────────────
    struct RowTag { int destIdx; int portIdx; };
    std::vector<RowTag> m_rowMap;          // flat item index → (destIdx, portIdx)

    // ── Callbacks ────────────────────────────────────────────────────────────
    CheckToggleCb m_toggleCb;
    BatchToggleCb m_batchCb;

    // ── Context menu helpers ─────────────────────────────────────────────────

    // ── Drawing ──────────────────────────────────────────────────────────────
    void     DrawCheckCell(CDC* dc, const CRect& cellRect, bool checked, bool disabled);
    COLORREF StatusColor(ConnectStatus s);

    // ── lParam encoding ──────────────────────────────────────────────────────
    // Bit 31 = enabled flag (1 = enabled / checked)
    // Bits 0-30 = flat item index (row identity)
    static LPARAM EncodeLParam(int flatIdx, bool enabled)
    {
        return static_cast<LPARAM>(flatIdx) | (enabled ? 0x80000000L : 0L);
    }
    static bool   LParamEnabled(LPARAM lp) { return (lp & 0x80000000L) != 0; }
    static int    LParamIdx    (LPARAM lp) { return static_cast<int>(lp & 0x7FFFFFFFL); }
};
