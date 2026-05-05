#pragma once
#include "AppTypes.h"
#include <functional>
#include <thread>
#include <atomic>
#include <vector>

// ──────────────────────────────────────────────────────────────────────────────
// UdpListener – escucha UDP en un conjunto de puertos.
//
// Compatibilidad: no usa SO_EXCLUSIVEADDRUSE.  Si un puerto ya está en uso
// se omite silenciosamente; sólo se abre en los que estén libres.
// Usa WSAPoll() para multiplexar todos los sockets en un único hilo.
//
// Los callbacks se invocan desde el hilo worker; el caller debe
// PostMessage(WM_LISTEN_PKT) en lugar de tocar la UI directamente.
// ──────────────────────────────────────────────────────────────────────────────
class UdpListener
{
public:
    // pkt: paquete recibido (heap-allocated, el receptor hace delete)
    using PacketCb  = std::function<void(ListenPacket* pkt)>;
    // msg: texto informativo (puertos activos, errores, …)
    using StatusCb  = std::function<void(const std::wstring& msg)>;

    UdpListener()  = default;
    ~UdpListener() { Stop(); }

    // Inicia la escucha en los puertos UDP indicados.
    // Devuelve el número de puertos donde se pudo hacer bind (≥ 0).
    int  Start(const std::vector<int>& udpPorts,
               PacketCb  onPacket,
               StatusCb  onStatus);

    void Stop();
    bool IsRunning()   const { return m_running.load(); }
    int  ActivePorts() const { return m_activePorts.load(); }

    // Local NIC IP for bind (empty = INADDR_ANY)
    void SetBindIP(const std::wstring& ip) { m_bindIP = ip; }

private:
    std::thread       m_thread;
    std::atomic<bool> m_running    { false };
    std::atomic<bool> m_stopReq   { false };
    std::atomic<int>  m_activePorts{ 0 };
    std::wstring      m_bindIP;   // local NIC IP, empty = INADDR_ANY

    void WorkerProc(std::vector<SOCKET> sockets,
                    std::vector<int>    ports,
                    PacketCb onPacket, StatusCb onStatus,
                    std::string localIPFilter);
};
