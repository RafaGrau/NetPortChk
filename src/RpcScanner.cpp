#include "pch.h"
#include "RpcScanner.h"
#include <chrono>

// ──────────────────────────────────────────────────────────────────────────────
// ProbeTCP – single non-blocking connect
// ──────────────────────────────────────────────────────────────────────────────
ConnectStatus RpcScanner::ProbeTCP(const std::string& ipA, int port,
                                   DWORD& latMs, int timeoutMs,
                                   const std::string& bindIP)
{
    latMs = 0;

    SOCKET s = socket(AF_INET, SOCK_STREAM, IPPROTO_TCP);
    if (s == INVALID_SOCKET) return ConnectStatus::UNKNOWN;

    // Bind to specific local NIC if requested
    if (!bindIP.empty())
    {
        sockaddr_in local{};
        local.sin_family = AF_INET;
        local.sin_port   = 0;
        if (inet_pton(AF_INET, bindIP.c_str(), &local.sin_addr) != 1)
        {
            closesocket(s);
            return ConnectStatus::UNKNOWN;
        }
        if (bind(s, reinterpret_cast<sockaddr*>(&local), sizeof(local)) == SOCKET_ERROR)
        {
            closesocket(s);
            return ConnectStatus::UNKNOWN;
        }
    }

    u_long mode = 1;
    ioctlsocket(s, FIONBIO, &mode);

    sockaddr_in addr{};
    addr.sin_family = AF_INET;
    addr.sin_port   = htons(static_cast<u_short>(port));
    if (inet_pton(AF_INET, ipA.c_str(), &addr.sin_addr) != 1)
    {
        closesocket(s);
        return ConnectStatus::UNKNOWN;
    }

    auto t0 = std::chrono::steady_clock::now();
    connect(s, reinterpret_cast<sockaddr*>(&addr), sizeof(addr));

    fd_set wfds, efds;
    FD_ZERO(&wfds); FD_SET(s, &wfds);
    FD_ZERO(&efds); FD_SET(s, &efds);
    // Clamp: select() con timeout negativo es UB.
    int tMs = (timeoutMs <= 0) ? 500 : timeoutMs;
    timeval tv{ tMs / 1000, (tMs % 1000) * 1000 };

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
        ConnectStatus st;
        ScanProto     foundProto = ScanProto::TCP;

        switch (m_proto)
        {
        case ScanProto::UDP:
            st = ProbeUDP(ipA, port, latMs, m_timeoutMs, m_bindIP);
            foundProto = ScanProto::UDP;
            break;
        case ScanProto::Both:
            st = ProbeTCP(ipA, port, latMs, m_timeoutMs, m_bindIP);
            foundProto = ScanProto::TCP;
            if (st != ConnectStatus::OK)
            {
                DWORD latUdp = 0;
                ConnectStatus stU = ProbeUDP(ipA, port, latUdp, m_timeoutMs, m_bindIP);
                if (stU == ConnectStatus::OK) { st = stU; latMs = latUdp; foundProto = ScanProto::UDP; }
            }
            break;
        default:
            st = ProbeTCP(ipA, port, latMs, m_timeoutMs, m_bindIP);
            foundProto = ScanProto::TCP;
            break;
        }

        int done = ++m_scanned;

        if (st == ConnectStatus::OK && onPortResult)
            onPortResult(port, st, latMs, foundProto);

        // Coalescer: emitir progreso cada 32 puertos o al completar.
        // Antes generábamos un PostMessage por cada puerto escaneado, lo
        // que con 16 384 puertos saturaba la cola de mensajes UI.
        if (onProgress)
        {
            int total = m_total.load();
            if (done == total || (done & 0x1F) == 0)
                onProgress(done, total);
        }
    }
}

// ──────────────────────────────────────────────────────────────────────────────
// ProbeUDP – envía un datagrama y espera respuesta (eco del listener)
// ──────────────────────────────────────────────────────────────────────────────
ConnectStatus RpcScanner::ProbeUDP(const std::string& ipA, int port,
                                   DWORD& latMs, int timeoutMs,
                                   const std::string& bindIP)
{
    latMs = 0;

    SOCKET s = socket(AF_INET, SOCK_DGRAM, IPPROTO_UDP);
    if (s == INVALID_SOCKET) return ConnectStatus::UNKNOWN;

    if (!bindIP.empty())
    {
        sockaddr_in local{};
        local.sin_family = AF_INET;
        local.sin_port   = 0;
        if (inet_pton(AF_INET, bindIP.c_str(), &local.sin_addr) != 1)
        {
            closesocket(s);
            return ConnectStatus::UNKNOWN;
        }
        if (bind(s, reinterpret_cast<sockaddr*>(&local), sizeof(local)) == SOCKET_ERROR)
        {
            closesocket(s);
            return ConnectStatus::UNKNOWN;
        }
    }

    sockaddr_in dest{};
    dest.sin_family = AF_INET;
    dest.sin_port   = htons(static_cast<u_short>(port));
    if (inet_pton(AF_INET, ipA.c_str(), &dest.sin_addr) != 1)
    {
        closesocket(s);
        return ConnectStatus::UNKNOWN;
    }

    const char probe[] = "\x01";  // 1 byte de sonda

    auto t0 = std::chrono::steady_clock::now();

    if (sendto(s, probe, 1, 0,
               reinterpret_cast<sockaddr*>(&dest), sizeof(dest)) == SOCKET_ERROR)
    {
        closesocket(s);
        return ConnectStatus::FAILED;
    }

    // Esperar respuesta
    fd_set rset; FD_ZERO(&rset); FD_SET(s, &rset);
    // Clamp: select() con timeout negativo es UB.
    int tMs = (timeoutMs <= 0) ? 500 : timeoutMs;
    timeval tv{ tMs / 1000, (tMs % 1000) * 1000 };
    int sel = select(0, &rset, nullptr, nullptr, &tv);

    ConnectStatus result = ConnectStatus::FAILED;
    if (sel > 0)
    {
        char buf[256];
        sockaddr_in from{}; int fromLen = sizeof(from);
        if (recvfrom(s, buf, sizeof(buf), 0,
                     reinterpret_cast<sockaddr*>(&from), &fromLen) >= 0)
        {
            auto t1 = std::chrono::steady_clock::now();
            latMs   = static_cast<DWORD>(
                std::chrono::duration_cast<std::chrono::milliseconds>(t1 - t0).count());
            result  = ConnectStatus::OK;
        }
    }

    closesocket(s);
    return result;
}

// ──────────────────────────────────────────────────────────────────────────────
// ──────────────────────────────────────────────────────────────────────────────
void RpcScanner::Coordinator(const std::wstring& ip,
                              int portFrom, int portTo,
                              PortResultCb onPortResult,
                              ProgressCb   onProgress,
                              CompleteCb   onComplete)
{
    // Convert IP to narrow once (UTF-8; las IPs son ASCII puro)
    char ipA[INET_ADDRSTRLEN]{};
    if (WideCharToMultiByte(CP_UTF8, 0, ip.c_str(), -1,
                             ipA, sizeof(ipA), nullptr, nullptr) == 0)
    {
        m_running = false;
        if (onComplete) onComplete();
        return;
    }

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

    // NOTE: m_running se actualiza exclusivamente en Stop() para evitar
    // condiciones de carrera con Stop() llamado desde el hilo UI.
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

    // El coordinador se posee como miembro: Stop() puede join-earlo de forma
    // segura sin riesgo de doble-join sobre los workers (lo hace el coord).
    m_coordThread = std::thread(&RpcScanner::Coordinator, this,
                                 ip, portFrom, portTo,
                                 onPortResult, onProgress, onComplete);
}

void RpcScanner::Stop()
{
    m_stopReq = true;
    // Despertar a todos los workers que estén en wait()
    m_queueCv.notify_all();

    // Joinear el coordinador — éste a su vez joinea los workers.
    // Sólo el coordinador toca m_workers, evitando carreras.
    if (m_coordThread.joinable()) m_coordThread.join();

    m_running = false;
}
