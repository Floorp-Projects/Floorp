/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifndef _CPR_WIN_DEFINES_H
#define _CPR_WIN_DEFINES_H

#include "cpr_types.h"
#include "cpr_ipc.h"
#include "cpr_locks.h"
#include "cpr_timers.h"
#include "cpr_threads.h"
#include "cpr_debug.h"
#include "cpr_memory.h"

__BEGIN_DECLS

/*
 * CPR version number structure
 */
typedef struct {
    uint8_t majorRelease;
    uint8_t minorRelease;
    uint8_t pointRelease;
} cpr_version_t;

typedef void *cprRegion_t;
typedef void *cprPool_t;
/*
 * Define handle for signals
 */
typedef void* cprSignal_t;

/*
 * socket status
 */
typedef enum
{
    CPR_SOC_SECURE,
    CPR_SOC_NONSECURE
} cpr_soc_sec_status_e;

/*
 * secure socket connection status
 */
typedef enum
{
    CPR_SOC_CONN_OK,
    CPR_SOC_CONN_WAITING,
    CPR_SOC_CONN_FAILED
} cpr_soc_connect_status_e;




cprRegion_t cprCreateRegion (const char *regionName);
cprPool_t cprCreatePool (cprRegion_t region, const char *name, uint32_t initialBuffers, uint32_t bufferSize) ;
uint16_t cprGetDepth (cprMsgQueue_t msgQueue);


void cprDisableSwap (void);
void cprEnableSwap (void);

#define TCP_PORT_RETRY_CNT  5
#define TCP_PORT_MASK           0xfff
/*
 * tbd: need to finalize placement.
 * Define range of ephemeral ports used for Cisco IP Phones.
 */
#define CIPPORT_EPH_LOW         0xC000
#define CIPPORT_EPH_HI          0xCFFF


/*
 * Msg queue information needed to hide OS differences in implementation.
 * To use msg queues, the application code may pass in a name to the
 * create function for msg queues. CPR does not use this field, it is
 * solely for the convenience of the application and to aid in debugging.
 */
typedef struct {
    const char *name;
    uint16_t queueId;
    uint16_t currentCount;
    void *handlePtr;
} cpr_msg_queue_t;




__END_DECLS

#endif
