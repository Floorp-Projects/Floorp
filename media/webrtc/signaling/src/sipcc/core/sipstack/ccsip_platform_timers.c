/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "cpr_types.h"
#include "cpr_string.h"
#include "cpr_memory.h"
#include "phntask.h"
#include "text_strings.h"
#include "ccsip_platform.h"
#include "phone_debug.h"
#include "ccsip_task.h"
#include "ccsip_pmh.h"
#include "ccsip_register.h"
#include "ccsip_core.h"
#include "ccsip_subsmanager.h"

/*
 * Constants
 */

/*
 * Globals TODO: hang off of a single SIP global
 */
sipPlatformUITimer_t sipPlatformUISMTimers[MAX_CCBS];
sipPlatformUIExpiresTimer_t sipPlatformUISMExpiresTimers[MAX_CCBS];
sipPlatformUIExpiresTimer_t sipPlatformUISMRegExpiresTimers[MAX_CCBS];
sipPlatformUIExpiresTimer_t sipPlatformUISMLocalExpiresTimers[MAX_CCBS];
// This timer will kick in after 1xx in releasing state so if 2xx gets lost
// we will be able to kill the ccb
sipPlatformSupervisionTimer_t sipPlatformSupervisionTimers[MAX_TEL_LINES];

sipPlatformUITimer_t sipPlatformUISMSubNotTimers[MAX_SCBS];
sipPlatformSupervisionTimer_t sipPlatformSubNotPeriodicTimer;
static cprTimer_t sipPlatformRegAllFailedTimer;
static cprTimer_t sipPlatformNotifyTimer;
static cprTimer_t sipPlatformStandbyKeepaliveTimer;
static cprTimer_t sipPlatformUnRegistrationTimer;
static cprTimer_t sipPassThroughTimer;
int
sip_platform_timers_init (void)
{
    static const char fname[] = "sip_platform_timers_init";
    static const char sipMsgTimerName[] = "sipMsg";
    static const char sipExpireTimerName[] = "sipExp";
    static const char sipRegTimeOutTimerName[] = "sipRegTimeout";
    static const char sipRegExpireTimerName[] = "sipRegExp";
    static const char sipLocalExpireTimerName[] = "sipLocalExp";
    static const char sipSupervisionTimerName[] = "sipSupervision";
    static const char sipSubNotTimerName[] = "sipSubNot";
    static const char sipSubNotPeriodicTimerName[] = "sipSubNotPeriodic";
    static const char sipRegAllFailedTimerName[] = "sipRegAllFailed";
    static const char sipNotifyTimerName[] = "sipNotify";
    static const char sipStandbyKeepaliveTimerName[] = "sipStandbyKeepalive";
    static const char sipUnregistrationTimerName[] = "sipUnregistration";
	static const char sipPassThroughTimerName[] = "sipPassThrough";

    int i;

    for (i = 0; i < MAX_CCBS; i++) {
        sipPlatformUISMTimers[i].timer =
            cprCreateTimer(sipMsgTimerName,
                           SIP_MSG_TIMER,
                           TIMER_EXPIRATION,
                           sip_msgq);

        sipPlatformUISMTimers[i].reg_timer =
            cprCreateTimer(sipRegTimeOutTimerName,
                           SIP_REG_TIMEOUT_TIMER,
                           TIMER_EXPIRATION,
                           sip_msgq);

        sipPlatformUISMExpiresTimers[i].timer =
            cprCreateTimer(sipExpireTimerName,
                           SIP_EXPIRES_TIMER,
                           TIMER_EXPIRATION,
                           sip_msgq);

        sipPlatformUISMRegExpiresTimers[i].timer =
            cprCreateTimer(sipRegExpireTimerName,
                           SIP_REG_EXPIRES_TIMER,
                           TIMER_EXPIRATION,
                           sip_msgq);

        sipPlatformUISMLocalExpiresTimers[i].timer =
            cprCreateTimer(sipLocalExpireTimerName,
                           SIP_LOCAL_EXPIRES_TIMER,
                           TIMER_EXPIRATION,
                           sip_msgq);

        if (!sipPlatformUISMTimers[i].timer ||
            !sipPlatformUISMTimers[i].reg_timer ||
            !sipPlatformUISMExpiresTimers[i].timer ||
            !sipPlatformUISMRegExpiresTimers[i].timer ||
            !sipPlatformUISMLocalExpiresTimers[i].timer) {
            CCSIP_DEBUG_ERROR(SIP_F_PREFIX
                              "Failed to create one or more"
                              " UISM timers: %d\n", fname, i);
            return SIP_ERROR;
        }
    }
    for (i = 0; i < MAX_TEL_LINES; i++) {
        sipPlatformSupervisionTimers[i].timer =
            cprCreateTimer(sipSupervisionTimerName,
                           SIP_SUPERVISION_TIMER,
                           TIMER_EXPIRATION,
                           sip_msgq);
    }
    for (i = 0; i < MAX_SCBS; i++) {
        sipPlatformUISMSubNotTimers[i].timer =
            cprCreateTimer(sipSubNotTimerName,
                           SIP_SUBNOT_TIMER,
                           TIMER_EXPIRATION,
                           sip_msgq);

        if (!sipPlatformUISMSubNotTimers[i].timer) {
            CCSIP_DEBUG_ERROR(SIP_F_PREFIX
                              "Failed to create Sub/Not"
                              " UISM timers: %d\n", fname, i);
            return SIP_ERROR;
        }
    }
    sipPlatformSubNotPeriodicTimer.timer =
        cprCreateTimer(sipSubNotPeriodicTimerName,
                       SIP_SUBNOT_PERIODIC_TIMER,
                       TIMER_EXPIRATION,
                       sip_msgq);

    if (!sipPlatformSubNotPeriodicTimer.timer) {
        CCSIP_DEBUG_ERROR(SIP_F_PREFIX
                          "Failed to create supervision timer: %d\n",
                          fname, i);
        return SIP_ERROR;
    }

    sipPlatformRegAllFailedTimer =
        cprCreateTimer(sipRegAllFailedTimerName,
                       SIP_REGALLFAIL_TIMER,
                       TIMER_EXPIRATION,
                       sip_msgq);
    if (!sipPlatformRegAllFailedTimer) {
        CCSIP_DEBUG_ERROR(SIP_F_PREFIX
                          "Failed to create RegAllFailed timer\n", fname);
        return SIP_ERROR;
    }
    /*
     * Create the standby cc keepalive timer used by the
     * registration Manager.
     */
    sipPlatformStandbyKeepaliveTimer =
            cprCreateTimer(sipStandbyKeepaliveTimerName,
                           SIP_KEEPALIVE_TIMER,
                           TIMER_EXPIRATION,
                           sip_msgq);

    if (!sipPlatformStandbyKeepaliveTimer) {
        CCSIP_DEBUG_ERROR(SIP_F_PREFIX
                          "Failed to create Standby"
                          " keepalive timer\n", fname);
        return SIP_ERROR;
    }
    sipPlatformUnRegistrationTimer =
            cprCreateTimer(sipUnregistrationTimerName,
                           SIP_UNREGISTRATION_TIMER,
                           TIMER_EXPIRATION,
                           sip_msgq);
    if (!sipPlatformUnRegistrationTimer) {
        CCSIP_DEBUG_ERROR(SIP_F_PREFIX
                          "Failed to create Stanby keepalive timer\n",
                          fname);
        return SIP_ERROR;
    }
    sipPlatformNotifyTimer =
            cprCreateTimer(sipNotifyTimerName,
                           SIP_NOTIFY_TIMER,
                           TIMER_EXPIRATION,
                           sip_msgq);
    if (!sipPlatformNotifyTimer) {
        CCSIP_DEBUG_ERROR(SIP_F_PREFIX
                          "Failed to create Notify timer\n", fname);
        return SIP_ERROR;
    }

	sipPassThroughTimer =
		cprCreateTimer(sipPassThroughTimerName,
					   SIP_PASSTHROUGH_TIMER,
					   TIMER_EXPIRATION,
					   sip_msgq);
	if (!sipPassThroughTimer) {
		CCSIP_DEBUG_ERROR("%s: failed to create sip PassThrough timer\n", fname);
		return SIP_ERROR;
	}

    return SIP_OK;
}

void
sip_platform_timers_shutdown (void)
{
    int i;

    for (i = 0; i < MAX_CCBS; i++) {
        sip_platform_msg_timer_stop(i);
        (void) cprDestroyTimer(sipPlatformUISMTimers[i].timer);
        sipPlatformUISMTimers[i].timer = NULL;
        (void) cprDestroyTimer(sipPlatformUISMTimers[i].reg_timer);
        sipPlatformUISMTimers[i].reg_timer = NULL;

        (void) sip_platform_expires_timer_stop(i);
        (void) cprDestroyTimer(sipPlatformUISMExpiresTimers[i].timer);
        sipPlatformUISMExpiresTimers[i].timer = NULL;

        (void) sip_platform_register_expires_timer_stop(i);
        (void) cprDestroyTimer(sipPlatformUISMRegExpiresTimers[i].timer);
        sipPlatformUISMRegExpiresTimers[i].timer = NULL;

        (void) sip_platform_localexpires_timer_stop(i);
        (void) cprDestroyTimer(sipPlatformUISMLocalExpiresTimers[i].timer);
        sipPlatformUISMLocalExpiresTimers[i].timer = NULL;
    }

    for (i = 0; i < MAX_TEL_LINES; i++) {
        (void) sip_platform_supervision_disconnect_timer_stop(i);
        (void) cprDestroyTimer(sipPlatformSupervisionTimers[i].timer);
        sipPlatformSupervisionTimers[i].timer = NULL;
    }

    for (i = 0; i < MAX_SCBS; i++) {
        sip_platform_msg_timer_subnot_stop(&sipPlatformUISMSubNotTimers[i]);
        (void) cprDestroyTimer(sipPlatformUISMSubNotTimers[i].timer);
        sipPlatformUISMSubNotTimers[i].timer = NULL;
    }
    (void) sip_platform_subnot_periodic_timer_stop();
    (void) cprDestroyTimer(sipPlatformSubNotPeriodicTimer.timer);
    sipPlatformSubNotPeriodicTimer.timer = NULL;
    (void) sip_platform_reg_all_fail_timer_stop();
    (void) cprDestroyTimer(sipPlatformRegAllFailedTimer);
    sipPlatformRegAllFailedTimer = NULL;
    (void) sip_platform_standby_keepalive_timer_stop();
    (void) cprDestroyTimer(sipPlatformStandbyKeepaliveTimer);
    sipPlatformStandbyKeepaliveTimer = NULL;
    (void) sip_platform_unregistration_timer_stop();
    (void) cprDestroyTimer(sipPlatformUnRegistrationTimer);
    sipPlatformUnRegistrationTimer = NULL;
    (void) sip_platform_notify_timer_stop();
    (void) cprDestroyTimer(sipPlatformNotifyTimer);
    sipPlatformNotifyTimer = NULL;
	(void) sip_platform_pass_through_timer_stop();
	(void) cprDestroyTimer(sipPassThroughTimer);
	sipPassThroughTimer = NULL;
}

/********************************************************
 *
 * Message timer support functions for SIP SM
 *
 ********************************************************/
void
sip_platform_msg_timers_init (void)
{
    static const char fname[] = "sip_platform_msg_timers_init";
    static long timer_init_complete = 0;
    int i;
    cprTimer_t timer, reg_timer;

    for (i = 0; i < MAX_CCBS; i++) {
        if (timer_init_complete) {
            if ((cprCancelTimer(sipPlatformUISMTimers[i].timer)
                    == CPR_FAILURE) ||
                (cprCancelTimer(sipPlatformUISMTimers[i].reg_timer)
                    == CPR_FAILURE)) {
                CCSIP_DEBUG_STATE(get_debug_string(DEBUG_GENERAL_FUNCTIONCALL_FAILED),
                                  fname, "cprCancelTimer");
            }
        }
        timer = sipPlatformUISMTimers[i].timer;
        reg_timer = sipPlatformUISMTimers[i].reg_timer;

        if (sipPlatformUISMTimers[i].message_buffer != NULL) {
            cpr_free(sipPlatformUISMTimers[i].message_buffer);
            sipPlatformUISMTimers[i].message_buffer = NULL;
            sipPlatformUISMTimers[i].message_buffer_len = 0;
        }

        memset(&sipPlatformUISMTimers[i], 0, sizeof(sipPlatformUITimer_t));
        sipPlatformUISMTimers[i].timer = timer;
        sipPlatformUISMTimers[i].reg_timer = reg_timer;
    }
    timer_init_complete = 1;
    return;
}


int
sip_platform_msg_timer_start (uint32_t msec,
                              void *data,
                              int idx,
                              char *message_buffer,
                              int message_buffer_len,
                              int message_type,
                              cpr_ip_addr_t *ipaddr,
                              uint16_t port,
                              boolean isRegister)
{
    static const char fname[] = "sip_platform_msg_timer_start";
    cprTimer_t timer;

    /* validate index */
    if ((idx < MIN_TEL_LINES) || (idx >= MAX_CCBS)) {
        CCSIP_DEBUG_ERROR(get_debug_string(DEBUG_LINE_NUMBER_INVALID),
                          fname, idx);
        return SIP_ERROR;
    }

    /* validate length */
    if (message_buffer_len >= SIP_UDP_MESSAGE_SIZE) {
        CCSIP_DEBUG_ERROR(get_debug_string(DEBUG_MSG_BUFFER_TOO_BIG),
                          fname, message_buffer_len);
        return SIP_ERROR;
    }

    /* stop the timer if it is running */
    if (cprCancelTimer(sipPlatformUISMTimers[idx].timer) == CPR_FAILURE) {
        CCSIP_DEBUG_STATE(get_debug_string(DEBUG_SIP_FUNCTIONCALL_FAILED),
                          idx, 0, fname, "cprCancelTimer");
        return SIP_ERROR;
    }
    if (cprCancelTimer(sipPlatformUISMTimers[idx].reg_timer) == CPR_FAILURE) {
        CCSIP_DEBUG_STATE(get_debug_string(DEBUG_SIP_FUNCTIONCALL_FAILED),
                          idx, 0, fname, "cprCancelTimer");
        return SIP_ERROR;
    }

    if (sipPlatformUISMTimers[idx].message_buffer == NULL) {
        sipPlatformUISMTimers[idx].message_buffer = (char *)cpr_malloc(message_buffer_len+1);
        if (sipPlatformUISMTimers[idx].message_buffer == NULL) return SIP_ERROR;
    }
    else if (message_buffer != sipPlatformUISMTimers[idx].message_buffer) {
        cpr_free(sipPlatformUISMTimers[idx].message_buffer);
        sipPlatformUISMTimers[idx].message_buffer = (char *)cpr_malloc(message_buffer_len+1);
        if (sipPlatformUISMTimers[idx].message_buffer == NULL) return SIP_ERROR;
    }

    sipPlatformUISMTimers[idx].message_buffer_len = message_buffer_len;
    sipPlatformUISMTimers[idx].message_buffer[message_buffer_len] = '\0';
    memcpy(sipPlatformUISMTimers[idx].message_buffer, message_buffer,
           message_buffer_len);
    sipPlatformUISMTimers[idx].message_type = (sipMethod_t) message_type;
    sipPlatformUISMTimers[idx].ipaddr = *ipaddr;
    sipPlatformUISMTimers[idx].port = port;

    /* start the timer */
    if (isRegister) {
        timer = sipPlatformUISMTimers[idx].reg_timer;
    } else {
        timer = sipPlatformUISMTimers[idx].timer;
    }

    if (cprStartTimer(timer, msec, data) == CPR_FAILURE) {
        CCSIP_DEBUG_STATE(get_debug_string(DEBUG_SIP_FUNCTIONCALL_FAILED),
                          idx, 0, fname, "cprStartTimer");
        cpr_free(sipPlatformUISMTimers[idx].message_buffer);
        sipPlatformUISMTimers[idx].message_buffer = NULL;
        sipPlatformUISMTimers[idx].message_buffer_len = 0;
        return SIP_ERROR;
    }
    sipPlatformUISMTimers[idx].outstanding = TRUE;
    return SIP_OK;
}


void
sip_platform_msg_timer_stop (int idx)
{
    static const char fname[] = "sip_platform_msg_timer_stop";

    if ((idx < MIN_TEL_LINES) || (idx >= MAX_CCBS)) {
        CCSIP_DEBUG_ERROR(get_debug_string(DEBUG_LINE_NUMBER_INVALID),
                          fname, idx);
        return;
    }

    if ((cprCancelTimer(sipPlatformUISMTimers[idx].timer) == CPR_FAILURE) ||
        (cprCancelTimer(sipPlatformUISMTimers[idx].reg_timer) == CPR_FAILURE)) {
        CCSIP_DEBUG_STATE(get_debug_string(DEBUG_SIP_FUNCTIONCALL_FAILED),
                          idx, 0, fname, "cprCancelTimer");
        return;
    }
    sipPlatformUISMTimers[idx].outstanding = FALSE;
}


boolean
sip_platform_msg_timer_outstanding_get (int idx)
{
    return sipPlatformUISMTimers[idx].outstanding;
}


void
sip_platform_msg_timer_outstanding_set (int idx, boolean value)
{
    sipPlatformUISMTimers[idx].outstanding = value;
}


int
sip_platform_msg_timer_update_destination (int idx,
                                           cpr_ip_addr_t *ipaddr,
                                           uint16_t port)
{
    static const char fname[] = "sip_platform_msg_timer_update_destination";

    if ((idx < TEL_CCB_START) || (idx > REG_BACKUP_CCB)) {
        CCSIP_DEBUG_ERROR(get_debug_string(DEBUG_LINE_NUMBER_INVALID),
                          fname, idx);
        return SIP_ERROR;
    }

    if (ipaddr == NULL) {
        sipPlatformUISMExpiresTimers[idx].ipaddr = ip_addr_invalid;
    } else {
        sipPlatformUISMTimers[idx].ipaddr = *ipaddr;
    }

    sipPlatformUISMTimers[idx].port = port;

    return SIP_OK;
}

/********************************************************
 *
 * Expires timer support functions for SIP SM
 *
 ********************************************************/
int
sip_platform_expires_timer_start (uint32_t msec,
                                  int idx,
                                  cpr_ip_addr_t *ipaddr,
                                  uint16_t port)
{
    static const char fname[] = "sip_platform_expires_timer_start";

    if (sip_platform_expires_timer_stop(idx) == SIP_ERROR) {
        return SIP_ERROR;
    }

    if (ipaddr == NULL) {
        sipPlatformUISMExpiresTimers[idx].ipaddr = ip_addr_invalid;
    } else {
        sipPlatformUISMExpiresTimers[idx].ipaddr = *ipaddr;
    }

    sipPlatformUISMExpiresTimers[idx].port = port;

    //sip_platform_expires_timer_callback
    if (cprStartTimer(sipPlatformUISMExpiresTimers[idx].timer, msec,
                      (void *) (long)idx) == CPR_FAILURE) {
        CCSIP_DEBUG_ERROR(get_debug_string(DEBUG_SIP_FUNCTIONCALL_FAILED),
                          idx, 0, fname, "cprStartTimer");
        return SIP_ERROR;
    }

    return SIP_OK;
}


int
sip_platform_expires_timer_stop (int idx)
{
    static const char fname[] = "sip_platform_expires_timer_stop";

    if ((idx < MIN_TEL_LINES) || (idx >= MAX_CCBS)) {
        CCSIP_DEBUG_ERROR(get_debug_string(DEBUG_LINE_NUMBER_INVALID),
                          fname, idx);
        return SIP_ERROR;
    }

    if (cprCancelTimer(sipPlatformUISMExpiresTimers[idx].timer)
            == CPR_FAILURE) {
        CCSIP_DEBUG_STATE(get_debug_string(DEBUG_SIP_FUNCTIONCALL_FAILED),
                          idx, 0, fname, "cprCancelTimer");
        return SIP_ERROR;
    }

    return SIP_OK;
}


int
sip_platform_register_expires_timer_start (uint32_t msec, int idx)
{
    static const char fname[] = "sip_platform_register_expires_timer_start";

    if (sip_platform_register_expires_timer_stop(idx) == SIP_ERROR) {
        return SIP_ERROR;
    }

    if (cprStartTimer(sipPlatformUISMRegExpiresTimers[idx].timer, msec,
                      (void *)(long) idx) == CPR_FAILURE) {
        CCSIP_DEBUG_ERROR(get_debug_string(DEBUG_SIP_FUNCTIONCALL_FAILED),
                          idx, 0, fname, "cprStartTimer");
        return SIP_ERROR;
    }

    return SIP_OK;
}


int
sip_platform_register_expires_timer_stop (int idx)
{
    static const char fname[] = "sip_platform_register_expires_timer_stop";

    if ((idx < MIN_TEL_LINES) || (idx >= MAX_CCBS)) {
        CCSIP_DEBUG_ERROR(get_debug_string(DEBUG_LINE_NUMBER_INVALID),
                          fname, idx);
        return SIP_ERROR;
    }

    if (cprCancelTimer(sipPlatformUISMRegExpiresTimers[idx].timer)
            == CPR_FAILURE) {
        CCSIP_DEBUG_STATE(get_debug_string(DEBUG_SIP_FUNCTIONCALL_FAILED),
                          idx, 0, fname, "cprCancelTimer");
        return SIP_ERROR;
    }

    return SIP_OK;
}

/********************************************************
 *
 * Local Expires timer support functions for SIP SM
 *
 ********************************************************/
int
sip_platform_localexpires_timer_start (uint32_t msec,
                                       int idx,
                                       cpr_ip_addr_t *ipaddr,
                                       uint16_t port)
{
    static const char fname[] = "sip_platform_localexpires_timer_start";

    if (sip_platform_localexpires_timer_stop(idx) == SIP_ERROR) {
        return SIP_ERROR;
    }

    sipPlatformUISMLocalExpiresTimers[idx].ipaddr = *ipaddr;
    sipPlatformUISMLocalExpiresTimers[idx].port = port;

    //sip_platform_localexpires_timer_callback
    if (cprStartTimer(sipPlatformUISMLocalExpiresTimers[idx].timer, msec,
                      (void *)(long) idx) == CPR_FAILURE) {
        CCSIP_DEBUG_STATE(get_debug_string(DEBUG_SIP_FUNCTIONCALL_FAILED),
                          idx, 0, fname, "cprStartTimer");
        return SIP_ERROR;
    }

    return SIP_OK;
}


int
sip_platform_localexpires_timer_stop (int idx)
{
    static const char fname[] = "sip_platform_localexpires_timer_stop";

    if ((idx < MIN_TEL_LINES) || (idx >= MAX_CCBS)) {
        CCSIP_DEBUG_ERROR(get_debug_string(DEBUG_LINE_NUMBER_INVALID),
                          fname, idx);
        return SIP_ERROR;
    }

    if (cprCancelTimer(sipPlatformUISMLocalExpiresTimers[idx].timer)
            == CPR_FAILURE) {
        CCSIP_DEBUG_STATE(get_debug_string(DEBUG_SIP_FUNCTIONCALL_FAILED),
                          idx, 0, fname, "cprCancelTimer");
        return SIP_ERROR;
    }

    return SIP_OK;
}


sipMethod_t
sip_platform_msg_timer_messageType_get (int idx)
{
    if ((idx >= TEL_CCB_START) && (idx <= REG_BACKUP_CCB)) {
        if (sipPlatformUISMTimers[idx].outstanding) {
            return sipPlatformUISMTimers[idx].message_type;
        }
    }
    return sipMethodUnknown;
}

/*
 * Call disconnect timer
 */
int
sip_platform_supervision_disconnect_timer_start (uint32_t msec, int idx)
{
    static const char fname[] = "sip_platform_supervision_disconnect_timer_start";

    if (sip_platform_supervision_disconnect_timer_stop(idx) == SIP_ERROR) {
        return SIP_ERROR;
    }

    if (cprStartTimer(sipPlatformSupervisionTimers[idx].timer, msec,
                      (void *)(long) idx) == CPR_FAILURE) {
        CCSIP_DEBUG_STATE(get_debug_string(DEBUG_SIP_FUNCTIONCALL_FAILED),
                          idx, 0, fname, "cprStartTimer");
        return SIP_ERROR;
    }

    return SIP_OK;
}


int
sip_platform_supervision_disconnect_timer_stop (int idx)
{
    static const char fname[] = "sip_platform_supervision_disconnect_timer_stop";

    if ((idx < TEL_CCB_START) || (idx > TEL_CCB_END)) {
        CCSIP_DEBUG_STATE(get_debug_string(DEBUG_LINE_NUMBER_INVALID), fname, idx);
        return SIP_ERROR;
    }

    if (cprCancelTimer(sipPlatformSupervisionTimers[idx].timer)
            == CPR_FAILURE) {
        CCSIP_DEBUG_STATE(get_debug_string(DEBUG_SIP_FUNCTIONCALL_FAILED),
                          idx, 0, fname, "cprCancelTimer");
        return SIP_ERROR;
    }

    return SIP_OK;
}

void
sip_platform_post_timer (uint32_t cmd, void *data)
{
    static const char fname[] = "sip_platform_post_timer";
    uint32_t *timer_msg = NULL;

    /* grab msg buffer */
    timer_msg = (uint32_t *) SIPTaskGetBuffer(sizeof(uint32_t));
    if (!timer_msg) {
        CCSIP_DEBUG_ERROR(get_debug_string(DEBUG_SYSBUF_UNAVAILABLE), fname);
        return;
    }
    *timer_msg = (long) data;

    /* Put it on the SIP message queue */
    if (SIPTaskSendMsg(cmd, (cprBuffer_t) timer_msg, sizeof(uint32_t), NULL)
            == CPR_FAILURE) {
        cpr_free(timer_msg);
        CCSIP_DEBUG_ERROR(SIP_F_PREFIX "Send msg failed.\n", fname);
    }
    return;
}


/****************************************************
 * Timer functions for Subscribe / Notify operations
 ****************************************************/

int
sip_platform_msg_timer_subnot_start (uint32_t msec,
                                     sipPlatformUITimer_t *timer_p,
                                     uint32_t id,
                                     char *message_buffer,
                                     int message_buffer_len,
                                     int message_type,
                                     cpr_ip_addr_t *ipaddr,
                                     uint16_t port)
{
    static const char fname[] = "sip_platform_msg_timer_start_subnot";

    sip_platform_msg_timer_subnot_stop(timer_p);

    if (message_buffer_len > SIP_UDP_MESSAGE_SIZE) {
        CCSIP_DEBUG_ERROR(get_debug_string(DEBUG_MSG_BUFFER_TOO_BIG),
                          fname, message_buffer_len);
        return SIP_ERROR;
    }

    if (timer_p->message_buffer == NULL) {
        timer_p->message_buffer = (char *)cpr_malloc(message_buffer_len+1);
        if (timer_p->message_buffer == NULL) return SIP_ERROR;
    }
    else if (timer_p->message_buffer != message_buffer) {
        cpr_free(timer_p->message_buffer);
        timer_p->message_buffer = (char *)cpr_malloc(message_buffer_len+1);
        if (timer_p->message_buffer == NULL) return SIP_ERROR;
    }

    timer_p->message_buffer_len = message_buffer_len;
    timer_p->message_buffer[message_buffer_len] = '\0';
    memcpy(timer_p->message_buffer, message_buffer,
           message_buffer_len);
    timer_p->message_type =
        (sipMethod_t) message_type;
    timer_p->ipaddr = *ipaddr;
    timer_p->port = port;

    if (cprStartTimer(timer_p->timer, msec, (void *)(long)id) == CPR_FAILURE) {
        CCSIP_DEBUG_ERROR(SIP_F_PREFIX "%s failed\n",
                          fname, "cprStartTimer");
        cpr_free(timer_p->message_buffer);
        timer_p->message_buffer = NULL;
        timer_p->message_buffer_len = 0;
        return SIP_ERROR;
    }

    return SIP_OK;

}

void
sip_platform_msg_timer_subnot_stop (sipPlatformUITimer_t *timer_p)
{
    static const char fname[] = "sip_platform_msg_timer_stop_subnot";

    if (timer_p->message_buffer != NULL) {
        cpr_free(timer_p->message_buffer);
        timer_p->message_buffer = NULL;
    }
    if (cprCancelTimer(timer_p->timer) == CPR_FAILURE) {
        CCSIP_DEBUG_STATE(DEB_F_PREFIX "%s failed\n",
                          DEB_F_PREFIX_ARGS(SIP_TIMER, fname), "cprCancelTimer");
        return;
    }
}

void
sip_platform_subnot_msg_timer_callback (void *data)
{
    sip_platform_post_timer(SIP_TMR_MSG_RETRY_SUBNOT, data);
}

int
sip_platform_subnot_periodic_timer_start (uint32_t msec)
{
    static const char fname[] = "sip_platform_subnot_periodic_timer_start";

    if (sip_platform_subnot_periodic_timer_stop() == SIP_ERROR) {
        return SIP_ERROR;
    }

    if (cprStartTimer(sipPlatformSubNotPeriodicTimer.timer, msec, (void *) 0)
            == CPR_FAILURE) {
        CCSIP_DEBUG_STATE(get_debug_string(DEBUG_SIP_FUNCTIONCALL_FAILED),
                          -1, 0, fname, "cprStartTimer");
        return SIP_ERROR;
    }
    sipPlatformSubNotPeriodicTimer.started = TRUE;
    return SIP_OK;
}

int
sip_platform_subnot_periodic_timer_stop (void)
{
    static const char fname[] = "sip_platform_subnot_periodic_timer_stop";

    if (sipPlatformSubNotPeriodicTimer.started == TRUE) {
        if (cprCancelTimer(sipPlatformSubNotPeriodicTimer.timer)
                == CPR_FAILURE) {
            CCSIP_DEBUG_STATE(get_debug_string(DEBUG_SIP_FUNCTIONCALL_FAILED),
                              -1, 0, fname, "cprCancelTimer");
            return SIP_ERROR;
        }
    }
    sipPlatformSubNotPeriodicTimer.started = FALSE;
    return SIP_OK;
}

void
sip_platform_subnot_periodic_timer_callback (void *data)
{
    sip_platform_post_timer(SIP_TMR_PERIODIC_SUBNOT, data);
}

/**
 ** sip_platform_reg_all_fail_timer_start
 *  Starts a timer when all registrations fail.
 *
 *  @param  msec Value of the timer to be started
 *
 *  @return SIP_OK if timer could be started; else  SIP_ERROR
 *
 */
int
sip_platform_reg_all_fail_timer_start (uint32_t msec)
{

    static const char fname[] = "sip_platform_reg_all_fail_timer_start";
    if (sip_platform_reg_all_fail_timer_stop() == SIP_ERROR) {
        return SIP_ERROR;
    }

    if (cprStartTimer(sipPlatformRegAllFailedTimer, msec, NULL) == CPR_FAILURE) {
        CCSIP_DEBUG_STATE(get_debug_string(DEBUG_SIP_FUNCTIONCALL_FAILED),
                          0, 0, fname, "cprStartTimer");
        return SIP_ERROR;
    }
    CCSIP_DEBUG_REG_STATE(DEB_F_PREFIX
        "Timer started for %u msecs", DEB_F_PREFIX_ARGS(SIP_TIMER, fname), msec);
    return SIP_OK;
}

/**
 ** sip_platform_reg_all_fail_timer_stop
 *  Stops the Reg-All Fail timer
 *
 *  @param  none
 *
 *  @return SIP_OK if timer could be stopped; else  SIP_ERROR
 *
 */
int
sip_platform_reg_all_fail_timer_stop (void)
{
    static const char fname[] = "sip_platform_reg_all_fail_timer_stop";

    if (cprCancelTimer(sipPlatformRegAllFailedTimer) == CPR_FAILURE) {
        CCSIP_DEBUG_STATE(get_debug_string(DEBUG_SIP_FUNCTIONCALL_FAILED),
                          0, 0, fname, "cprCancelTimer");
        return SIP_ERROR;
    }
    return SIP_OK;
}

/****************************************************
 * Timer functions for standby cc keepalive operations
 ****************************************************/
int
sip_platform_standby_keepalive_timer_start (uint32_t msec)
{
    static const char fname[] = "sip_platform_standby_keepalive_timer_start";

    if (sip_platform_standby_keepalive_timer_stop() == SIP_ERROR) {
        return SIP_ERROR;
    }

    if (cprStartTimer(sipPlatformStandbyKeepaliveTimer, msec, NULL)
            == CPR_FAILURE) {
        CCSIP_DEBUG_STATE(get_debug_string(DEBUG_SIP_FUNCTIONCALL_FAILED),
                          0, 0, fname, "cprStartTimer");
        return SIP_ERROR;
    }
    CCSIP_DEBUG_STATE(DEB_F_PREFIX
        "Timer started for %u msecs", DEB_F_PREFIX_ARGS(SIP_TIMER, fname), msec);
    return SIP_OK;
}

int
sip_platform_standby_keepalive_timer_stop ()
{
    static const char fname[] = "sip_platform_standby_keepalive_timer_stop";

    if (cprCancelTimer(sipPlatformStandbyKeepaliveTimer) == CPR_FAILURE) {
        CCSIP_DEBUG_STATE(get_debug_string(DEBUG_SIP_FUNCTIONCALL_FAILED),
                          0, 0, fname, "cprCancelTimer");
        return SIP_ERROR;
    }
    return SIP_OK;
}

int
sip_platform_unregistration_timer_start (uint32_t msec, boolean external)
{
    static const char fname[] = "sip_platform_unregistration_timer_start";

    if (sip_platform_unregistration_timer_stop() == SIP_ERROR) {
        return SIP_ERROR;
    }

    if (cprStartTimer(sipPlatformUnRegistrationTimer, msec, (void *)(long)external)
            == CPR_FAILURE) {
        CCSIP_DEBUG_STATE(get_debug_string(DEBUG_SIP_FUNCTIONCALL_FAILED),
                          0, 0, fname, "cprStartTimer");
        return SIP_ERROR;
    }
    CCSIP_DEBUG_STATE(DEB_F_PREFIX
        "Timer started for %u msecs", DEB_F_PREFIX_ARGS(SIP_TIMER, fname), msec);
    return SIP_OK;
}

int
sip_platform_unregistration_timer_stop ()
{
    static const char fname[] = "sip_platform_unregistration_timer_stop";

    if (cprCancelTimer(sipPlatformUnRegistrationTimer) == CPR_FAILURE) {
        CCSIP_DEBUG_STATE(get_debug_string(DEBUG_SIP_FUNCTIONCALL_FAILED),
                          0, 0, fname, "cprCancelTimer");
        return SIP_ERROR;
    }
    return SIP_OK;
}

void
sip_platform_unregistration_callback (void *data)
{
    sip_platform_post_timer(SIP_TMR_SHUTDOWN_PHASE2, data);
}

int
sip_platform_notify_timer_start (uint32_t msec)
{
    static const char fname[] = "sip_platform_notify_timer_start";

    if (sip_platform_notify_timer_stop() == SIP_ERROR) {
        return SIP_ERROR;
    }

    if (cprStartTimer(sipPlatformNotifyTimer, msec, NULL) == CPR_FAILURE) {
        CCSIP_DEBUG_STATE(get_debug_string(DEBUG_SIP_FUNCTIONCALL_FAILED),
                          0, 0, fname, "cprStartTimer");
        return SIP_ERROR;
    }
    CCSIP_DEBUG_STATE(DEB_F_PREFIX
        "Timer started for %u msecs", DEB_F_PREFIX_ARGS(SIP_TIMER, fname), msec);
    return SIP_OK;
}

int
sip_platform_notify_timer_stop ()
{
    static const char fname[] = "sip_platform_notify_timer_stop";

    if (cprCancelTimer(sipPlatformNotifyTimer) == CPR_FAILURE) {
        CCSIP_DEBUG_STATE(get_debug_string(DEBUG_SIP_FUNCTIONCALL_FAILED),
                          0, 0, fname, "cprCancelTimer");
        return SIP_ERROR;
    }
    return SIP_OK;
}

/***********************************************
  *         PassThrough Timer
 ***********************************************
  ** sip_platform_pass_through_timer_start
  *  Starts a timer when all registrations fail.
  *
  *  @param  sec Value of the timer to be started
  *
  *  @return SIP_OK if timer could be started; else  SIP_ERROR
  *
  */
int
sip_platform_pass_through_timer_start (uint32_t sec)
{
	static const char fname[] = "sip_platform_pass_through_timer_start";

	if (sip_platform_pass_through_timer_stop() == SIP_ERROR) {
		return SIP_ERROR;
	}
	if (cprStartTimer(sipPassThroughTimer, sec*1000, NULL) == CPR_FAILURE) {
		CCSIP_DEBUG_STATE(get_debug_string(DEBUG_SIP_FUNCTIONCALL_FAILED),
				 		  0, 0, fname, "cprStartTimer");
		return SIP_ERROR;
	}

	CCSIP_DEBUG_REG_STATE("%s: Regmgr Pass Through Timer started for %u secs", fname, sec);
	return SIP_OK;
}

 /**
  ** sip_platform_pass_through_timer_stop
  *  Stops the Pass Through timer
  *
  *  @param  none
  *
  *  @return SIP_OK if timer could be stopped; else  SIP_ERROR
  *
  */
int
sip_platform_pass_through_timer_stop (void)
{
	static const char fname[] = "sip_platform_pass_through_timer_stop";

	if (cprCancelTimer(sipPassThroughTimer) == CPR_FAILURE) {
		CCSIP_DEBUG_STATE(get_debug_string(DEBUG_SIP_FUNCTIONCALL_FAILED),
						  0, 0, fname, "cprCancelTimer");
		return SIP_ERROR;
	}
	return SIP_OK;
}
