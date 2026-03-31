#include "pch.h"
#include "AppTypes.h"

namespace PortDB
{

// ─── Domain Controller ────────────────────────────────────────────────────────
static const std::vector<PortEntry> s_DC =
{
    {  53,  Protocol::TCP, L"DNS",                          true},
    {  53,  Protocol::UDP, L"DNS (UDP)",                    true},
    {  88,  Protocol::TCP, L"Kerberos",                     true},
    {  88,  Protocol::UDP, L"Kerberos (UDP)",               true},
    { 135,  Protocol::TCP, L"RPC Endpoint Mapper",          true},
    { 139,  Protocol::TCP, L"NetBIOS Session",              true},
    { 389,  Protocol::TCP, L"LDAP",                         true},
    { 389,  Protocol::UDP, L"LDAP (UDP)",                   true},
    { 445,  Protocol::TCP, L"SMB",                          true},
    { 464,  Protocol::TCP, L"Kerberos Pwd Change",          true},
    { 464,  Protocol::UDP, L"Kerberos Pwd Change (UDP)",    true},
    { 636,  Protocol::TCP, L"LDAPS",                        true},
    {3268,  Protocol::TCP, L"Global Catalog",               true},
    {3269,  Protocol::TCP, L"Global Catalog SSL",           true},
    {9389,  Protocol::TCP, L"AD Web Services",              true},
    {5985,  Protocol::TCP, L"WinRM HTTP",                   false},
    {5986,  Protocol::TCP, L"WinRM HTTPS",                  false},
};

// ─── Print Server ─────────────────────────────────────────────────────────────
static const std::vector<PortEntry> s_PS =
{
    {  80,  Protocol::TCP, L"HTTP (IPP/Web UI)",            true},
    { 443,  Protocol::TCP, L"HTTPS",                        true},
    { 445,  Protocol::TCP, L"SMB (Print$)",                 true},
    { 515,  Protocol::TCP, L"LPD",                          true},
    { 631,  Protocol::TCP, L"IPP",                          true},
    {9100,  Protocol::TCP, L"RAW (JetDirect)",              true},
    { 135,  Protocol::TCP, L"RPC",                          false},
};

// ─── SCCM Full (Management Point + Distribution Point + SUP) ─────────────────
static const std::vector<PortEntry> s_SCCM_Full =
{
    {  80,  Protocol::TCP, L"HTTP (MP/DP/SUP)",             true},
    { 443,  Protocol::TCP, L"HTTPS (MP/DP/SUP)",            true},
    { 445,  Protocol::TCP, L"SMB",                          true},
    { 135,  Protocol::TCP, L"RPC Endpoint Mapper",          true},
    {8530,  Protocol::TCP, L"WSUS HTTP",                    true},
    {8531,  Protocol::TCP, L"WSUS HTTPS",                   true},
    {2701,  Protocol::TCP, L"Remote Control",               true},
    {2702,  Protocol::TCP, L"Remote Control (Enc.)",        false},
    {10123, Protocol::TCP, L"Client → MP (alt)",            true},
    {10800, Protocol::UDP, L"Wake On LAN",                  false},
    {9 ,    Protocol::UDP, L"Wake On LAN (port 9)",         false},
};

// ─── SCCM Distribution Point only ────────────────────────────────────────────
static const std::vector<PortEntry> s_SCCM_DP =
{
    {  80,  Protocol::TCP, L"HTTP Content",                 true},
    { 443,  Protocol::TCP, L"HTTPS Content",                true},
    { 445,  Protocol::TCP, L"SMB (Content Share)",          true},
    {8530,  Protocol::TCP, L"WSUS HTTP",                    true},
    {8531,  Protocol::TCP, L"WSUS HTTPS",                   true},
};

// ─── DNS Server ───────────────────────────────────────────────────────────────
static const std::vector<PortEntry> s_DNS =
{
    {  53,  Protocol::TCP, L"DNS (TCP)",                    true},
    {  53,  Protocol::UDP, L"DNS (UDP)",                    true},
    { 853,  Protocol::TCP, L"DNS over TLS",                 false},
    { 953,  Protocol::TCP, L"RNDC (Named control)",         false},
};

// ─── DHCP Server ──────────────────────────────────────────────────────────────
static const std::vector<PortEntry> s_DHCP =
{
    {  67,  Protocol::UDP, L"DHCP Server",                  true},
    {  68,  Protocol::UDP, L"DHCP Client",                  true},
    { 546,  Protocol::UDP, L"DHCPv6 Client",                false},
    { 547,  Protocol::UDP, L"DHCPv6 Server",                false},
};

// ─── Custom (no default ports) ────────────────────────────────────────────────
// Returns empty vector; user adds ports manually via the port editor dialog.

// ─── Public API ───────────────────────────────────────────────────────────────
std::vector<PortEntry> GetPorts(DestinationType t)
{
    switch (t)
    {
    case DestinationType::DC:          return s_DC;
    case DestinationType::PrintServer: return s_PS;
    case DestinationType::SCCM_Full:   return s_SCCM_Full;
    case DestinationType::SCCM_DP:     return s_SCCM_DP;
    case DestinationType::DNS:         return s_DNS;
    case DestinationType::DHCP:        return s_DHCP;
    case DestinationType::Custom:      return {};
    }
    return {};
}

const wchar_t* TypeName(DestinationType t)
{
    switch (t)
    {
    case DestinationType::DC:          return L"Domain Controller";
    case DestinationType::PrintServer: return L"Print Server";
    case DestinationType::SCCM_Full:   return L"SCCM (Full)";
    case DestinationType::SCCM_DP:     return L"SCCM DP";
    case DestinationType::DNS:         return L"Servidor DNS";
    case DestinationType::DHCP:        return L"Servidor DHCP";
    case DestinationType::Custom:      return L"Personalizado";
    }
    return L"Unknown";
}

const wchar_t* TypeTag(DestinationType t)
{
    switch (t)
    {
    case DestinationType::DC:          return L"DC";
    case DestinationType::PrintServer: return L"PrintServer";
    case DestinationType::SCCM_Full:   return L"SCCM_Full";
    case DestinationType::SCCM_DP:     return L"SCCM_DP";
    case DestinationType::DNS:         return L"DNS";
    case DestinationType::DHCP:        return L"DHCP";
    case DestinationType::Custom:      return L"Custom";
    }
    return L"Unknown";
}

} // namespace PortDB

namespace PortDB
{
const wchar_t* PortDefaultDesc(int port, Protocol proto)
{
    struct Entry { int port; Protocol proto; const wchar_t* desc; };
    static const Entry kTable[] =
    {
        {   7, Protocol::TCP, L"Echo"},
        {   9, Protocol::UDP, L"Wake On LAN"},
        {  20, Protocol::TCP, L"FTP Data"},
        {  21, Protocol::TCP, L"FTP Control"},
        {  22, Protocol::TCP, L"SSH"},
        {  23, Protocol::TCP, L"Telnet"},
        {  25, Protocol::TCP, L"SMTP"},
        {  53, Protocol::TCP, L"DNS"},
        {  53, Protocol::UDP, L"DNS (UDP)"},
        {  67, Protocol::UDP, L"DHCP Server"},
        {  68, Protocol::UDP, L"DHCP Client"},
        {  69, Protocol::UDP, L"TFTP"},
        {  80, Protocol::TCP, L"HTTP"},
        {  88, Protocol::TCP, L"Kerberos"},
        {  88, Protocol::UDP, L"Kerberos (UDP)"},
        { 110, Protocol::TCP, L"POP3"},
        { 123, Protocol::UDP, L"NTP"},
        { 135, Protocol::TCP, L"RPC Endpoint Mapper"},
        { 137, Protocol::UDP, L"NetBIOS Name"},
        { 138, Protocol::UDP, L"NetBIOS Datagram"},
        { 139, Protocol::TCP, L"NetBIOS Session"},
        { 143, Protocol::TCP, L"IMAP"},
        { 161, Protocol::UDP, L"SNMP"},
        { 162, Protocol::UDP, L"SNMP Trap"},
        { 389, Protocol::TCP, L"LDAP"},
        { 389, Protocol::UDP, L"LDAP (UDP)"},
        { 443, Protocol::TCP, L"HTTPS"},
        { 445, Protocol::TCP, L"SMB"},
        { 464, Protocol::TCP, L"Kerberos Pwd Change"},
        { 464, Protocol::UDP, L"Kerberos Pwd Change (UDP)"},
        { 465, Protocol::TCP, L"SMTPS"},
        { 514, Protocol::UDP, L"Syslog"},
        { 515, Protocol::TCP, L"LPD"},
        { 587, Protocol::TCP, L"SMTP Submission"},
        { 631, Protocol::TCP, L"IPP"},
        { 636, Protocol::TCP, L"LDAPS"},
        { 993, Protocol::TCP, L"IMAPS"},
        { 995, Protocol::TCP, L"POP3S"},
        {1433, Protocol::TCP, L"SQL Server"},
        {1434, Protocol::UDP, L"SQL Server Browser"},
        {2701, Protocol::TCP, L"SCCM Remote Control"},
        {3268, Protocol::TCP, L"LDAP Global Catalog"},
        {3269, Protocol::TCP, L"LDAP Global Catalog SSL"},
        {3389, Protocol::TCP, L"RDP"},
        {5985, Protocol::TCP, L"WinRM HTTP"},
        {5986, Protocol::TCP, L"WinRM HTTPS"},
        {8530, Protocol::TCP, L"WSUS HTTP"},
        {8531, Protocol::TCP, L"WSUS HTTPS"},
        {9100, Protocol::TCP, L"RAW Printing (JetDirect)"},
        {9389, Protocol::TCP, L"AD Web Services"},
        {10123,Protocol::TCP, L"SCCM Client \x2192 MP (alt)"},
    };
    for (const auto& e : kTable)
        if (e.port == port && e.proto == proto) return e.desc;
    return nullptr;
}
} // namespace PortDB

namespace StrUtil
{
const wchar_t* StatusText(ConnectStatus s)
{
    switch (s)
    {
    case ConnectStatus::OK:          return L"OK";
    case ConnectStatus::FAILED:      return L"CERRADO";
    case ConnectStatus::NO_RESPONSE: return L"SIN RESPUESTA";
    case ConnectStatus::UNKNOWN:     return L"DESCONOCIDO";
    case ConnectStatus::PENDING:     return L"PENDIENTE";
    case ConnectStatus::DISABLED:    return L"\u2014";
    }
    return L"";
}

const wchar_t* ProtoText(Protocol p)
{
    return (p == Protocol::TCP) ? L"TCP" : L"UDP";
}
} // namespace StrUtil
