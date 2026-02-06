/*
author          Oliver Blaser
date            03.02.2026
copyright       GPL-3.0 - Copyright (c) 2026 Oliver Blaser
*/

#include <cstddef>
#include <cstdint>
#include <cstring>
#include <string>

#include "common/string.h"
#include "gateway.h"
#include "util/macros.h"


#define LOG_MODULE_LEVEL LOG_LEVEL_DBG
#define LOG_MODULE_NAME  GW
#include "common/log.h"



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
    if (strlen(arg) != 3) { return -(__LINE__); }

    const char data = arg[0];
    const auto parity = comm::toUpper_ascii(std::string(1, arg[1]));
    const char stop = arg[2];

    if (comm::isDigit(data)) { m_dataBits = data - 0x30; }
    else { return -(__LINE__); }

    if (parity == "N") { m_parity = Parity::none; }
    else if (parity == "E") { m_parity = Parity::even; }
    else if (parity == "O") { m_parity = Parity::odd; }
    else { return -(__LINE__); }

    if (comm::isDigit(stop)) { m_stopBits = stop - 0x30; }
    else { return -(__LINE__); }

    return 0;
}
