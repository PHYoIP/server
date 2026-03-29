/*
author          Oliver Blaser
date            29.03.2026
copyright       GPL-3.0 - Copyright (c) 2026 Oliver Blaser
*/

#ifndef IG_CLIENT_H
#define IG_CLIENT_H

#include <cstddef>
#include <cstdint>
#include <string>

#include "common/socket.h"
#include "common/thread.h"



namespace server {
namespace client {



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



    /**
     * @brief Client connection/instance inside the server.
     */
    class Client
    {
    public:
        static constexpr size_t bufferSize = 512;
        static constexpr uint32_t registerTimeout = 10; // [s]

        static void task(Client* cnt) { cnt->m_task(); }

    public:
        Client()
            : sd(), m_connfd(-1), m_addr(), m_port(0)
        {}

        virtual ~Client() {}

        /**
         * Prepares the instance for new execution. Is only allowed to be called on killed threads/instances.
         *
         * If an error is returned, `Client::task(Client*)` must not be executed on this instance!
         *
         * @param cfd New connection socket fd
         * @param addr Peer IPv4 address
         * @param port Peer port (in host encoding)
         * @return 0 on success
         */
        int reset(sockfd_t connfd, const std::string& addr, uint16_t port);

    public:
        SharedData sd;

    private:
        sockfd_t m_connfd;
        std::string m_addr;
        uint16_t m_port;

        void m_task();
        int m_taskRecv();
        int m_taskSend();
    };



} // namespace client
} // namespace server



#endif // IG_CLIENT_H
