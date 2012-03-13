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

#ifndef _CCSIP_TASK_H_
#define _CCSIP_TASK_H_

#include "cpr_types.h"
#include "cpr_ipc.h"
#include "cpr_socket.h"
#include "cpr_memory.h"

/*
 * List of timers that the SIP task is responsible for.
 * CPR will send a msg to the SIP task when these
 * timers expire. CPR expects a timer id when the timer
 * is created, this enum serves that purpose.
 */
typedef enum {
    SIP_ACK_TIMER,
    SIP_WAIT_TIMER,
    SIP_RETRY_TIMER,
    SIP_MSG_TIMER,
    SIP_EXPIRES_TIMER,
    SIP_REG_TIMEOUT_TIMER,
    SIP_REG_EXPIRES_TIMER,
    SIP_LOCAL_EXPIRES_TIMER,
    SIP_SUPERVISION_TIMER,
    SIP_GLARE_AVOIDANCE_TIMER,
    SIP_KEEPALIVE_TIMER,
    SIP_SUBNOT_TIMER,
    SIP_SUBNOT_PERIODIC_TIMER,
    SIP_PUBLISH_RETRY_TIMER,
    SIP_UNSOLICITED_TRANSACTION_TIMER,
    SIP_DIAL_TIMEOUT_TIMER,
    SIP_FAIL_OVER_START_TIMER,
    SIP_FAIL_OVER_COMPLETE_TIMER,
    SIP_FALL_BACK_START_TIMER,
    SIP_FALL_BACK_COMPLETE_TIMER,
    SIP_UNREGISTRATION_TIMER,
    SIP_REGALLFAIL_TIMER,
    SIP_NOTIFY_TIMER,
	SIP_PASSTHROUGH_TIMER
} sipTimerList_t;


/* Action Values for SIPTaskPostShutdown function */
#define SIP_INTERNAL 0
#define SIP_EXTERNAL 1
#define SIP_STOP     2

#define MAX_SIP_REASON_LENGTH  64
#define UNREG_NO_REASON        0
#define VERSION_STAMP_MISMATCH 1
/*
 * The code creating the SIP timers needs to have
 * access to the sip_msgq variable since CPR
 * needs to know where to send the timer expiration
 * message.
 */
extern cprMsgQueue_t sip_msgq;

typedef struct
{
    boolean taskInited; // FALSE
    cprMsgQueue_t msgQueue;
} sipGlobal_t;

/*
 * External Data
 */
extern sipGlobal_t sip;
extern char sipHeaderServer[];
extern char sipHeaderUserAgent[];
extern char sipUnregisterReason[];

/*
 * Prototypes
 */
void         SIPTaskInit(void);
void         SIPTaskProcessListEvent(uint32_t cmd, void *msg, void *pUsr, uint16_t len);
int          SIPTaskProcessUDPMessage(cprBuffer_t msg, uint16_t len, cpr_sockaddr_storage from);
int          SIPTaskProcessConfigChangeNotify(int32_t notify_type);
cpr_status_e SIPTaskSendMsg(uint32_t cmd, cprBuffer_t msg, uint16_t len, void *usr);
cprBuffer_t  SIPTaskGetBuffer(uint16_t size);
void         SIPTaskReinitialize(boolean checkConfig);
void         SIPTaskPostShutdown(int, int reason, const char *reasonInfo);

#endif
