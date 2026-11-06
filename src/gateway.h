/*
author          Oliver Blaser
date            06.11.2026
copyright       GPL-3.0 - Copyright (c) 2026 Oliver Blaser
*/

#ifndef IG_GATEWAY_H
#define IG_GATEWAY_H

#include <cstddef>
#include <cstdint>
#include <string>

#include "client.h"



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
        : m_mode(), m_start(0), m_stop{ 0x0A, 0 }, m_timeout(0), m_posLen(0)
    {}

    virtual ~ProtocolConfig() {}

    int parse(const char* arg);

    bool twoStop() const { return (m_stop[1] != 0); }
    bool hasLengthSpecifier() const { return (m_posLen != noLen); }

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
    enum class Parity
    {
        none,
        even,
        odd,
    };

public:
    Config()
        : m_port(), m_baud(115200), m_dataBits(8), m_parity(Parity::none), m_stopBits(1), m_protoConfig()
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
    uint8_t stopBits() const { return m_stopBits; }
    const ProtocolConfig& protoConfig() const { return m_protoConfig; }

private:
    std::string m_port;
    uint32_t m_baud;
    uint8_t m_dataBits;
    Parity m_parity;
    uint8_t m_stopBits;
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
        : server::client::Common(nullptr), m_config()
    {}

    Client(server::Server* srv, const Config& config)
        : server::client::Common(srv), m_config(config)
    {}

    virtual ~Client() {}

private:
    const gateway::Config m_config;

    void m_task();
};

} // namespace gateway



#endif // IG_GATEWAY_H
