/*
author          Oliver Blaser
date            13.04.2026
copyright       GPL-3.0 - Copyright (c) 2026 Oliver Blaser
*/

#ifndef IG_CLIENT_H
#define IG_CLIENT_H

#include <cstddef>
#include <cstdint>
#include <queue>
#include <string>
#include <vector>

#include "common/socket.h"
#include "common/thread.h"



namespace server {

class Server;

namespace client {



    class SharedData : public thread::ThreadCtl
    {
    public:
        SharedData()
            : thread::ThreadCtl(), m_err(0), m_clitype(0)
        {}

        virtual ~SharedData() {}

        // clang-format off
        int error() const { lock_guard lg(m_mtx); return m_err; }
        uint16_t clientType() const { lock_guard lg(m_mtx); return m_clitype; }
        void setClientType(uint16_t ct) { lock_guard lg(m_mtx); m_clitype = ct; } 
        void push(const std::vector<uint8_t>& packet) { lock_guard lg(m_mtx); m_queue.push(packet); }
        // clang-format on

        void reset()
        {
            lock_guard lg(m_mtx);
            m_err = 0;
            m_clitype = 0;
            while (!m_queue.empty()) { m_queue.pop(); }
            thread::ThreadCtl::reset();
        }

    private:
        int m_err;
        uint16_t m_clitype;
        std::queue<std::vector<uint8_t>> m_queue;

    public:
        // thread internal

        // clang-format off
        void setError(int err) { lock_guard lg(m_mtx); m_err = err; } ///< thread internal
        bool packetQueued() const { lock_guard lg(m_mtx); return !m_queue.empty(); } ///< thread internal
        // clang-format on

        std::vector<uint8_t> pop()
        {
            lock_guard lg(m_mtx);
            const auto tmp = m_queue.front();
            m_queue.pop();
            return tmp;
        }
    };



    /**
     * @brief Client connection/instance inside the server.
     */
    class Client
    {
    public:
        static constexpr size_t bufferSize = 1024;
        static constexpr uint32_t registerTimeout = 30; // [s]

        /**
         * [s] delist/close timeout, the client has to close the connection within this period, or the connection getts
         * closed by the server.
         */
        static constexpr uint32_t delistCloseTimeout = 10;

        static void task(Client* cnt) { cnt->m_task(); }

    public:
#ifdef _MSC_VER
#pragma warning(suppress :26495)
#endif
        Client() {} ///< `Client::reset()` needs to be called on an instance before it can be run by  `Client::task()`.

        virtual ~Client() {}

        /**
         * Prepares the instance for new execution. Is only allowed to be called on killed threads/instances.
         *
         * If an error is returned, `Client::task(Client*)` must not be executed on this instance!
         *
         * @param srv Pointer to the parent server instance
         * @param connfd New connection socket fd
         * @param addr Peer IPv4 address
         * @param port Peer port (in host encoding)
         * @return 0 on success
         */
        int reset(server::Server* srv, sockfd_t connfd, const std::string& addr, uint16_t port);

    public:
        SharedData sd;

    private:
        server::Server* m_srv = nullptr; // pointer to the parent server instance
        sockfd_t m_connfd = -1;
        std::string m_idstr; // client identifier string
        uint16_t m_port = 0;
        bool m_registered = false;
        uint32_t m_triggerFlags = 0;

        uint8_t m_rxBuffer[Client::bufferSize];
        uint8_t m_txBuffer[Client::bufferSize];
        size_t m_rxIdx = 0;
        size_t m_txCount = 0;
        size_t m_txIdx = 0;

        bool m_txBusy() const { return (m_txCount != 0); }

        void m_task();
        int m_taskRecv();
        int m_taskSend();

        int m_handleReceivedChunk(const uint8_t* data, size_t count);
        int m_handleReceivedPacket(const uint8_t* data, size_t count);
        int m_handleCmp(const uint8_t* data, size_t count);
        int m_handleCmpRegister(const uint8_t* data, size_t count);
        int m_handleUart(const uint8_t* data, size_t count);
    };



} // namespace client
} // namespace server



#endif // IG_CLIENT_H
