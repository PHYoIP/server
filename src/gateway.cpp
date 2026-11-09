/*
author          Oliver Blaser
date            09.11.2026
copyright       GPL-3.0 - Copyright (c) 2026 Oliver Blaser
*/

#include <cerrno>
#include <cinttypes>
#include <cstddef>
#include <cstdint>
#include <cstring>
#include <string>

#include "client.h"
#include "common/serial.h"
#include "common/socket.h"
#include "common/string.h"
#include "gateway.h"
#include "server.h"
#include "util/macros.h"
#include "util/time.h"

#include <phyoip/protocol/uart.h>

#ifdef _WIN32

#include <WS2tcpip.h>

#else // *nix

#include <arpa/inet.h>
#include <limits.h>

#endif // Windows / *nix


#define LOG_MODULE_LEVEL LOG_LEVEL_DBG
#define LOG_MODULE_NAME  GW
#include "common/log.h"



enum
{
    S_init = 0,
    S_open,
    S_idle,
    S_terminate,
    S_kill,
};



static inline ssize_t parseUartPacket(uint8_t* buffer, size_t size, const uint8_t* packet);



int gateway::ProtocolConfig::parse(const char* arg)
{
    if (!arg || (*arg == 0)) { return -(__LINE__); }

    const char* p = arg;
    const char* const end = arg + strlen(arg);

    const char* fieldBegin;
    const char* fieldEnd;
    size_t fieldIdx = 0;

    constexpr char delimiter = ',';

    // set optional parameters to default values incase they are not in parameter list
    m_start = 0;
    m_posLen = noLen;

    while (p < end)
    {
        fieldBegin = p;
        while ((p < end) && (*p != delimiter)) { ++p; }
        fieldEnd = p;

        const std::string field(fieldBegin, fieldEnd);

        if (fieldIdx == 0)
        {
            if (0 != m_setMode(field)) { return -(__LINE__); }
        }
        else
        {
            switch (m_mode)
            {
            case Mode::ASCII:
                if (0 != m_setAsciiParam(field, fieldIdx)) { return -(__LINE__); }
                break;

            case Mode::binary:
                if (0 != m_setBinParam(field, fieldIdx)) { return -(__LINE__); }
                break;
            }
        }

        ++p; // skip delimiter
        ++fieldIdx;
    }

    return 0;
}

bool gateway::ProtocolConfig::hasStartCond() const
{
    switch (m_mode)
    {
    case Mode::ASCII:
        if (m_start != 0) { return true; }
        break;

    case Mode::binary:
        if (this->binHasLenSpec()) { return true; }
        break;
    }

    return false;
}

int gateway::ProtocolConfig::m_setMode(const std::string& field)
{
    if (field == "A") { m_mode = Mode::ASCII; }
    else if (field == "B") { m_mode = Mode::binary; }
    else { return -(__LINE__); }

    return 0;
}

int gateway::ProtocolConfig::m_setAsciiParam(const std::string& field, size_t idx)
{
    size_t size;
    char value[2];

    if (field == "LF")
    {
        size = 1;
        value[0] = 0x0A;
        value[1] = 0;
    }
    else if (field == "CR")
    {
        size = 1;
        value[0] = 0x0D;
        value[1] = 0;
    }
    else if (field == "CRLF")
    {
        size = 2;
        value[0] = 0x0D;
        value[1] = 0x0A;
    }
    else if (field == "NULL")
    {
        size = 1;
        value[0] = 0;
        value[1] = 0;
    }
    else
    {
        try
        {
            const int32_t tmp = comm::hexstoi(field);

            if ((tmp < 0) || (tmp > UINT16_MAX)) { return -(__LINE__); }

            if (tmp & 0xFF00)
            {
                size = 2;
                value[0] = (char)((tmp >> 8) & 0x00FF);
                value[1] = (char)(tmp & 0x00FF);
            }
            else
            {
                size = 1;
                value[0] = (char)(tmp & 0x00FF);
                value[1] = 0;
            }
        }
        catch (...)
        {
            return -(__LINE__);
        }
    }



    if (idx == 1) // stop symbol
    {
        m_stop[0] = value[0];
        m_stop[1] = value[1];
    }
    else if (idx == 2) // start symbol
    {
        if ((size == 2) || (value[0] == 0)) { return -(__LINE__); }
        m_start = value[0];
    }
    else { return -(__LINE__); }



    return 0;
}

int gateway::ProtocolConfig::m_setBinParam(const std::string& field, size_t idx)
{
    LOG_ERR("%s not yet implemented", UTIL__FUNCSIG__);
    return -(__LINE__);

    if (idx == 1) {}
    else if (idx == 2) {}
    else { return -(__LINE__); }

    return 0;
}

int gateway::Config::parseConfig(const char* arg)
{
    if (!arg) { return -(__LINE__); }
    if (strlen(arg) < 3) { return -(__LINE__); }

    const char data = arg[0];
    const char parity = arg[1] & ~0x20;
    const std::string stop = arg + 2;

    if (comm::isDigit(data)) { m_dataBits = data - 0x30; }
    else { return -(__LINE__); }

    if (parity == 'N') { m_parity = Parity::none; }
    else if (parity == 'E') { m_parity = Parity::even; }
    else if (parity == 'O') { m_parity = Parity::odd; }
    else if (parity == 'M') { m_parity = Parity::mark; }
    else if (parity == 'S') { m_parity = Parity::space; }
    else { return -(__LINE__); }

    if (stop == "1") { m_stopBits = StopBits::one; }
    else if (stop == "1.5") { m_stopBits = StopBits::onePointFive; }
    else if (stop == "2") { m_stopBits = StopBits::two; }
    else { return -(__LINE__); }

    return 0;
}



void gateway::Client::m_task()
{
    int state = S_init;

    sd.setStatus(thread::Status::booting);

    while (!sd.sigkill())
    {
        switch (state)
        {
        case S_init:
            LOG_DBG("started thread");
            state = S_open;
            break;

        case S_open:
        {
            errno = 0;
            const int err = m_serial.open(m_config.port().c_str(), m_config.baud(), m_config.dataBits(), m_config.parity(), m_config.stopBits());
            if (err)
            {
                if (errno) { LOG_ERR_ERRNO("failed to open serial port", errno); }
                else { LOG_ERR("failed to open serial port %s, err: %i", m_config.port().c_str(), err); }

                // gateway servers that failed to open the serial port should not be running
                m_srv->sd.terminate();

                sd.setError(-(__LINE__));
                state = S_terminate;
            }
            else
            {
                errno = 0;
                if (0 != m_serial.flush())
                {
                    if (errno) { LOG_ERR_ERRNO("failed to flush serial port", errno); }
                    else { LOG_ERR("failed to flush serial port"); }
                }

                sd.setStatus(thread::Status::running);
                state = S_idle;
            }
        }
        break;

        case S_idle:
        {
            int err;

            err = m_taskRx();
            if (err)
            {
                sd.setError(err);
                state = S_terminate;
            }

            err = m_taskTx();
            if (err)
            {
                sd.setError(err);
                state = S_terminate;
            }

            if (sd.packetQueued() && !m_txBusy())
            {
                const auto packet = sd.pop();
                const ssize_t res = parseUartPacket(m_txBuffer, sizeof(m_txBuffer), packet.data());
                if (res > 0)
                {
                    m_txCount = (size_t)res;
                    // LOG_DBG_HD(m_txBuffer, m_txCount, "send packet:");
                }
                else
                {
                    sd.setError(res);
                    state = S_terminate;
                }
            }

            if (sd.sigterm())
            {
                LOG_INF("terminating");
                state = S_terminate;
            }
        }
        break;

        case S_terminate:

            LOG_DBG("terminating thread");
            sd.setStatus(thread::Status::terminating);

            if (m_serial.isOpen())
            {
                errno = 0;
                const int err = m_serial.close();
                if (err)
                {
                    if (errno) { LOG_ERR_ERRNO("failed to close serial port", errno); }
                    else { LOG_ERR("failed to close serial port %s, err: %i", m_config.port().c_str(), err); }
                }
            }

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

        UTIL_sleep_us(100); // TODO dimensioning/coordination with gateway::ProtocolConfig::TIMEOUT
    }

    LOG_DBG("exited thread (err: %i)", sd.error());
    sd.setStatus(thread::Status::killed);
}

int gateway::Client::m_taskRx()
{
    uint8_t buffer[256];

    const ssize_t res = m_serial.read(buffer, sizeof(buffer));
    if (res < 0)
    {
        LOG_ERR_ERRNO("serial read failed", errno);
        return -(__LINE__);
    }
    else if (res > 0)
    {
        if (0 != UTIL_getTime(&m_tRxChunk)) { LOG_ERR_ERRNO("UTIL_getTime() failed", errno); }

        const int err = m_handleReceivedChunk(buffer, (size_t)res);
        if (err) { return err; }
    }

    return 0;
}

int gateway::Client::m_taskTx()
{
    if (m_txCount)
    {
        const ssize_t res = m_serial.write(m_txBuffer + m_txIdx, m_txCount);
        if (res < 0)
        {
            LOG_ERR_ERRNO("serial write failed", errno);
            return -(__LINE__);
        }
        else
        {
            size_t n = (size_t)res;
            if (n > m_txCount)
            {
                LOG_ERR("written %zu of %zu bytes", n, m_txCount);
                n = m_txCount; // clamp for correct math below
            }

            m_txIdx += n;
            m_txCount -= n;
        }
    }

    if (!m_txCount) { m_txIdx = 0; }

    return 0;
}

int gateway::Client::m_handleReceivedChunk(const uint8_t* data, size_t count)
{
#if 0
    LOG_DBG_HD(data, count, "received chunk:");
#endif

    const auto& protoCfg = m_config.protoConfig();
    const uint8_t* p = data;

    while (p < (data + count))
    {
        if (protoCfg.hasStartCond())
        {
            bool startCond = false;

            if (protoCfg.mode() == ProtocolConfig::Mode::ASCII) { startCond = (*p == (uint8_t)(protoCfg.asciiStart())); }
            else if (protoCfg.mode() == ProtocolConfig::Mode::binary)
            {
                // startCond = ((tNow - tLastRx) >= protoCfg.binTimeout());
                LOG_ERR("binary is not yet implemented in start condition");
                return -(__LINE__);
            }
            else
            {
                LOG_ERR("unhandled protocol in start condition");
                return -(__LINE__);
            }

            if (startCond)
            {
                if (m_rxIdx > 0) { LOG_WRN_HD(m_rxBuffer, m_rxIdx, "protocol START, discarding received data:"); }
                m_rxStart = true;
                m_rxIdx = 0;
            }
        }

        if (m_rxIdx >= SIZEOF_ARRAY(m_rxBuffer))
        {
            LOG_ERR_HD(m_rxBuffer, sizeof(m_rxBuffer), "receive buffer overflow, discarding received data:");
            m_rxStart = false;
            m_rxIdx = 0;
        }

        m_rxBuffer[m_rxIdx++] = *(p++);



        bool stopCond = false;

        if (protoCfg.mode() == ProtocolConfig::Mode::ASCII)
        {
            const char* const stop = protoCfg.asciiStop();

            if (!protoCfg.asciiTwoStop()) { stopCond = (m_rxBuffer[m_rxIdx - 1] == (uint8_t)(*stop)); }
            else if (m_rxIdx >= 2)
            {
                stopCond = ((m_rxBuffer[m_rxIdx - 2] == (uint8_t)(*(stop + 0))) && //
                            (m_rxBuffer[m_rxIdx - 1] == (uint8_t)(*(stop + 1))));  //
            }
        }
        else if (protoCfg.mode() == ProtocolConfig::Mode::binary)
        {
            LOG_ERR("binary is not yet implemented in stop condition");
            return -(__LINE__);
        }
        else
        {
            LOG_ERR("unhandled protocol in stop condition");
            return -(__LINE__);
        }

        if (stopCond)
        {
            if (protoCfg.hasStartCond() && !m_rxStart) { LOG_WRN_HD(m_rxBuffer, m_rxIdx, "protocol STOP without START, discarding received data:"); }
            else
            {
                m_tRxPacket = m_tRxChunk;

                const int err = m_handleReceivedPacket(m_rxBuffer, m_rxIdx);
                if (err) { return err; }
            }

            m_rxStart = false;
            m_rxIdx = 0;
        }
    }

    return 0;
}

int gateway::Client::m_handleReceivedPacket(const uint8_t* data, size_t count)
{
#if LOG_LEVEL_IS_ENABLED(LOG_LEVEL_DBG) && 0
    LOG_INF_HD(data, count, "received packet:");
#endif

    if (count > UINT16_MAX)
    {
        LOG_ERR("%s can't handle %zu bytes of data", UTIL__FUNCSIG__, count);
        return -(__LINE__);
    }

    constexpr uint8_t dataoffs = sizeof(struct phyoip_uarthdr);
    std::vector<uint8_t> packet(dataoffs + count, 0);

    struct phyoip_uarthdr* const hdr = (struct phyoip_uarthdr*)packet.data();
    hdr->hsize = dataoffs;
    hdr->dsize = htons((uint16_t)count);
    hdr->timestamp = htonll((int64_t)m_tRxPacket.tv_sec);
    hdr->nsec = htonl(m_tRxPacket.tv_nsec);
    hdr->ingress = 1;

    for (size_t i = 0; i < count; ++i) { packet[dataoffs + i] = *(data + i); }

    m_srv->pushPacket(packet);

    return 0;
}



// TODO move to packet::parse::uart() or packet::parse::uartData()
inline ssize_t parseUartPacket(uint8_t* buffer, size_t size, const uint8_t* packet)
{
    const struct phyoip_uarthdr* const hdr = (const struct phyoip_uarthdr*)(packet);
    const size_t hsize = hdr->hsize;
    const size_t dsize = ntohs(hdr->dsize);

    const uint8_t* data = packet + hsize;
    const size_t count = dsize;

    if (count > size)
    {
        LOG_ERR("insufficient buffer, size: %zu, hsize: %zu, dsize: %zu", size, hsize, dsize);
        return -(__LINE__);
    }

    if (count > SSIZE_MAX)
    {
        LOG_ERR("can't copy UART data, count: %zu, SSIZE_MAX: %" PRIi64, count, (int64_t)SSIZE_MAX);
        return -(__LINE__);
    }

    for (size_t i = 0; i < count; ++i) { *(buffer + i) = *(data + i); }

    return (ssize_t)count;
}
