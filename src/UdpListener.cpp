#include "pch.h"
#include "UdpListener.h"
#include <cstdint>

// ──────────────────────────────────────────────────────────────────────────────
// Start
// ──────────────────────────────────────────────────────────────────────────────
int UdpListener::Start(const std::vector<int>& udpPorts,
                       PacketCb  onPacket,
                       StatusCb  onStatus)
{
    Stop();

    // Convertir m_bindIP a UTF-8 (las IPs son ASCII puro pero CP_UTF8 es
    // independiente del locale del sistema, a diferencia de CP_ACP).
    char bindA[INET_ADDRSTRLEN]{};
    bool hasBind = !m_bindIP.empty();
    if (hasBind)
    {
        if (WideCharToMultiByte(CP_UTF8, 0, m_bindIP.c_str(), -1,
                                 bindA, sizeof(bindA), nullptr, nullptr) == 0)
            hasBind = false;
    }

    std::vector<SOCKET> sockets;
    std::vector<int>    boundPorts;
    int                 skipCount = 0;

    for (int port : udpPorts)
    {
        SOCKET s = socket(AF_INET, SOCK_DGRAM, IPPROTO_UDP);
        if (s == INVALID_SOCKET) { ++skipCount; continue; }

        BOOL reuse = TRUE;
        setsockopt(s, SOL_SOCKET, SO_REUSEADDR,
                   reinterpret_cast<const char*>(&reuse), sizeof(reuse));

        // Poner en modo no bloqueante
        u_long nb = 1;
        ioctlsocket(s, FIONBIO, &nb);

        sockaddr_in addr{};
        addr.sin_family = AF_INET;
        addr.sin_port   = htons(static_cast<u_short>(port));
        if (hasBind)
        {
            if (inet_pton(AF_INET, bindA, &addr.sin_addr) != 1)
            {
                closesocket(s);
                ++skipCount;
                continue;
            }
        }
        else
            addr.sin_addr.s_addr = INADDR_ANY;

        if (bind(s, reinterpret_cast<sockaddr*>(&addr), sizeof(addr)) == SOCKET_ERROR)
        {
            closesocket(s);
            ++skipCount;
            continue;
        }

        sockets.push_back(s);
        boundPorts.push_back(port);
    }

    int n = static_cast<int>(sockets.size());
    m_activePorts.store(n);

    if (n == 0)
    {
        if (onStatus)
            onStatus(L"Listener: ningún puerto disponible. "
                     L"Ejecute como administrador para puertos privilegiados.");
        return 0;
    }

    // IP local para filtrar paquetes propios (el SO puede enviar datagramas a sí mismo)
    char localIPFilter[INET_ADDRSTRLEN]{};
    if (hasBind)
        strncpy_s(localIPFilter, bindA, sizeof(localIPFilter) - 1);

    if (onStatus)
    {
        std::wstring nic = hasBind
            ? (std::wstring(L" en NIC ") + m_bindIP)
            : L" en todas las interfaces";
        std::wstring msg = L"Escuchando " + std::to_wstring(n)
                         + L" puerto(s) UDP" + nic;
        if (skipCount > 0)
            msg += L" (" + std::to_wstring(skipCount) + L" no disponibles)";
        onStatus(msg);
    }

    m_stopReq = false;
    m_running  = true;
    m_thread   = std::thread(&UdpListener::WorkerProc, this,
                             std::move(sockets), std::move(boundPorts),
                             onPacket, onStatus, std::string(localIPFilter));
    return n;
}

// ──────────────────────────────────────────────────────────────────────────────
// Stop
// ──────────────────────────────────────────────────────────────────────────────
void UdpListener::Stop()
{
    m_stopReq = true;
    if (m_thread.joinable()) m_thread.join();
    m_running     = false;
    m_activePorts = 0;
}

// ──────────────────────────────────────────────────────────────────────────────
// WorkerProc – usa select() con lotes para evitar límites de WSAPoll
// ──────────────────────────────────────────────────────────────────────────────
void UdpListener::WorkerProc(std::vector<SOCKET> sockets,
                              std::vector<int>    ports,
                              PacketCb onPacket,
                              StatusCb onStatus,
                              std::string localIPFilter)
{
    const int BATCH = 64;   // sockets por lote — bien por debajo de FD_SETSIZE(64)
    char buf[4096];

    while (!m_stopReq)
    {
        bool anyActivity = false;

        // Procesar en lotes de BATCH sockets con select()
        for (size_t base = 0; base < sockets.size() && !m_stopReq; base += BATCH)
        {
            size_t end = min(base + BATCH, sockets.size());

            fd_set rset; FD_ZERO(&rset);
            for (size_t i = base; i < end; ++i) FD_SET(sockets[i], &rset);

            timeval tv{ 0, 5000 };   // 5 ms por lote
            int ret = select(0, &rset, nullptr, nullptr, &tv);
            if (ret <= 0) continue;

            anyActivity = true;
            for (size_t i = base; i < end; ++i)
            {
                if (!FD_ISSET(sockets[i], &rset)) continue;

                sockaddr_in from{}; int fromLen = sizeof(from);
                int r = recvfrom(sockets[i], buf, sizeof(buf)-1, 0,
                                 reinterpret_cast<sockaddr*>(&from), &fromLen);
                if (r < 0) continue;

                // Anti-amplificación / anti-loop:
                // Sólo respondemos con eco si el origen está en el espacio
                // de direcciones privadas o loopback. Esto impide que la
                // app sea usada como reflector UDP por atacantes que
                // falsifican el origen para apuntar a una víctima pública.
                auto isPrivateOrLoopback = [](const sockaddr_in& a) -> bool {
                    uint32_t ip = ntohl(a.sin_addr.s_addr);
                    return (ip & 0xFF000000) == 0x0A000000   // 10.0.0.0/8
                        || (ip & 0xFFF00000) == 0xAC100000   // 172.16.0.0/12
                        || (ip & 0xFFFF0000) == 0xC0A80000   // 192.168.0.0/16
                        || (ip & 0xFFFF0000) == 0xA9FE0000   // 169.254.0.0/16 (link-local)
                        || (ip & 0xFF000000) == 0x7F000000;  // 127.0.0.0/8
                };

                if (isPrivateOrLoopback(from))
                {
                    const char* echo   = (r > 0) ? buf : "\x01";
                    int         echLen = (r > 0) ? r   : 1;
                    sendto(sockets[i], echo, echLen, 0,
                           reinterpret_cast<sockaddr*>(&from), fromLen);
                }

                if (r == 0) continue;

                // IP origen → wstring
                char ipA[INET_ADDRSTRLEN]{};
                inet_ntop(AF_INET, &from.sin_addr, ipA, sizeof(ipA));

                // Ignorar paquetes cuyo origen es la propia IP local
                // (se producen cuando el SO hace loopback de datagramas propios)
                if (!localIPFilter.empty() && strcmp(ipA, localIPFilter.c_str()) == 0)
                    continue;

                int needed = MultiByteToWideChar(CP_UTF8, 0, ipA, -1, nullptr, 0);
                if (needed <= 0) continue;
                std::wstring senderW(needed, L'\0');
                MultiByteToWideChar(CP_UTF8, 0, ipA, -1, senderW.data(), needed);
                if (!senderW.empty() && senderW.back() == L'\0') senderW.pop_back();

                auto* pkt      = new ListenPacket();
                pkt->port      = ports[i];
                pkt->senderPort= ntohs(from.sin_port);
                pkt->senderIP  = std::move(senderW);
                pkt->bytes     = static_cast<DWORD>(r);
                GetLocalTime(&pkt->time);

                if (onPacket) onPacket(pkt);
            }
        }

        // Si ningún lote tuvo actividad, dormir un poco antes del siguiente ciclo
        if (!anyActivity) Sleep(1);
    }

    for (SOCKET s : sockets) closesocket(s);

    m_running     = false;
    m_activePorts = 0;

    if (onStatus) onStatus(L"Listener detenido.");
}
