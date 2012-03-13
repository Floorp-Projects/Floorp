/* ***** BEGIN LICENSE BLOCK *****
 * Version: MPL 1.1/GPL 2.0/LGPL 2.1
 *
 * The contents of this file are subject to the Mozilla Public License Version
 * 1.1 (the "License"); you may not use this file except in compliance with
 * the License. You may obtain a copy of the License at
 * http://www.mozilla.org/MPL/
 *
 * Software distributed under the License is distributed on an "AS IS" basis,
 * WITHOUT WARRANTY OF ANY KIND, either express or implied. See the License
 * for the specific language governing rights and limitations under the
 * License.
 *
 * The Original Code is the Cisco Systems SIP Stack.
 *
 * The Initial Developer of the Original Code is
 * Cisco Systems (CSCO).
 * Portions created by the Initial Developer are Copyright (C) 2002
 * the Initial Developer. All Rights Reserved.
 *
 * Contributor(s):
 *  Enda Mannion <emannion@cisco.com>
 *  Suhas Nandakumar <snandaku@cisco.com>
 *  Ethan Hugg <ehugg@cisco.com>
 *
 * Alternatively, the contents of this file may be used under the terms of
 * either the GNU General Public License Version 2 or later (the "GPL"), or
 * the GNU Lesser General Public License Version 2.1 or later (the "LGPL"),
 * in which case the provisions of the GPL or the LGPL are applicable instead
 * of those above. If you wish to allow use of your version of this file only
 * under the terms of either the GPL or the LGPL, and not to allow others to
 * use your version of this file under the terms of the MPL, indicate your
 * decision by deleting the provisions above and replace them with the notice
 * and other provisions required by the GPL or the LGPL. If you do not delete
 * the provisions above, a recipient may use your version of this file under
 * the terms of any one of the MPL, the GPL or the LGPL.
 *
 * ***** END LICENSE BLOCK ***** */

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
