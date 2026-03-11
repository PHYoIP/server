/*
author          Oliver Blaser
date            11.03.2026
copyright       GPL-3.0 - Copyright (c) 2026 Oliver Blaser
*/

#include <cstddef>
#include <cstdint>
#include <ctime>
#include <string>

#include "client.h"
#include "common/ansi-esc.h"
#include "common/socket.h"
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
#define LOG_MODULE_NAME  CNT
#include "common/log.h"



enum
{
    S_init = 0,

    S_idle,

    S_terminate,
    S_kill,
};



int server::client::Client::reset(sockfd_t connfd, const std::string& addr, uint16_t port)
{
    const auto status = sd.status();
    if ((status != thread::Status::null) && (status != thread::Status::killed))
    {
        LOG_ERR("can't reset running client");
        return -(__LINE__);
    }

    sd.reset();

    m_connfd = connfd;
    m_addr = addr;
    m_port = port;

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
        switch (state)
        {
        case S_init:
            LOG_DBG("started thread for client %s", m_addr.c_str());
            sd.setStatus(thread::Status::running);
            state = S_idle;
            break;

        case S_idle:
        {
            if (sd.sigterm())
            {
                LOG_INF("terminating");
                sd.setStatus(thread::Status::terminating);
                state = S_terminate;
            }
        }
        break;

        case S_terminate:

            LOG_DBG("terminating thread for client %s", m_addr.c_str());

            // close socket
            if (0 != sockclose(m_connfd))
            {
                LOG_ERR_SOCKET("close(connfd) failed");
                if (!sd.error()) { sd.setError(-(__LINE__)); }
            }

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

    LOG_DBG("exited thread for client %s", m_addr.c_str());
    sd.setStatus(thread::Status::killed);
}
