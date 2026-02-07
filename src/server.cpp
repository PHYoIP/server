/*
author          Oliver Blaser
date            07.02.2026
copyright       GPL-3.0 - Copyright (c) 2026 Oliver Blaser
*/

#include <cstddef>
#include <cstdint>

#include "common/ansi-esc.h"
#include "common/socket.h"
#include "server.h"
#include "util/time.h"

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



void server::Server::task(Server* srv)
{
    srv->sd.setStatus(thread::Status::booting);
    srv->state = S_init;

    while (!srv->sd.sigkill())
    {
        switch (srv->state)
        {
        case S_init:
            srv->state = S_open;
            break;

        case S_open:
            srv->sockfd = socket(AF_INET, SOCK_STREAM, IPPROTO_TCP);
            if (srv->sockfd < 0)
            {
                LOG_ERR_SOCKET("socket() failed");
                srv->sd.setError(-(__LINE__));
                srv->state = S_kill;
            }
            else { srv->state = S_bind; }
            break;

        case S_bind:
        {
            int err;

            const sockopt_optval_t optval[] = { 1 };
            err = setsockopt(srv->sockfd, SOL_SOCKET, SO_REUSEADDR, optval, sizeof(int));
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

            const sockfd_t connfd = accept(srv->sockfd, (struct sockaddr*)(&addr), &addrlen); // TODO async
            if (connfd < 0)
            {
                // TODO ? error counter

                LOG_ERR_SOCKET("accept() failed");
            }
            else
            {
                LOG_WRN("TODO handle connection"); // for now just closing right away

                const int err = sockclose(connfd);
                if (err) { LOG_ERR_SOCKET("close(connfd) failed"); }
            }

            if (srv->sd.sigterm())
            {
                LOG_INF("terminating");
                srv->sd.setStatus(thread::Status::terminating);
                srv->state = S_terminate;
            }
        }
        break;



        case S_terminate:
            // clean up connections
            // ...

            // close socket
            if (0 != sockclose(srv->sockfd))
            {
                LOG_ERR_SOCKET("close(sockfd) failed");
                if (!srv->sd.error()) { srv->sd.setError(-(__LINE__)); }
            }

            // exit/kill thread
            srv->state = S_kill;
            break;

        case S_kill:
            srv->sd.kill();
            break;

        default:
            LOG_ERR("invalid state: %i", srv->state);
            UTIL_sleep_ms(5000);
            break;
        }

        UTIL_sleep_us(500);
    }

    srv->sd.setStatus(thread::Status::killed);
}
