/*
author          Oliver Blaser
date            07.02.2026
copyright       GPL-3.0 - Copyright (c) 2026 Oliver Blaser
*/

#ifndef IG_SERVER_H
#define IG_SERVER_H

#include <cstddef>
#include <cstdint>

#include "common/thread.h"



namespace server {

class SharedData : public thread::ThreadCtl
{
public:
    SharedData()
        : thread::ThreadCtl()
    {}

    virtual ~SharedData() {}
};

class Server
{
public:
    static void task(Server* srv);

public:
    Server()
        : sd(), m_port(0), state(0)
    {}

    Server(uint16_t port)
        : sd(), m_port(port), state(0)
    {}

    virtual ~Server() {}

    uint16_t port() const { return m_port; }

public:
    SharedData sd;

private:
    const uint16_t m_port;

private:
    int state;
};



} // namespace server



#endif // IG_SERVER_H
