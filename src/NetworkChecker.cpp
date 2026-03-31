#include "pch.h"
#include "NetworkChecker.h"
#include <mstcpip.h>    // SIO_UDP_CONNRESET

// Fallback: some SDK configurations don't expose SIO_UDP_CONNRESET via mstcpip.h
#ifndef SIO_UDP_CONNRESET
#  define SIO_UDP_CONNRESET  _WSAIOW(IOC_VENDOR, 12)
#endif

// ──────────────────────────────────────────────────────────────────────────────
// TCP connect with 3-second timeout (non-blocking socket + select)
// ──────────────────────────────────────────────────────────────────────────────
ConnectStatus NetworkChecker::CheckTCP(const std::wstring& ip, int port, DWORD& latMs,
                                       DWORD& bytesSent, DWORD& bytesRecv, int timeoutMs)
{
    latMs = 0; bytesSent = 0; bytesRecv = 0;

    // Convert wide IP to narrow
    char ipA[64] = {};
    WideCharToMultiByte(CP_ACP, 0, ip.c_str(), -1, ipA, sizeof(ipA), nullptr, nullptr);

    SOCKET s = socket(AF_INET, SOCK_STREAM, IPPROTO_TCP);
    if (s == INVALID_SOCKET) return ConnectStatus::UNKNOWN;

    // Non-blocking
    u_long mode = 1;
    ioctlsocket(s, FIONBIO, &mode);

    sockaddr_in addr {};
    addr.sin_family = AF_INET;
    addr.sin_port   = htons(static_cast<u_short>(port));
    inet_pton(AF_INET, ipA, &addr.sin_addr);

    auto t0 = std::chrono::steady_clock::now();
    connect(s, reinterpret_cast<sockaddr*>(&addr), sizeof(addr));

    fd_set wfds, efds;
    FD_ZERO(&wfds); FD_SET(s, &wfds);
    FD_ZERO(&efds); FD_SET(s, &efds);

    timeval tv { timeoutMs / 1000, (timeoutMs % 1000) * 1000 };
    int sel = select(0, nullptr, &wfds, &efds, &tv);

    ConnectStatus result = ConnectStatus::FAILED;
    if (sel > 0 && FD_ISSET(s, &wfds))
    {
        // Verify there is no error
        int err = 0; int len = sizeof(err);
        getsockopt(s, SOL_SOCKET, SO_ERROR, reinterpret_cast<char*>(&err), &len);
        if (err == 0)
        {
            auto t1 = std::chrono::steady_clock::now();
            latMs  = static_cast<DWORD>(
                std::chrono::duration_cast<std::chrono::milliseconds>(t1 - t0).count());
            result = ConnectStatus::OK;

            // TCP handshake itself exchanges ~60-80 bytes (SYN+ACK).
            // Count those as sent regardless of the probe result.
            bytesSent = 60;

            // Send a minimal probe and wait up to 500ms for a response.
            // Many services (LDAP, SMB, RDP…) send a banner or greeting
            // on connect; others are silent until the client speaks.
            const char probe[] = "\x00"; // 1 null byte — harmless to any protocol
            int sent = ::send(s, probe, 1, 0);
            if (sent > 0) bytesSent += static_cast<DWORD>(sent);

            // Wait briefly for any response
            fd_set rfds2;
            FD_ZERO(&rfds2); FD_SET(s, &rfds2);
            timeval tvRecv { 0, 500000 }; // 500 ms
            if (select(0, &rfds2, nullptr, nullptr, &tvRecv) > 0)
            {
                char buf[512] = {};
                int r = ::recv(s, buf, sizeof(buf) - 1, 0);
                if (r > 0) bytesRecv = static_cast<DWORD>(r);
            }
        }
    }
    else if (sel == 0)
    {
        result = ConnectStatus::FAILED; // timeout
    }

    closesocket(s);
    return result;
}

// ──────────────────────────────────────────────────────────────────────────────
// UDP probe – send a 1-byte packet, wait for a response or ICMP port-unreachable.
// WSAECONNRESET (via SIO_UDP_CONNRESET) → FAILED (host refused);
// timeout                               → UNKNOWN (port may be open/filtered);
// recv data                             → OK.
// ──────────────────────────────────────────────────────────────────────────────
ConnectStatus NetworkChecker::CheckUDP(const std::wstring& ip, int port, DWORD& latMs,
                                       DWORD& bytesSent, DWORD& bytesRecv, int timeoutMs)
{
    latMs = 0; bytesSent = 0; bytesRecv = 0;

    char ipA[64] = {};
    WideCharToMultiByte(CP_ACP, 0, ip.c_str(), -1, ipA, sizeof(ipA), nullptr, nullptr);

    SOCKET s = socket(AF_INET, SOCK_DGRAM, IPPROTO_UDP);
    if (s == INVALID_SOCKET) return ConnectStatus::UNKNOWN;

    // Enable ICMP port-unreachable → WSAECONNRESET (disabled by default on Windows)
    BOOL bConnReset = TRUE;
    DWORD dwDummy   = 0;
    WSAIoctl(s, SIO_UDP_CONNRESET, &bConnReset, sizeof(bConnReset),
             nullptr, 0, &dwDummy, nullptr, nullptr);

    sockaddr_in addr {};
    addr.sin_family = AF_INET;
    addr.sin_port   = htons(static_cast<u_short>(port));
    inet_pton(AF_INET, ipA, &addr.sin_addr);

    // "Connect" sets default peer – allows recv to get ICMP errors
    connect(s, reinterpret_cast<sockaddr*>(&addr), sizeof(addr));

    const char probe[] = "\x00"; // 1-byte probe
    auto t0 = std::chrono::steady_clock::now();
    int sent = ::send(s, probe, 1, 0);
    bytesSent = (sent > 0) ? static_cast<DWORD>(sent) : 0;

    fd_set rfds;
    FD_ZERO(&rfds); FD_SET(s, &rfds);
    timeval tv { timeoutMs / 1000, (timeoutMs % 1000) * 1000 };
    int sel = select(0, &rfds, nullptr, nullptr, &tv);

    ConnectStatus result = ConnectStatus::NO_RESPONSE; // UDP timeout: no reply (open or filtered)
    if (sel > 0)
    {
        char buf[256];
        int r = recv(s, buf, sizeof(buf), 0);
        if (r >= 0)
        {
            auto t1 = std::chrono::steady_clock::now();
            latMs     = static_cast<DWORD>(
                std::chrono::duration_cast<std::chrono::milliseconds>(t1 - t0).count());
            bytesRecv = static_cast<DWORD>(r);
            result    = ConnectStatus::OK;
        }
        else
        {
            int wsaErr = WSAGetLastError();
            if (wsaErr == WSAECONNRESET || wsaErr == WSAEHOSTUNREACH ||
                wsaErr == WSAENETUNREACH)
                result = ConnectStatus::FAILED;
        }
    }

    closesocket(s);
    return result;
}

// ──────────────────────────────────────────────────────────────────────────────
// Worker thread
// ──────────────────────────────────────────────────────────────────────────────
void NetworkChecker::WorkerProc(std::vector<DestinationResult>* results,
                                ResultCb onResult, CompleteCb onComplete)
{
    // Per-server: TCP ports first, then UDP ports for each destination.
    // This gives results server-by-server rather than protocol-by-protocol.
    for (int di = 0; di < static_cast<int>(results->size()) && !m_stopReq; ++di)
    {
        auto& dr = (*results)[di];

        for (int pass = 0; pass < 2 && !m_stopReq; ++pass)
        {
            Protocol passProto = (pass == 0) ? Protocol::TCP : Protocol::UDP;
            for (int pi = 0; pi < static_cast<int>(dr.portResults.size()) && !m_stopReq; ++pi)
            {
                auto& pr = dr.portResults[pi];
                if (pr.entry.protocol != passProto) continue;

                if (!pr.enabled)
                    pr.status = ConnectStatus::DISABLED;
                else
                    pr.status = (passProto == Protocol::TCP)
                        ? CheckTCP(dr.config.ip, pr.entry.port, pr.latencyMs, pr.bytesSent, pr.bytesRecv, m_timeoutMs)
                        : CheckUDP(dr.config.ip, pr.entry.port, pr.latencyMs, pr.bytesSent, pr.bytesRecv, m_timeoutMs);

                if (onResult) onResult(di, pi);
            }
        }
        dr.completed = true;
    }

    m_running = false;
    if (onComplete) onComplete();
}

// ──────────────────────────────────────────────────────────────────────────────
// Public API
// ──────────────────────────────────────────────────────────────────────────────
void NetworkChecker::StartAsync(std::vector<DestinationResult>& results,
                                ResultCb onResult, CompleteCb onComplete)
{
    Stop();
    m_stopReq = false;
    m_running = true;
    m_thread  = std::thread(&NetworkChecker::WorkerProc, this,
                            &results, onResult, onComplete);
}

void NetworkChecker::Stop()
{
    m_stopReq = true;
    if (m_thread.joinable()) m_thread.join();
    m_running = false;
}

// ──────────────────────────────────────────────────────────────────────────────
// GetLocalIP – first non-loopback IPv4 address
// ──────────────────────────────────────────────────────────────────────────────
std::wstring NetworkChecker::GetLocalIP()
{
    char hostname[256] = {};
    gethostname(hostname, sizeof(hostname));

    addrinfo hints {}, *res = nullptr;
    hints.ai_family   = AF_INET;
    hints.ai_socktype = SOCK_STREAM;
    if (getaddrinfo(hostname, nullptr, &hints, &res) != 0 || !res)
        return L"127.0.0.1";

    char buf[INET_ADDRSTRLEN] = {};
    for (addrinfo* ai = res; ai; ai = ai->ai_next)
    {
        auto* sa4 = reinterpret_cast<sockaddr_in*>(ai->ai_addr);
        inet_ntop(AF_INET, &sa4->sin_addr, buf, sizeof(buf));
        std::string ip(buf);
        if (ip != "127.0.0.1") break;
    }
    freeaddrinfo(res);

    int needed = MultiByteToWideChar(CP_ACP, 0, buf, -1, nullptr, 0);
    std::wstring result(needed, 0);
    MultiByteToWideChar(CP_ACP, 0, buf, -1, result.data(), needed);
    if (!result.empty() && result.back() == 0) result.pop_back();
    return result;
}
