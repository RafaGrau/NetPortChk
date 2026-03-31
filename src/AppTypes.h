#pragma once
#include <string>
#include <vector>
#include <functional>

// ──────────────────────────────────────────────────────────────────
// Custom window messages
// ──────────────────────────────────────────────────────────────────
#define WM_NC_RESULT   (WM_USER + 100)   // wParam = dest idx, lParam = port idx
#define WM_NC_COMPLETE (WM_USER + 101)   // all checks finished

// ──────────────────────────────────────────────────────────────────
// Enumerations
// ──────────────────────────────────────────────────────────────────
enum class Protocol       { TCP, UDP };
enum class ConnectStatus  { PENDING, OK, FAILED, NO_RESPONSE, UNKNOWN, DISABLED };
enum class DestinationType{ DC, PrintServer, SCCM_Full, SCCM_DP, DNS, DHCP, Custom };

// ──────────────────────────────────────────────────────────────────
// Colours
// ──────────────────────────────────────────────────────────────────
constexpr COLORREF CLR_OK          = RGB(0,   160,   0);
constexpr COLORREF CLR_FAIL        = RGB(200,   0,   0);
constexpr COLORREF CLR_NO_RESPONSE = RGB(100,  100, 180);
constexpr COLORREF CLR_UNKNOWN     = RGB(180, 120,   0);
constexpr COLORREF CLR_PENDING     = RGB(150, 150, 150);
constexpr COLORREF CLR_DISABLED    = RGB(180, 180, 180);

// ──────────────────────────────────────────────────────────────────
// Config file name
// ──────────────────────────────────────────────────────────────────
constexpr wchar_t CONFIG_FILE[] = L"NetPortChk.config";

// ──────────────────────────────────────────────────────────────────
// Data structures
// ──────────────────────────────────────────────────────────────────
struct PortEntry
{
    int          port        {};
    Protocol     protocol    { Protocol::TCP };
    std::wstring description;
    bool         enabled     { true };
};

struct PortResult
{
    PortEntry     entry;
    ConnectStatus status    { ConnectStatus::PENDING };
    DWORD         latencyMs { 0 };
    DWORD         bytesSent { 0 };   // bytes transmitted during probe
    DWORD         bytesRecv { 0 };   // bytes received during probe
    bool          enabled;

    PortResult() : enabled(true) {}
    explicit PortResult(const PortEntry& e) : entry(e), enabled(e.enabled) {}
};

struct DestinationConfig
{
    std::wstring            name;
    std::wstring            ip;
    DestinationType         type  { DestinationType::DC };
    std::vector<PortEntry>  ports;
};

struct DestinationResult
{
    DestinationConfig       config;
    std::vector<PortResult> portResults;
    bool                    completed { false };
};

struct AppConfig
{
    int                             timeoutMs { 1000 };   // port check timeout
    std::vector<DestinationConfig>  destinations;
};

// ──────────────────────────────────────────────────────────────────
// Port database helpers  (implemented in PortDB.cpp)
// ──────────────────────────────────────────────────────────────────
namespace PortDB
{
    std::vector<PortEntry> GetPorts(DestinationType t);
    const wchar_t*         TypeName(DestinationType t);
    const wchar_t*         TypeTag (DestinationType t);
    const wchar_t*         PortDefaultDesc(int port, Protocol proto); // well-known port description
}

// ──────────────────────────────────────────────────────────────────
// String helpers
// ──────────────────────────────────────────────────────────────────
namespace StrUtil
{
    const wchar_t* StatusText(ConnectStatus s);
    const wchar_t* ProtoText (Protocol p);
}
