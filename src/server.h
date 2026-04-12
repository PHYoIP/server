/*
author          Oliver Blaser
date            29.03.2026
copyright       GPL-3.0 - Copyright (c) 2026 Oliver Blaser
*/

#ifndef IG_SERVER_H
#define IG_SERVER_H

#include <array>
#include <cstddef>
#include <cstdint>

#include "client.h"
#include "common/socket.h"
#include "common/thread.h"



namespace server {

class SharedData : public thread::ThreadCtl
{
public:
    SharedData()
        : thread::ThreadCtl(), m_err(0)
    {}

    virtual ~SharedData() {}


    // clang-format off
    int error() const { lock_guard lg(m_mtx); return m_err; }
    // clang-format on


private:
    int m_err;

public:
    // thread internal

    // clang-format off
    void setError(int err) { lock_guard lg(m_mtx); m_err = err; } ///< thread internal
    // clang-format on
};

class Server
{
public:
    static constexpr int listenBacklog = 5;
    static constexpr size_t maxClients = 20;

    static void task(Server* srv);

public:
    Server()
        : sd(), m_port(0)
    {}

    Server(uint16_t port)
        : sd(), m_port(port)
    {}

    virtual ~Server() {}

    uint16_t port() const { return m_port; }

public:
    SharedData sd;

private:
    const uint16_t m_port;

private:
    /// \name Task Internal
    /// These are only accessed within `Server::task()` and do not need to be initialised in constructor.
    /// @{

    int state;
    sockfd_t sockfd;
    std::array<client::Client, maxClients> clients;

    void spawnClient(sockfd_t connfd, const void* sockaddr_in);

    /// @}
};



} // namespace server



#endif // IG_SERVER_H
