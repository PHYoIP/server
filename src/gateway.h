/*
author          Oliver Blaser
date            03.02.2026
copyright       GPL-3.0 - Copyright (c) 2026 Oliver Blaser
*/

#ifndef IG_GATEWAY_H
#define IG_GATEWAY_H

#include <cstddef>
#include <cstdint>
#include <string>



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

private:
    std::string m_port;
    uint32_t m_baud;
    uint8_t m_dataBits;
    Parity m_parity;
    uint8_t m_stopBits;
    ProtocolConfig m_protoConfig;
};

} // namespace gateway



#endif // IG_GATEWAY_H
