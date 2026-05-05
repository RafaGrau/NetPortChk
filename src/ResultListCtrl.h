#pragma once
#include "AppTypes.h"
#include <vector>
#include <map>
#include <utility>
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

    // Returns the destination index for a given list item (-1 if invalid)
    int GetItemDestIdx(int item) const
    {
        if (item < 0 || item >= static_cast<int>(m_rowMap.size())) return -1;
        return m_rowMap[item].destIdx;
    }

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
    // Status guardado aquí evita comparar texto traducido en custom-draw,
    // que rompería al traducir StatusText (StrUtil) a otro idioma.
    struct RowTag { int destIdx; int portIdx; ConnectStatus status { ConnectStatus::PENDING }; };
    std::vector<RowTag> m_rowMap;          // flat item index → (destIdx, portIdx, status)

    // Acelerador O(1) para UpdateResult/SyncCheckState — antes O(n) por
    // recorrido lineal de m_rowMap por cada paquete recibido.
    std::map<std::pair<int,int>, int> m_indexByDP;

    // Estado: si ya se ha hecho el autosize de columnas. AutoFitColumns es
    // costoso (LVSCW_AUTOSIZE recorre todas las celdas) y no merece la pena
    // reaplicarlo cada vez que se repuebla la lista durante un escaneo.
    bool m_columnsFitted { false };

    // ── Callbacks ────────────────────────────────────────────────────────────
    CheckToggleCb m_toggleCb;
    BatchToggleCb m_batchCb;

    // ── Drawing ──────────────────────────────────────────────────────────────
    void     DrawCheckCell(CDC* dc, const CRect& cellRect, bool checked, bool disabled);
    COLORREF StatusColor(ConnectStatus s);

    // ── lParam encoding ──────────────────────────────────────────────────────
    // Bit 31    = enabled flag (1 = enabled / checked)
    // Bits 0-30 = flat item index (row identity)
    //
    // Usar constexpr LPARAM evita el sign-extend a 64 bits que ocurre al
    // mezclar el literal `long` 0x80000000L con LPARAM (es int64 en x64).
    static constexpr LPARAM kEnabledBit = static_cast<LPARAM>(1) << 31;
    static constexpr LPARAM kIdxMask    = kEnabledBit - 1;

    static LPARAM EncodeLParam(int flatIdx, bool enabled)
    {
        return (static_cast<LPARAM>(flatIdx) & kIdxMask) | (enabled ? kEnabledBit : 0);
    }
    static bool   LParamEnabled(LPARAM lp) { return (lp & kEnabledBit) != 0; }
    static int    LParamIdx    (LPARAM lp) { return static_cast<int>(lp & kIdxMask); }
};
