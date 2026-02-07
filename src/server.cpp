/*
author          Oliver Blaser
date            07.02.2026
copyright       GPL-3.0 - Copyright (c) 2026 Oliver Blaser
*/

#include <cstddef>
#include <cstdint>

#include "server.h"
#include "util/time.h"


#define LOG_MODULE_LEVEL LOG_LEVEL_DBG
#define LOG_MODULE_NAME  SRV
#include "common/log.h"



enum
{
    S_init = 0,
    S_boot,
    S_idle,

    S_terminate,
    S_kill,
};



void server::Server::task(Server* srv)
{
    srv->sd.setStatus(thread::Status::booting);

    while (!srv->sd.sigkill())
    {
        switch (srv->state)
        {
        case S_init:
            srv->state = S_boot;
            break;

        case S_boot:
            srv->sd.setStatus(thread::Status::running);
            srv->state = S_idle;
            break;

        case S_idle:
            if (srv->sd.sigterm())
            {
                LOG_INF("terminating");
                srv->sd.setStatus(thread::Status::terminating);
                srv->state = S_terminate;
            }
            break;

        case S_terminate:
            // clean up
            for (size_t i = 0; i < 20; ++i)
            {
                printf(".");
                UTIL_sleep_ms(100);
            }
            printf("\n");
            srv->state = S_kill;
            break;

        case S_kill:
            srv->sd.kill();
            break;

        default:
            LOG_ERR("invalid state: %i", srv->state);
            break;
        }

        UTIL_sleep_ms(50);
    }

    srv->sd.setStatus(thread::Status::killed);
}
