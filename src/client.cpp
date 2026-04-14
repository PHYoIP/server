/*
author          Oliver Blaser
date            13.04.2026
copyright       GPL-3.0 - Copyright (c) 2026 Oliver Blaser
*/

#include <cstddef>
#include <cstdint>
#include <ctime>
#include <string>

#include "client.h"
#include "common/ansi-esc.h"
#include "common/packet.h"
#include "common/socket.h"
#include "project.h"
#include "server.h"
#include "util/macros.h"
#include "util/time.h"

#include <phyoip/protocol/cmp.h>
#include <phyoip/protocol/phyoip.h>
#include <phyoip/protocol/uart.h>

#ifdef _WIN32

#include <io.h>
#include <sys/types.h>
#include <WinSock2.h>
#include <WS2tcpip.h>

#include <Windows.h>

#else // *nix

#include <netinet/in.h>
#include <sys/socket.h>
#include <unistd.h>

#endif // Windows / *nix


#define LOG_MODULE_LEVEL LOG_LEVEL_DBG
#define LOG_MODULE_NAME  CNT
#include "common/log.h"



#define TRIGF_VALUE_MASK     (0x000000FF)
#define TRIGF_SEND_ACK       (0x80000000)
#define TRIGF_SEND_PEER_INFO (0x40000000)
#define TRIGF_PERFORM_DELIST (0x20000000)



enum
{
    S_init = 0,

    S_idle,

    S_terminate,
    S_kill,
};



int server::client::Client::reset(server::Server* srv, sockfd_t connfd, const std::string& addr, uint16_t port)
{
    const auto status = sd.status();
    if ((status != thread::Status::null) && (status != thread::Status::killed))
    {
        LOG_ERR("can't reset running client");
        return -(__LINE__);
    }

    sd.reset();

    m_srv = srv;
    m_connfd = connfd;
    m_idstr = addr + ':' + std::to_string(port);
    m_port = port;
    m_registered = false;
    m_triggerFlags = 0;

    m_rxIdx = 0;
    m_txCount = 0;
    m_txIdx = 0;

    return 0;
}

void server::client::Client::m_task()
{
    int state = S_init;
    bool registered = false;

    const time_t tThreadStart = time(nullptr);

    sd.setStatus(thread::Status::booting);

    while (!sd.sigkill())
    {
        const time_t tNow = time(nullptr);

        switch (state)
        {
        case S_init:
        {
            LOG_DBG("started thread for client %s", m_idstr.c_str());

            int err;
#ifdef _WIN32
            unsigned long ul = 1;
            err = ioctlsocket(m_connfd, FIONBIO, &ul);
            if (err) { LOG_ERR_WSA("ioctlsocket(FIONBIO) failed", WSAGetLastError()); }
#else
            int flags = fcntl(m_connfd, F_GETFL);
            err = fcntl(m_connfd, F_SETFL, flags | O_NONBLOCK);
            if (err) { LOG_ERR_ERRNO("fcntl(O_NONBLOCK) failed", errno); }
#endif

            if (err)
            {
                sd.setError(-(__LINE__));
                state = S_terminate;
            }
            else
            {
                sd.setStatus(thread::Status::running);
                state = S_idle;
            }
        }
        break;

        case S_idle:
        {
            int err;

            err = m_taskRecv();
            if (err)
            {
                if (err != 1) { sd.setError(err); }
                state = S_terminate;
            }

            err = m_taskSend();
            if (err)
            {
                sd.setError(err);
                state = S_terminate;
            }

            if ((m_triggerFlags & TRIGF_SEND_ACK) && !m_txBusy())
            {
                const uint8_t ackStatus = (uint8_t)m_triggerFlags;
                const ssize_t res = packet::serialise::cmpAck(m_txBuffer, sizeof(m_txBuffer), ackStatus);
                if (res > 0)
                {
                    m_txCount = (size_t)res;
                    m_registered = (ackStatus == PHYOIP_ACK_OK);
                    m_triggerFlags &= ~(TRIGF_SEND_ACK);
                }
                else
                {
                    sd.setError(-(__LINE__));
                    state = S_terminate;
                }
            }

            if ((m_triggerFlags & TRIGF_SEND_PEER_INFO) && !m_txBusy())
            {
                LOG_ERR("TODO implement send peer info");
                // packet::serialise::cmpPeerInfo(duf, bufsz, prj::appName, "PHYoIP server sample implementation", PRJ_VERSION_MAJ, PRJ_VERSION_MIN,
                // PRJ_VERSION_STR);
                // static_assert((PRJ_VERSION_MAJ <= UINT8_MAX) && (PRJ_VERSION_MIN <= UINT8_MAX));

                m_triggerFlags &= ~(TRIGF_SEND_PEER_INFO);
            }

            if (sd.packetQueued() && !m_txBusy())
            {
                const auto packet = sd.pop();
                const ssize_t res = packet::serialise::phyoip(m_txBuffer, sizeof(m_txBuffer), PHYOIP_PROTO_UART, packet.data(), packet.size());
                if (res > 0)
                {
                    m_txCount = (size_t)res;
                    // LOG_DBG_HD(m_txBuffer, m_txCount, "send packet:");
                }
                else
                {
                    sd.setError(-(__LINE__));
                    state = S_terminate;
                }
            }

            if (m_triggerFlags & TRIGF_PERFORM_DELIST)
            {
                m_registered = false;
                state = S_terminate;
            }

            if (!m_registered && ((tNow - tThreadStart) >= registerTimeout))
            {
                LOG_ERR("register timeout on client %s", m_idstr.c_str());
                sd.setError(-(__LINE__));
                state = S_terminate;
            }

            if (sd.sigterm())
            {
                LOG_INF("terminating");
                state = S_terminate;
            }
        }
        break;

        case S_terminate:

            sd.setStatus(thread::Status::terminating);

            LOG_DBG("terminating thread for client %s", m_idstr.c_str());

            if (m_registered)
            {
                LOG_WRN("TODO send PHYOIP_CMP_DELIST");
                // send PHYOIP_CMP_DELIST
                // wait for client to close the connection (poll recv()) or timeout
            }

            // close socket
            if (0 != sockclose(m_connfd))
            {
                LOG_ERR_SOCKET("close(connfd) failed");
                if (!sd.error()) { sd.setError(-(__LINE__)); }
            }

            m_srv->delistClient(this);

            // exit/kill thread
            state = S_kill;

            break;

        case S_kill:
            sd.kill();
            break;

        default:
            LOG_ERR("invalid state: %i", state);
            UTIL_sleep_ms(5000);
            break;
        }

        UTIL_sleep_us(100);
    }

    LOG_DBG("exited thread for client %s (err: %i)", m_idstr.c_str(), sd.error());
    sd.setStatus(thread::Status::killed);
}

/**
 * @return 0 on success, negative on error, 1 if an unregistered client closed the connection
 */
int server::client::Client::m_taskRecv()
{
    uint8_t buffer[512];

    const ssize_t res = recv(m_connfd, (char*)buffer,
#if 0 // testing chunk parser
                             (size_t)(5 + (rand() % 10)),
#else
                             sizeof(buffer),
#endif
                             0);
    if (res == 0)
    {
        if (m_registered)
        {
            LOG_ERR("client %s closed the connection unexpectedly", m_idstr.c_str());
            return -(__LINE__);
        }

        return 1; // this return code needs to be checked at `m_handleReceivedChunk()`
    }
    else if (res < 0)
    {
        if (
#ifdef _WIN32
            (WSAGetLastError() != WSAEWOULDBLOCK)
#else
            (errno != EAGAIN) && (errno != EWOULDBLOCK)
#endif
        )
        {
            LOG_ERR_SOCKET("recv() failed");
            return -(__LINE__);
        }
    }
    else
    {
        const int err = m_handleReceivedChunk(buffer, (size_t)res);
        if (err == 1)
        {
            LOG_ERR("m_handleReceivedChunk() returned %i", err);
            return -(__LINE__);
        }
        else if (err) { return err; }
    }

    return 0;
}

int server::client::Client::m_taskSend()
{
    if (m_txCount)
    {
        const ssize_t res = send(m_connfd, (char*)m_txBuffer + m_txIdx, m_txCount, 0); // on MSW `txCount` gets implicitly cast to `int`
        if (res < 0)
        {
            if (
#ifdef _WIN32
                (WSAGetLastError() != WSAEWOULDBLOCK)
#else
                (errno != EAGAIN) && (errno != EWOULDBLOCK)
#endif
            )
            {
                LOG_ERR_SOCKET("send() failed");
                if (0 != sockclose(m_connfd)) { LOG_ERR_SOCKET("close(connfd) failed"); }
                return -(__LINE__);
            }
        }
        else
        {
            size_t n = (size_t)res;
            if (n > m_txCount)
            {
                LOG_ERR("written %zu of %zu bytes", n, m_txCount);
                n = m_txCount; // clamp for correct math below
            }

            m_txIdx += n;
            m_txCount -= n;
        }
    }
    else { m_txIdx = 0; }

    return 0;
}

int server::client::Client::m_handleReceivedChunk(const uint8_t* data, size_t count)
{
    // LOG_DBG_HD(buffer, (size_t)res, "received chunk:");

    const uint8_t* p = data;

    while (p < (data + count))
    {
        if (m_rxIdx >= SIZEOF_ARRAY(m_rxBuffer))
        {
            LOG_ERR("receive buffer overflow on client %s", m_idstr.c_str());
            return -(__LINE__);
        }

        m_rxBuffer[m_rxIdx++] = *(p++);



        const struct phyoiphdr* const hdr = (const struct phyoiphdr*)(m_rxBuffer);

        if (m_rxIdx == sizeof(struct phyoiphdr))
        {
            if (!packet::isCompatible(hdr))
            {
                LOG_ERR("received incompatible packet (v%i.%i) from client %s", (int)(hdr->vermaj), (int)(hdr->vermin), m_idstr.c_str());
                return -(__LINE__);
            }
        }

        if (m_rxIdx == packet::size(m_rxBuffer, m_rxIdx))
        {
            const int err = m_handleReceivedPacket(m_rxBuffer, m_rxIdx);
            if (err) { return err; }

            m_rxIdx = 0;
        }
    }

    return 0;
}

int server::client::Client::m_handleReceivedPacket(const uint8_t* data, size_t count)
{
    // LOG_DBG_HD(data, count, "received packet:");

    const struct phyoiphdr* const hdr = (const struct phyoiphdr*)(m_rxBuffer);
    const size_t hsize = hdr->hsize;
    const uint8_t proto = hdr->proto;

    if (proto == PHYOIP_PROTO_CMP) { return m_handleCmp(data + hsize, count - hsize); }
    else if (proto == PHYOIP_PROTO_UART) { return m_handleUart(data + hsize, count - hsize); }

    LOG_ERR("can't handle packet with protocol 0x%02i from client %s", (int)proto, m_idstr.c_str());
    return -(__LINE__);
}

int server::client::Client::m_handleCmp(const uint8_t* data, size_t count)
{
    if (count < sizeof(struct phyoip_cmphdr))
    {
        LOG_ERR("received invalid CMP packet (%zu bytes) from client %s", count, m_idstr.c_str());
        return -(__LINE__);
    }

    const struct phyoip_cmphdr* const hdr = (const struct phyoip_cmphdr*)(data);
    const size_t hsize = hdr->hsize;
    const size_t dsize = ntohs(hdr->dsize);
    const uint8_t type = hdr->type;

    if (count < (hsize + dsize))
    {
        LOG_ERR("dropping invalid CMP packet from client %s: size (%zu) < hsize (%zu) + dsize (%zu)", m_idstr.c_str(), count, hsize, dsize);
        LOG_DBG_HD(data, count, "data:");
        return 0;
    }

    if (type == PHYOIP_CMP_REQPEERINFO) { m_triggerFlags |= TRIGF_SEND_PEER_INFO; }
    else if (type == PHYOIP_CMP_PEERINFO) { (void)0; /* dropping peer info package */ }
    else if (type == PHYOIP_CMP_REGISTER) { return m_handleCmpRegister(data + hsize, count - hsize); }
    else if (type == PHYOIP_CMP_ACK)
    {
        LOG_ERR("received CMP ACK packet from client %s", m_idstr.c_str());
        return -(__LINE__);
    }
    else if (type == PHYOIP_CMP_DELIST) { m_triggerFlags |= TRIGF_PERFORM_DELIST; }
    else if (type == PHYOIP_CMP_XENV) { (void)0; /* dropping extension envelope package */ }
    else { LOG_INF("dropping CMP packet with unknown type 0x%02x from client %s", (int)type, m_idstr.c_str()); }

    return 0;
}

int server::client::Client::m_handleCmpRegister(const uint8_t* data, size_t count)
{
    if (count < sizeof(struct phyoip_cmpreg))
    {
        LOG_ERR("received invalid CMP register packet (%zu bytes) from client %s", count, m_idstr.c_str());
        return -(__LINE__);
    }

    const struct phyoip_cmpreg* const cmpreg = (const struct phyoip_cmpreg*)(data);
    const uint8_t proto = cmpreg->proto;
    const uint16_t clitype = ntohs(cmpreg->clitype);

    m_triggerFlags &= ~(TRIGF_VALUE_MASK);

    const bool positiveFeedbackLoop =
        (((clitype & PHYOIP_CTPERM_RE) && (clitype & PHYOIP_CTPERM_WE)) || // depending on the implementations this could flood and overflow all buffers
         ((clitype & PHYOIP_CTPERM_RI) && (clitype & PHYOIP_CTPERM_WI)));  // it has no practical meaning anyway, so just block it

    if (proto != PHYOIP_PROTO_UART) { m_triggerFlags |= (TRIGF_SEND_ACK | PHYOIP_ACK_PROTO); }
    else if (positiveFeedbackLoop) { m_triggerFlags |= (TRIGF_SEND_ACK | PHYOIP_ACK_ERROR); }
    else if ((clitype & PHYOIP_CTPERM_MASK) == PHYOIP_CTPERM_NONE) { m_triggerFlags |= (TRIGF_SEND_ACK | PHYOIP_ACK_ERROR); }
    else
    {
        const int err = m_srv->registerClient(this, clitype);
        if (err) { m_triggerFlags |= (TRIGF_SEND_ACK | PHYOIP_ACK_PERM); }
        else { m_triggerFlags |= (TRIGF_SEND_ACK | PHYOIP_ACK_OK); }
    }

    return 0;
}

int server::client::Client::m_handleUart(const uint8_t* data, size_t count)
{
    // LOG_DBG_HD(data, count, "UART packet:");

    if (count < sizeof(struct phyoip_uarthdr))
    {
        LOG_ERR("received invalid UART packet (%zu bytes) from client %s", count, m_idstr.c_str());
        return -(__LINE__);
    }

    const struct phyoip_uarthdr* const hdr = (const struct phyoip_uarthdr*)(data);
    const size_t hsize = hdr->hsize;
    const size_t dsize = ntohs(hdr->dsize);

    if (count < (hsize + dsize))
    {
        LOG_ERR("received invalid UART packet from client %s: size (%zu) < hsize (%zu) + dsize (%zu)", m_idstr.c_str(), count, hsize, dsize);
        return -(__LINE__);
    }

    m_srv->pushPacket(std::vector<uint8_t>(data, data + count));

    return 0;
}
