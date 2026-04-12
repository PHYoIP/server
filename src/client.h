/*
author          Oliver Blaser
date            12.04.2026
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
            : thread::ThreadCtl(), m_err(0), m_clitype(0), m_id(0)
        {}

        virtual ~SharedData() {}


        // clang-format off
        int error() const { lock_guard lg(m_mtx); return m_err; }
        uint16_t clientType() const { lock_guard lg(m_mtx); return m_clitype; }
        uintptr_t clientId() const { lock_guard lg(m_mtx); return m_id; }
        void push() { lock_guard lg(m_mtx); /* m_queue.push */ }
        // clang-format on

        void reset()
        {
            setError(0);
            setClientInfo(0, 0);
            // m_queue.clear();
            thread::ThreadCtl::reset();
        }

    private:
        int m_err;
        uint16_t m_clitype;
        uintptr_t m_id;

    public:
        // thread internal

        // clang-format off
        void setError(int err) { lock_guard lg(m_mtx); m_err = err; } ///< thread internal
        void setClientInfo(uint16_t ct, uintptr_t id) { lock_guard lg(m_mtx); m_clitype = ct; m_id = id; } ///< thread internal
        // clang-format on
    };



    /**
     * @brief Client connection/instance inside the server.
     */
    class Client
    {
    public:
        static constexpr size_t bufferSize = 1024;
        static constexpr uint32_t registerTimeout = 30; // [s]

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
         * @param cfd New connection socket fd
         * @param addr Peer IPv4 address
         * @param port Peer port (in host encoding)
         * @return 0 on success
         */
        int reset(sockfd_t connfd, const std::string& addr, uint16_t port);

    public:
        SharedData sd;

    private:
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
