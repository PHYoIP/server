/*
author          Oliver Blaser
date            09.11.2026
copyright       GPL-3.0 - Copyright (c) 2026 Oliver Blaser
*/

#ifndef IG_GATEWAY_H
#define IG_GATEWAY_H

#include <cstddef>
#include <cstdint>
#include <ctime>
#include <string>

#include "client.h"
#include "common/serial.h"



namespace gateway {

class ProtocolConfig
{
public:
    enum class Mode
    {
        ASCII,
        binary,
    };

public:
    ProtocolConfig()
        : m_mode(), m_start(0), m_stop{ 0x0A, 0 }, m_timeout(0), m_posLen(noLen)
    {}

    virtual ~ProtocolConfig() {}

    int parse(const char* arg);

    Mode mode() const { return m_mode; }
    bool hasStartCond() const;

    char asciiStart() const { return m_start; }
    bool asciiTwoStop() const { return (m_stop[1] != 0); }
    const char* asciiStop() const { return m_stop; }

    bool binHasLenSpec() const { return (m_posLen != noLen); }

private:
    Mode m_mode;

    // ASCII parameters
    char m_start;   // null if not used
    char m_stop[2]; // [0] the stop symbol (may be null), [1] null if symbol is only one char

    // binary parameters
    uint32_t m_timeout;
    size_t m_posLen;

    static constexpr size_t noLen = (size_t)(-1);

    int m_setMode(const std::string& field);
    int m_setAsciiParam(const std::string& field, size_t idx);
    int m_setBinParam(const std::string& field, size_t idx);
};

class Config
{
public:
    using Parity = comm::Serial::Parity;
    using StopBits = comm::Serial::StopBits;

public:
    Config()
        : m_port(), m_baud(115200), m_dataBits(8), m_parity(Parity::none), m_stopBits(StopBits::one), m_protoConfig()
    {}

    virtual ~Config() {}

    void setPort(const std::string& port) { m_port = port; }
    void setBaud(uint32_t baud) { m_baud = baud; }
    void setProtocolConfig(const ProtocolConfig& cfg) { m_protoConfig = cfg; }

    int parseConfig(const char* arg);

    const std::string& port() const { return m_port; }
    uint32_t baud() const { return m_baud; }
    uint8_t dataBits() const { return m_dataBits; }
    const Parity& parity() const { return m_parity; }
    const StopBits& stopBits() const { return m_stopBits; }
    const ProtocolConfig& protoConfig() const { return m_protoConfig; }

private:
    std::string m_port;
    uint32_t m_baud;
    uint8_t m_dataBits;
    Parity m_parity;
    StopBits m_stopBits;
    ProtocolConfig m_protoConfig;
};

} // namespace gateway

namespace gateway {

class Client : public server::client::Common
{
public:
    static constexpr size_t bufferSize = 1024;

    static void task(Client* cnt) { cnt->m_task(); }

public:
#ifdef _MSC_VER
// #pragma warning(suppress :26495)
#endif
    Client()
        : server::client::Common(nullptr), m_config(), m_serial()
    {}

    Client(server::Server* srv, const Config& config)
        : server::client::Common(srv), m_config(config)
    {}

    virtual ~Client() {}

private:
    const gateway::Config m_config;
    comm::Serial m_serial;
    uint8_t m_rxBuffer[bufferSize];
    uint8_t m_txBuffer[bufferSize];
    size_t m_rxIdx = 0;
    size_t m_txCount = 0;
    size_t m_txIdx = 0;
    bool m_rxStart = false;
    struct timespec m_tRxChunk = {};
    struct timespec m_tRxPacket = {};

    bool m_txBusy() const { return (m_txCount != 0); }

    void m_task();
    int m_taskRx();
    int m_taskTx();

    int m_handleReceivedChunk(const uint8_t* data, size_t count);
    int m_handleReceivedPacket(const uint8_t* data, size_t count);
};

} // namespace gateway



#endif // IG_GATEWAY_H
