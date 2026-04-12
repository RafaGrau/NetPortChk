#pragma once
#include "AppTypes.h"
#include <functional>
#include <thread>
#include <atomic>
#include <mutex>
#include <vector>
#include <queue>
#include <condition_variable>

// ──────────────────────────────────────────────────────────────────────────────
// RpcScanResult – result for one port in an RPC range scan
// ──────────────────────────────────────────────────────────────────────────────
struct RpcScanResult
{
    int           port      { 0 };
    ConnectStatus status    { ConnectStatus::PENDING };
    DWORD         latencyMs { 0 };
};

// ──────────────────────────────────────────────────────────────────────────────
// RpcScanner
// Parallel TCP scanner for port ranges (designed for RPC dynamic ports
// 49152-65535 but works on any range).
//
// Uses a thread pool of up to 'concurrency' worker threads each running
// non-blocking connect() + select(), so hundreds of simultaneous probes
// can be in flight without one-per-port OS threads.
//
// Callbacks fire from worker threads — caller must PostMessage to UI.
// ──────────────────────────────────────────────────────────────────────────────
class RpcScanner
{
public:
    // portResult(port, status, latencyMs) — called once per open port
    // progress(scanned, total)            — called after every probe
    // complete()                          — all ports done
    using PortResultCb = std::function<void(int port, ConnectStatus status, DWORD latMs)>;
    using ProgressCb   = std::function<void(int scanned, int total)>;
    using CompleteCb   = std::function<void()>;

    RpcScanner()  = default;
    ~RpcScanner() { Stop(); }

    void SetTimeout    (int ms)          { m_timeoutMs   = ms;          }
    void SetConcurrency(int n)           { m_concurrency = max(1, min(n, 500)); }
    int  GetTimeout()     const          { return m_timeoutMs;           }
    int  GetConcurrency() const          { return m_concurrency;         }

    // Start async scan of [portFrom, portTo] TCP on 'ip'.
    void StartAsync(const std::wstring& ip,
                    int portFrom, int portTo,
                    PortResultCb onPortResult,
                    ProgressCb   onProgress,
                    CompleteCb   onComplete);

    void Stop();
    bool IsRunning() const { return m_running.load(); }

    // Total ports queued in last StartAsync call
    int  Total()   const { return m_total.load(); }
    int  Scanned() const { return m_scanned.load(); }

private:
    std::vector<std::thread> m_workers;
    std::atomic<bool>  m_running  { false };
    std::atomic<bool>  m_stopReq  { false };
    std::atomic<int>   m_total    { 0 };
    std::atomic<int>   m_scanned  { 0 };

    int  m_timeoutMs   { 500 };
    int  m_concurrency { 200 };

    // Work queue
    std::queue<int>        m_queue;
    std::mutex             m_queueMtx;
    std::condition_variable m_queueCv;
    bool                   m_queueDone { false };

    static ConnectStatus ProbeTCP(const std::string& ipA, int port,
                                  DWORD& latMs, int timeoutMs);

    void WorkerThread(const std::string& ipA,
                      PortResultCb onPortResult,
                      ProgressCb   onProgress,
                      CompleteCb   onComplete);

    void Coordinator(const std::wstring& ip,
                     int portFrom, int portTo,
                     PortResultCb onPortResult,
                     ProgressCb   onProgress,
                     CompleteCb   onComplete);
};
