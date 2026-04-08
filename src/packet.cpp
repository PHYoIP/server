/*
author          Oliver Blaser
date            08.04.2026
copyright       GPL-3.0 - Copyright (c) 2026 Oliver Blaser
*/

#include <cstddef>
#include <cstdint>
#include <cstring>
#include <string>
#include <vector>

#include "packet.h"
#include "project.h"
#include "util/macros.h"

#include <phyoip/protocol/cmp.h>
#include <phyoip/protocol/phyoip.h>

#ifdef _WIN32
#include <WinSock2.h>
#else // *nix
#include <arpa/inet.h>
#endif // Windows / *nix


#define LOG_MODULE_LEVEL LOG_LEVEL_DBG
#define LOG_MODULE_NAME  PCK
#include "common/log.h"



constexpr uint8_t phyoiphdrIdentifier[] = PHYOIP_HDR_IDENTIFIER_INITIALIZER_LIST;



int packet::serialise::phyoip(uint8_t* buffer, size_t size, uint8_t proto, const uint8_t* data, size_t count)
{
    constexpr uint8_t dataoffs = sizeof(struct phyoiphdr);

    if (count > (size - dataoffs))
    {
        LOG_ERR("%s can't serialise %zu bytes of data", UTIL__FUNCSIG__, count);
        return -(__LINE__);
    }

    struct phyoiphdr* const hdr = (struct phyoiphdr*)(buffer);
    for (size_t i = 0; i < PHYOIP_HDR_IDENTIFIER_SIZE; ++i) { hdr->identifier[i] = phyoiphdrIdentifier[i]; }
    hdr->vermaj = PHYOIP_PROTOCOL_VERSION_MAJ;
    hdr->vermin = PHYOIP_PROTOCOL_VERSION_MIN;
    hdr->hsize = dataoffs;
    hdr->proto = proto;

    if (!data) { count = 0; }
    for (size_t i = 0; i < count; ++i) { buffer[dataoffs + i] = *(data + i); }

    return 0;
}

int packet::serialise::cmp(uint8_t* buffer, size_t size, uint8_t type, const uint8_t* data, size_t count)
{
    constexpr uint8_t dataoffs = sizeof(struct phyoip_cmphdr);

    if (count > UINT16_MAX)
    {
        LOG_ERR("%s can't serialise %zu bytes of data", UTIL__FUNCSIG__, count);
        return -(__LINE__);
    }

    if (size < (dataoffs + count))
    {
        LOG_ERR("%s insufficient buffer size: %zu", UTIL__FUNCSIG__, size);
        return -(__LINE__);
    }

    std::vector<uint8_t> cmpBuffer(dataoffs + count, 0);

    struct phyoip_cmphdr* const hdr = (struct phyoip_cmphdr*)cmpBuffer.data();
    hdr->hsize = dataoffs;
    hdr->dsize = htons((uint16_t)count);
    hdr->type = type;

    if (!data) { count = 0; }
    for (size_t i = 0; i < count; ++i) { cmpBuffer[dataoffs + i] = *(data + i); }

    return packet::serialise::phyoip(buffer, size, PHYOIP_PROTO_CMP, cmpBuffer.data(), cmpBuffer.size());
}

int packet::serialise::cmpAck(uint8_t* buffer, size_t size, uint8_t status)
{
    std::vector<uint8_t> data(sizeof(struct phyoip_cmpack), 0);

    struct phyoip_cmpack* const ack = (struct phyoip_cmpack*)data.data();
    ack->status = status;

    return packet::serialise::cmp(buffer, size, PHYOIP_CMP_ACK, data.data(), data.size());
}

int packet::serialise::cmpPeerInfo(uint8_t* buffer, size_t size)
{
    const std::string name = prj::appName;
    const std::string description = "PHYoIP server sample implementation";
    const std::string versionString = PRJ_VERSION_STR;

    const size_t nameoffs = sizeof(struct phyoip_cmppi);
    const size_t descoffs = sizeof(struct phyoip_cmppi) + name.length() + 1;
    const size_t veroffs = sizeof(struct phyoip_cmppi) + name.length() + 1 + description.length() + 1;

    if ((nameoffs > UINT16_MAX) || (descoffs > UINT16_MAX) || (veroffs > UINT16_MAX))
    {
        LOG_ERR("%s string pointer/offset overflow", UTIL__FUNCSIG__);
        return -(__LINE__);
    }

    std::vector<uint8_t> data((sizeof(struct phyoip_cmppi) + name.length() + 1 + description.length() + 1 + versionString.length() + 1), 0);

    struct phyoip_cmppi* const info = (struct phyoip_cmppi*)data.data();
    info->proto = PHYOIP_PROTO_UART;
    info->supplier = htons(PHYOIP_SUPPLIER_PHYOIP);
    info->nameoffs = htons((uint16_t)nameoffs);
    info->version.v.maj = PRJ_VERSION_MAJ;
    info->version.v.min = PRJ_VERSION_MIN;
    info->veroffs = htons((uint16_t)veroffs);
    info->descoffs = htons((uint16_t)descoffs);
    static_assert((PRJ_VERSION_MAJ <= UINT8_MAX) && (PRJ_VERSION_MIN <= UINT8_MAX));

    strcpy((char*)data.data() + nameoffs, name.c_str());
    strcpy((char*)data.data() + descoffs, description.c_str());
    strcpy((char*)data.data() + veroffs, versionString.c_str());

    return packet::serialise::cmp(buffer, size, PHYOIP_CMP_PEERINFO, data.data(), data.size());
}

int packet::serialise::cmpReqPeerInfo(uint8_t* buffer, size_t size) { return packet::serialise::cmp(buffer, size, PHYOIP_CMP_REQPEERINFO, nullptr, 0); }
