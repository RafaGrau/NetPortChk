#pragma once
#include "AppTypes.h"
#include <functional>
#include <thread>
#include <atomic>
#include <mutex>
#include <vector>

// ──────────────────────────────────────────────────────────────────────────────
// NetworkChecker – runs TCP/UDP connectivity checks on a background thread.
//
// Callbacks are called from the worker thread; post to main window via PostMessage.
// ──────────────────────────────────────────────────────────────────────────────
class NetworkChecker
{
public:
    // onResult  (destIdx, portIdx) – one port finished
    // onComplete()                 – all ports finished
    using ResultCb   = std::function<void(int destIdx, int portIdx)>;
    using CompleteCb = std::function<void()>;

    NetworkChecker()  = default;
    ~NetworkChecker() { Stop(); }

    // Kick off async check; results written back into *results.
    // Caller must keep the vector alive until onComplete fires.
    void StartAsync(std::vector<DestinationResult>& results,
                    ResultCb   onResult,
                    CompleteCb onComplete);

    void Stop();
    bool IsRunning() const { return m_running.load(); }

    // Timeout for TCP connect and UDP probe (milliseconds). Default 2000.
    void SetTimeout(int ms) { m_timeoutMs = ms; }
    int  GetTimeout() const { return m_timeoutMs; }

    // Returns first non-loopback IPv4 address of local machine.
    static std::wstring GetLocalIP();

private:
    std::thread        m_thread;
    std::atomic<bool>  m_running  { false };
    std::atomic<bool>  m_stopReq  { false };
    int                m_timeoutMs{ 1000 };  // configurable timeout

    // Per-port check
    static ConnectStatus CheckTCP(const std::wstring& ip, int port, DWORD& latMs, DWORD& bytesSent, DWORD& bytesRecv, int timeoutMs);
    static ConnectStatus CheckUDP(const std::wstring& ip, int port, DWORD& latMs, DWORD& bytesSent, DWORD& bytesRecv, int timeoutMs);

    void WorkerProc(std::vector<DestinationResult>* results,
                    ResultCb onResult, CompleteCb onComplete);
};
