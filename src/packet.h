/*
author          Oliver Blaser
date            08.04.2026
copyright       GPL-3.0 - Copyright (c) 2026 Oliver Blaser
*/

#ifndef IG_PACKET_H
#define IG_PACKET_H

#include <cstddef>
#include <cstdint>



namespace packet {
namespace serialise {

    int phyoip(uint8_t* buffer, size_t size, uint8_t proto, const uint8_t* data, size_t count);

    int cmp(uint8_t* buffer, size_t size, uint8_t type, const uint8_t* data, size_t count);
    int cmpAck(uint8_t* buffer, size_t size, uint8_t status);
    int cmpPeerInfo(uint8_t* buffer, size_t size);
    int cmpReqPeerInfo(uint8_t* buffer, size_t size);

}
}



#endif // IG_PACKET_H
