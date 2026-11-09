/*
author          Oliver Blaser
date            09.11.2026
copyright       GPL-3.0 - Copyright (c) 2026 Oliver Blaser
*/

#include <cstddef>
#include <cstdint>
#include <ctime>
#include <mutex>
#include <string>
#include <thread>

#include "client.h"
#include "common/ansi-esc.h"
#include "common/socket.h"
#include "common/thread.h"
#include "server.h"
#include "util/macros.h"
#include "util/time.h"

#include <phyoip/protocol/cmp.h>
#include <phyoip/protocol/ports.h>
#include <phyoip/protocol/uart.h>

#ifdef _WIN32

#include <io.h>
#include <sys/types.h>
#include <WinSock2.h>
#include <WS2tcpip.h>

#include <Windows.h>

#else // *nix

#include <arpa/inet.h>
#include <fcntl.h>
#include <netinet/in.h>
#include <sys/socket.h>
#include <unistd.h>

#endif // Windows / *nix


#define LOG_MODULE_LEVEL LOG_LEVEL_DBG
#define LOG_MODULE_NAME  SRV
#include "common/log.h"



enum
{
    S_init = 0,

    S_open,
    S_bind,
    S_listen,
    S_ready,

    S_idle,

    S_terminate,
    S_kill,
};



static std::string getPeerInfoDescr(int argc = 0, char** argv = nullptr);



void server::Server::task(Server* srv)
{
    time_t tTerminate = 0;

    srv->sd.setStatus(thread::Status::booting);
    srv->state = S_init;

    while (!srv->sd.sigkill())
    {
        const time_t tNow = time(nullptr);

        switch (srv->state)
        {
        case S_init:
            if (srv->m_gwc)
            {
                // wait for gateway client to open the serial port
                if ((srv->m_gwc->sd.status() != thread::Status::null) && (srv->m_gwc->sd.status() != thread::Status::booting)) { srv->state = S_open; }

                // last, highest precedence
                if (srv->sd.sigterm())
                {
                    // sockfd is not set, so go to S_kill, there are no clients to be terminated either
                    srv->state = S_kill;
                }
            }
            else { srv->state = S_open; }
            break;

        case S_open:
            srv->sockfd = socket(AF_INET, SOCK_STREAM, IPPROTO_TCP); // TODO non blocking (quick ref: https://www.scottklement.com/rpg/socktut/nonblocking.html)
            if (srv->sockfd < 0)
            {
                LOG_ERR_SOCKET("socket() failed");
                srv->sd.setError(-(__LINE__));
                srv->state = S_kill;
            }
            else
            {
                int err;
#ifdef _WIN32
                unsigned long ul = 1;
                err = ioctlsocket(srv->sockfd, FIONBIO, &ul);
                if (err) { LOG_ERR_WSA("ioctlsocket(FIONBIO) failed", WSAGetLastError()); }
#else
                int flags = fcntl(srv->sockfd, F_GETFL);
                err = fcntl(srv->sockfd, F_SETFL, flags | O_NONBLOCK);
                if (err) { LOG_ERR_ERRNO("fcntl(O_NONBLOCK) failed", errno); }
#endif

                if (err)
                {
                    srv->sd.setError(-(__LINE__));
                    srv->state = S_terminate;
                }
                else { srv->state = S_bind; }
            }
            break;

        case S_bind:
        {
            int err;

            const sockopt_optval_t optval[] = { 1 };
            err = setsockopt(srv->sockfd, SOL_SOCKET, SO_REUSEADDR, optval, sizeof(optval));
            if (err) { LOG_ERR_SOCKET("setsockopt(SO_REUSEADDR) failed"); }

            struct sockaddr_in srvaddr;
            memset(&srvaddr, 0, sizeof(srvaddr));

            srvaddr.sin_family = AF_INET;
            srvaddr.sin_port = htons(srv->m_port);
            srvaddr.sin_addr.s_addr = htonl(INADDR_ANY);

            err = bind(srv->sockfd, (struct sockaddr*)(&srvaddr), sizeof(srvaddr));
            if (err)
            {
                LOG_ERR_SOCKET("bind() failed");
                srv->sd.setError(-(__LINE__));
                srv->state = S_terminate;
            }
            else { srv->state = S_listen; }
        }
        break;

        case S_listen:
            static_assert(listenBacklog <= SOMAXCONN);
            if (0 != listen(srv->sockfd, listenBacklog))
            {
                LOG_ERR_SOCKET("listen() failed");
                srv->sd.setError(-(__LINE__));
                srv->state = S_terminate;
            }
            else { srv->state = S_ready; }
            break;

        case S_ready:
            LOG_INF("%slistening on port %i", ansi::esc[SGR_FG_BGREEN], (int)(srv->m_port));
            srv->sd.setStatus(thread::Status::running);
            srv->state = S_idle;
            break;



        case S_idle:
        {
            struct sockaddr_in addr;
            socklen_t addrlen = sizeof(addr);

            const sockfd_t connfd = accept(srv->sockfd, (struct sockaddr*)(&addr), &addrlen);
            if (connfd < 0)
            {
#ifdef _WIN32
                errno = -1;
                const int wserr = WSAGetLastError();
                if (wserr == WSAEWOULDBLOCK) { errno = EWOULDBLOCK; }
#endif

                if ((errno == EAGAIN) || (errno == EWOULDBLOCK)) { (void)0; } // nop, sleep is done at end of loop
                // else if (errno == ECONNABORTED) { (void)0; }
                else if (errno == EINTR) { srv->sd.terminate(); }
                else
                {
                    // TODO ? error counter
                    LOG_ERR_SOCKET("accept() failed");
                }
            }
            else { srv->spawnClient(connfd, &addr); }

            if (srv->sd.sigterm())
            {
                LOG_INF("terminating");
                srv->state = S_terminate;
            }
        }
        break;



        case S_terminate:
        {
            srv->sd.setStatus(thread::Status::terminating);

            constexpr time_t termTimeout = 10;
            if (tTerminate == 0) { tTerminate = tNow; }

            bool allClientsClosed = true;

            // clean up connections
            for (size_t i = 0; i < srv->clients.size(); ++i)
            {
                auto& c = srv->clients[i];

                c.sd.terminate();

                const auto status = c.sd.status();
                if ((status != thread::Status::null) && (status != thread::Status::killed))
                {
                    allClientsClosed = false;

                    if ((tNow - tTerminate) >= termTimeout)
                    {
                        LOG_WRN("killing client #%zu %s", i, c.idstr().c_str());
                        c.sd.kill();
                    }
                }
            }

            if (srv->m_gwc)
            {
                auto& c = srv->m_gwc;

                c->sd.terminate();

                const auto status = c->sd.status();
                if ((status != thread::Status::null) && (status != thread::Status::killed))
                {
                    allClientsClosed = false;

                    if ((tNow - tTerminate) >= termTimeout)
                    {
                        LOG_WRN("killing gateway client");
                        c->sd.kill();
                    }
                }
            }

            if (allClientsClosed)
            {
                // close socket
                if (0 != sockclose(srv->sockfd))
                {
                    LOG_ERR_SOCKET("close(sockfd) failed");
                    if (!srv->sd.error()) { srv->sd.setError(-(__LINE__)); }
                }

                // exit/kill thread
                srv->state = S_kill;
            }
        }
        break;

        case S_kill:
            srv->sd.kill();
            break;

        default:
            LOG_ERR("invalid state: %i", srv->state);
            UTIL_sleep_ms(5000);
            break;
        }

        UTIL_sleep_ms(5);

        if (srv->state == S_terminate)
        {
            UTIL_sleep_ms(5);
            if (tTerminate < tNow) { UTIL_sleep_ms(90); }
        }
    }

    srv->sd.setStatus(thread::Status::killed);
}

server::Server::Server()
    : sd(), m_port(PHYOIP_DEFAULT_PORT), m_peerInfoDescr(getPeerInfoDescr()), m_gwc(nullptr)
{}

server::Server::Server(uint16_t port, int argc, char** argv)
    : sd(), m_port(port ? port : PHYOIP_DEFAULT_PORT), m_peerInfoDescr(getPeerInfoDescr(argc, argv)), m_gwc(nullptr)
{}

int server::Server::registerClient(client::Client* client, uint16_t type)
{
    std::lock_guard<std::mutex> lg(m_mtxClient);

    bool weClientRegistered = false;
    bool wiClientRegistered = false;

    for (size_t i = 0; i < clients.size(); ++i)
    {
        const auto t = clients[i].sd.clientType();

        if (t & PHYOIP_CTPERM_WE) { weClientRegistered = true; }
        if (t & PHYOIP_CTPERM_WI) { wiClientRegistered = true; }
    }

    if (m_gwc) { wiClientRegistered = true; }



    bool accept = true;

    // accept multiple WE clients, but only one WI client
    (void)weClientRegistered;
    if ((type & PHYOIP_CTPERM_WI) && wiClientRegistered) { accept = false; }



    if (!accept) { return -1; }

    client->sd.setClientType(type);
    return 0;
}

int server::Server::registerClient(gateway::Client* client)
{
    if (m_gwc)
    {
        LOG_ERR("can't register multiple gateway clients");
        return -1;
    }

    m_gwc = client;
    m_gwc->sd.setClientType(PHYOIP_CT_GW);

    return 0;
}

void server::Server::delistClient(client::Client* client)
{
    std::lock_guard<std::mutex> lg(m_mtxClient);
    client->sd.setClientType(PHYOIP_CTPERM_NONE);
}

void server::Server::pushPacket(const std::vector<uint8_t>& packet)
{
    std::lock_guard<std::mutex> lg(m_mtxClient);

    const struct phyoip_uarthdr* const hdr = (const struct phyoip_uarthdr*)(packet.data());

    if (m_gwc && !(hdr->ingress)) { m_gwc->sd.push(packet); }

    for (size_t i = 0; i < clients.size(); ++i)
    {
        auto& c = clients[i];
        const auto perm = c.sd.clientType();

        if ((hdr->ingress && (perm & PHYOIP_CTPERM_RI)) || (!(hdr->ingress) && (perm & PHYOIP_CTPERM_RE))) { c.sd.push(packet); }
    }
}

void server::Server::spawnClient(sockfd_t connfd, const void* sockaddr_in)
{
    const struct sockaddr_in* const addr = (const struct sockaddr_in*)sockaddr_in;
    char addrString[SOCKADDRSTRLEN];
    int err;
    size_t idx;
    bool accept = false;

    if (!sockaddrtos(addr, addrString, sizeof(addrString)))
    {
        addrString[0] = '-';
        addrString[1] = 0;
    }

    for (size_t i = 0; i < clients.size(); ++i)
    {
        const auto status = clients[i].sd.status();
        if ((status == thread::Status::null) || (status == thread::Status::killed))
        {
            accept = true;
            idx = i;
            break;
        }
    }

    if (accept)
    {
        char buffer[INET_ADDRSTRLEN];

        err = clients[idx].reset(this, connfd, inet_ntop(addr->sin_family, &(addr->sin_addr.s_addr), buffer, sizeof(buffer)), ntohs(addr->sin_port));
        if (err)
        {
            LOG_ERR("failed to reset client #%zu, rejecting %s", idx, addrString);

            err = sockclose(connfd);
            if (err) { LOG_ERR_SOCKET("close(connfd) failed"); }
        }
        else
        {
#if LOG_LEVEL_IS_ENABLED(LOG_LEVEL_DBG)
            size_t nActiveClients = 1; // this new one is not yet marked as running
            for (size_t i = 0; i < clients.size(); ++i)
            {
                const auto status = clients[i].sd.status();
                if ((status != thread::Status::null) && (status != thread::Status::killed)) { ++nActiveClients; }
            }
            LOG_DBG("client #%zu (%zu/%zu): %s", idx, nActiveClients, Server::maxClients, addrString);
#endif

            std::thread th(server::client::Client::task, &(clients[idx]));
            th.detach();
        }
    }
    else
    {
        LOG_WRN("can't accept more than %zu client connections, rejecting %s", Server::maxClients, addrString);

        // TODO gracefully disconnect on protocol level

        err = sockclose(connfd);
        if (err) { LOG_ERR_SOCKET("close(connfd) failed"); }
    }
}



std::string getPeerInfoDescr(int argc, char** argv)
{
    std::string str = "PHYoIP server sample implementation";

    if (argc && argv)
    {
        str += "\nargs:";

        for (int i = 0; i < argc; ++i)
        {
            str += (char)0x20;
            str += *(argv + i);
        }
    }
    else if (argc || argv) { LOG_ERR("%s(%i, %p)", UTIL__FUNCNAME__, argc, argv); }

    return str;
}
