/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifndef _GSM_H_
#define _GSM_H_

#include "cpr_types.h"
#include "cpr_memory.h"
#include "cpr_ipc.h"
#include "cpr_stdio.h"

#define GSM_ERR_MSG err_msg
typedef void(* media_timer_callback_fp) (void);
void gsm_set_media_callback(media_timer_callback_fp* callback);

void gsm_set_initialized(void);
cpr_status_e gsm_send_msg(uint32_t cmd, cprBuffer_t buf, uint16_t len);
cprBuffer_t gsm_get_buffer(uint16_t size);
boolean gsm_is_idle(void);

/*
 * List of timers that the GSM task is responsible for.
 * CPR will send a msg to the GSM task when these
 * timers expire. CPR expects a timer id when the timer
 * is created, this enum serves that purpose.
 */
typedef enum {
    GSM_ERROR_ONHOOK_TIMER,
    GSM_AUTOANSWER_TIMER,
    GSM_DIAL_TIMEOUT_TIMER,
    GSM_KPML_INTER_DIGIT_TIMER,
    GSM_KPML_CRITICAL_DIGIT_TIMER,
    GSM_KPML_EXTRA_DIGIT_TIMER,
    GSM_KPML_SUBSCRIPTION_TIMER,
    GSM_MULTIPART_TONES_TIMER,
    GSM_CONTINUOUS_TONES_TIMER,
    GSM_REQ_PENDING_TIMER,
    GSM_RINGBACK_DELAY_TIMER,
    GSM_REVERSION_TIMER,
    GSM_FLASH_ONCE_TIMER,
    GSM_CAC_FAILURE_TIMER,
    GSM_TONE_DURATION_TIMER
} gsmTimerList_t;

/*
 * The common code creating the GSM timers needs to have
 * access to the gsm_msgq variable since CPR
 * needs to know where to send the timer expiration
 * message.
 */
extern cprMsgQueue_t gsm_msgq;

extern void kpml_process_msg(uint32_t cmd, void *msg);
extern void dp_process_msg(uint32_t cmd, void *msg);
extern void kpml_init(void);
extern void kpml_shutdown(void);

#endif
