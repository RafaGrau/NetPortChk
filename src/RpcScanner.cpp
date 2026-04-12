#include "pch.h"
#include "RpcScanner.h"
#include <chrono>

// ──────────────────────────────────────────────────────────────────────────────
// ProbeTCP – single non-blocking connect
// ──────────────────────────────────────────────────────────────────────────────
ConnectStatus RpcScanner::ProbeTCP(const std::string& ipA, int port,
                                   DWORD& latMs, int timeoutMs)
{
    latMs = 0;

    SOCKET s = socket(AF_INET, SOCK_STREAM, IPPROTO_TCP);
    if (s == INVALID_SOCKET) return ConnectStatus::UNKNOWN;

    u_long mode = 1;
    ioctlsocket(s, FIONBIO, &mode);

    sockaddr_in addr{};
    addr.sin_family = AF_INET;
    addr.sin_port   = htons(static_cast<u_short>(port));
    inet_pton(AF_INET, ipA.c_str(), &addr.sin_addr);

    auto t0 = std::chrono::steady_clock::now();
    connect(s, reinterpret_cast<sockaddr*>(&addr), sizeof(addr));

    fd_set wfds, efds;
    FD_ZERO(&wfds); FD_SET(s, &wfds);
    FD_ZERO(&efds); FD_SET(s, &efds);
    timeval tv{ timeoutMs / 1000, (timeoutMs % 1000) * 1000 };

    ConnectStatus result = ConnectStatus::FAILED;
    int sel = select(0, nullptr, &wfds, &efds, &tv);
    if (sel > 0 && FD_ISSET(s, &wfds))
    {
        int err = 0; int len = sizeof(err);
        getsockopt(s, SOL_SOCKET, SO_ERROR, reinterpret_cast<char*>(&err), &len);
        if (err == 0)
        {
            auto t1 = std::chrono::steady_clock::now();
            latMs  = static_cast<DWORD>(
                std::chrono::duration_cast<std::chrono::milliseconds>(t1 - t0).count());
            result = ConnectStatus::OK;
        }
    }
    // sel == 0  → timeout (port filtered/closed), already FAILED

    closesocket(s);
    return result;
}

// ──────────────────────────────────────────────────────────────────────────────
// WorkerThread – dequeues ports and probes them
// ──────────────────────────────────────────────────────────────────────────────
void RpcScanner::WorkerThread(const std::string& ipA,
                               PortResultCb onPortResult,
                               ProgressCb   onProgress,
                               CompleteCb   /*onComplete — fired by coordinator*/)
{
    while (!m_stopReq)
    {
        int port = -1;
        {
            std::unique_lock<std::mutex> lk(m_queueMtx);
            m_queueCv.wait(lk, [this]{ return !m_queue.empty() || m_queueDone || m_stopReq; });
            if (m_stopReq) break;
            if (m_queue.empty()) break;   // queueDone and empty
            port = m_queue.front();
            m_queue.pop();
        }

        DWORD latMs = 0;
        ConnectStatus st = ProbeTCP(ipA, port, latMs, m_timeoutMs);

        int done = ++m_scanned;

        if (st == ConnectStatus::OK && onPortResult)
            onPortResult(port, st, latMs);

        if (onProgress)
            onProgress(done, m_total.load());
    }
}

// ──────────────────────────────────────────────────────────────────────────────
// Coordinator – fills queue, spawns workers, waits for completion
// ──────────────────────────────────────────────────────────────────────────────
void RpcScanner::Coordinator(const std::wstring& ip,
                              int portFrom, int portTo,
                              PortResultCb onPortResult,
                              ProgressCb   onProgress,
                              CompleteCb   onComplete)
{
    // Convert IP to narrow once
    char ipA[64]{};
    WideCharToMultiByte(CP_ACP, 0, ip.c_str(), -1, ipA, sizeof(ipA), nullptr, nullptr);

    int total = portTo - portFrom + 1;
    m_total   = total;
    m_scanned = 0;

    // Fill queue
    {
        std::lock_guard<std::mutex> lk(m_queueMtx);
        m_queueDone = false;
        while (!m_queue.empty()) m_queue.pop();
        for (int p = portFrom; p <= portTo; ++p)
            m_queue.push(p);
    }

    // Spawn workers
    int nWorkers = min(m_concurrency, total);
    m_workers.clear();
    m_workers.reserve(nWorkers);
    for (int i = 0; i < nWorkers; ++i)
        m_workers.emplace_back(&RpcScanner::WorkerThread, this,
                               std::string(ipA), onPortResult, onProgress, onComplete);

    // Signal done AFTER queue is full so workers can drain it
    {
        std::lock_guard<std::mutex> lk(m_queueMtx);
        m_queueDone = true;
    }
    m_queueCv.notify_all();

    // Wait for all workers
    for (auto& t : m_workers)
        if (t.joinable()) t.join();
    m_workers.clear();

    m_running = false;
    if (!m_stopReq && onComplete) onComplete();
}

// ──────────────────────────────────────────────────────────────────────────────
// Public API
// ──────────────────────────────────────────────────────────────────────────────
void RpcScanner::StartAsync(const std::wstring& ip,
                             int portFrom, int portTo,
                             PortResultCb onPortResult,
                             ProgressCb   onProgress,
                             CompleteCb   onComplete)
{
    Stop();
    m_stopReq = false;
    m_running = true;

    // Coordinator runs in its own thread so StartAsync returns immediately
    std::thread coord(&RpcScanner::Coordinator, this,
                      ip, portFrom, portTo,
                      onPortResult, onProgress, onComplete);
    coord.detach();
}

void RpcScanner::Stop()
{
    m_stopReq = true;
    // Wake all waiting workers so they can exit
    m_queueCv.notify_all();
    for (auto& t : m_workers)
        if (t.joinable()) t.join();
    m_workers.clear();
    m_running = false;
}
