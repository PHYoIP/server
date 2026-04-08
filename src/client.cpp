/*
author          Oliver Blaser
date            08.04.2026
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
    m_registered = false;

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
        {
            LOG_DBG("started thread for client %s", m_addr.c_str());

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
                sd.setError(err);
                state = S_terminate;
            }

            err = m_taskSend();
            if (err)
            {
                sd.setError(err);
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

int server::client::Client::m_taskRecv()
{
    uint8_t buffer[512];

    const ssize_t res = recv(m_connfd, (char*)buffer,
#if 0
                             sizeof(buffer),
#else
                             (size_t)(5 + (rand() % 20)),
#endif
                             0);
    if (res == 0)
    {
        LOG_ERR("the client closed the connection unexpectedly");
        return -(__LINE__);
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
        LOG_DBG_HD(buffer, (size_t)res, "received chunk:");
        // ...
    }

    return 0;
}

int server::client::Client::m_taskSend()
{
    // ...
    return 0;
}
