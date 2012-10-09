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

#ifndef _CCSIP_PLATFORM_TIMERS_H_
#define _CCSIP_PLATFORM_TIMERS_H_

#include "cpr_types.h"
#include "cpr_timers.h"
#include "ccsip_platform.h"
#include "ccsip_pmh.h"

#define LINE_NOT_FOUND -1

/*
 * Defintions
 */
typedef void (*sipTimerCallbackFn_t)(cprTimer_t pTmrBlk);

typedef struct
{
    cprTimer_t  timer;
    cprTimer_t  reg_timer;
    int         line;
    char*        message_buffer;
    int         message_buffer_len;
    sipMethod_t message_type;
    cpr_ip_addr_t   ipaddr;
    uint16_t    port;
    boolean     outstanding;
} sipPlatformUITimer_t;

typedef struct
{
    cprTimer_t timer;
    int        line;
    cpr_ip_addr_t   ipaddr;
    uint16_t   port;
} sipPlatformUIExpiresTimer_t;

typedef struct
{
    cprTimer_t timer;
    int        line;
    boolean    started;
} sipPlatformSupervisionTimer_t;


extern sipPlatformUITimer_t sipPlatformUISMSubNotTimers[]; // Array of timers

/*
 * Prototypes
 */
int
sip_platform_timers_init(void);
void
sip_platform_post_timer(uint32_t cmd, void *data);
void
sip_platform_msg_timers_init(void);
int
sip_platform_msg_timer_start(uint32_t msec,
                             void *data,
                             int line,
                             char *message_buffer,
                             int message_buffer_len,
                             int message_type,
                             cpr_ip_addr_t *ipaddr,
                             uint16_t port,
                             boolean isRegister);
void
sip_platform_msg_timer_stop(int line);
boolean
sip_platform_msg_timer_outstanding_get(int line);
void
sip_platform_msg_timer_outstanding_set(int line, boolean value);
void
sip_platform_msg_timer_callback(void *data);
int
sip_platform_expires_timer_start(uint32_t msec,
                                 int line,
                                 cpr_ip_addr_t *ipaddr,
                                 uint16_t port);
int
sip_platform_expires_timer_stop(int line);
void
sip_platform_expires_timer_callback(void *data);
int
sip_platform_register_expires_timer_start(uint32_t msec,
                                          int line);
int
sip_platform_register_expires_timer_stop(int line);
int
sip_platform_localexpires_timer_start(uint32_t msec,
                                      int line,
                                      cpr_ip_addr_t *ipaddr,
                                      uint16_t port);
int
sip_platform_localexpires_timer_stop(int line);
void
sip_platform_localexpires_timer_callback(void *data);
int
sip_platform_msg_timer_update_destination(int line,
                                          cpr_ip_addr_t *ipaddr,
                                          uint16_t port);
sipMethod_t
sip_platform_msg_timer_messageType_get(int line);

// Supervision timer
int
sip_platform_supervision_disconnect_timer_start(uint32_t msec,
                                                int line);
int
sip_platform_supervision_disconnect_timer_stop(int line);
void
sip_platform_supervision_disconnect_timer_callback(void *data);

// Sub/Not Timers
int
sip_platform_msg_timer_subnot_start(uint32_t msec,
                                    sipPlatformUITimer_t *,
                                    uint32_t index,
                                    char *message_buffer,
                                    int message_buffer_len,
                                    int message_type,
                                    cpr_ip_addr_t *ipaddr,
                                    uint16_t port);
void
sip_platform_msg_timer_subnot_stop(sipPlatformUITimer_t *);
void
sip_platform_subnot_msg_timer_callback(void *data);
int
sip_platform_subnot_periodic_timer_start(uint32_t msec);
int
sip_platform_subnot_periodic_timer_stop(void);
void
sip_platform_subnot_periodic_timer_callback(void *data);
int
sip_platform_standby_keepalive_timer_start(uint32_t msec);
int
sip_platform_standby_keepalive_timer_stop();
void
sip_platform_standby_keepalive_callback(void *data);
int
sip_platform_unregistration_timer_start(uint32_t msec, boolean external);
int
sip_platform_unregistration_timer_stop();
void
sip_platform_unregistration_callback(void *data);
void
sip_platform_timers_shutdown(void);
int
sip_platform_notify_timer_start(uint32_t msec);
int
sip_platform_notify_timer_stop();
int
sip_platform_reg_all_fail_timer_start(uint32_t msec);
int
sip_platform_reg_all_fail_timer_stop(void);
int
sip_platform_pass_through_timer_start(uint32_t sec);
int
sip_platform_pass_through_timer_stop(void);

#endif
