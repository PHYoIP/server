
# author        Oliver Blaser
# date          19.03.2026
# copyright     GPL-3.0 - Copyright (c) 2026 Oliver Blaser

cmake_minimum_required(VERSION 3.13)

project(PHYoIP-server)



set(BINNAME phyoip-server)

if(_DEBUG)
    add_definitions(-D_DEBUG)
    add_definitions(-DCONFIG_LOG_LEVEL=4)
    add_definitions(-DCONFIG_LOG_MAX_LEVEL=4)
else()
    add_definitions(-DCONFIG_LOG_LEVEL=2)
    add_definitions(-DCONFIG_LOG_MAX_LEVEL=2)
endif()

include_directories(
    ../../src/
    ../../common/include
    ../../common/util
    ../../protocol/include
)

set(SOURCES
    ../../src/main.cpp
    ../../common/src/ansi-esc.cpp
    ../../common/src/packet.cpp
    ../../common/src/serial.cpp
    ../../common/src/socket.cpp
    ../../common/src/string.cpp
    ../../common/util/util/time.c
    ../../src/client.cpp
    ../../src/gateway.cpp
    ../../src/server.cpp
)



add_executable(${BINNAME} ${SOURCES})

target_compile_options(${BINNAME} PRIVATE
    -Wall
    # -Wextra
    # -Wpedantic
    -Werror=format
    -Werror=overflow
    -Werror=return-type
    -Werror=array-bounds
    # -Werror=unused-parameter
    -Wdouble-promotion

    $<$<COMPILE_LANGUAGE:C>:
        -Werror=discarded-qualifiers
        -Werror=implicit
        -Werror=incompatible-pointer-types
        -Werror=int-conversion
        -Werror=pointer-sign
    >

    $<$<COMPILE_LANGUAGE:CXX>:
        -Werror=reorder
        -Werror=switch
        # -Wno-psabi
        # -Wuseless-cast
        # -Wsuggest-override
    >
)
