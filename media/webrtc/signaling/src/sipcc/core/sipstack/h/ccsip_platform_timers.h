/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

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
