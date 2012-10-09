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
 * access to the gsm_msg_queue variable since CPR
 * needs to know where to send the timer expiration
 * message.
 */
extern cprMsgQueue_t gsm_msg_queue;

extern void kpml_process_msg(uint32_t cmd, void *msg);
extern void dp_process_msg(uint32_t cmd, void *msg);
extern void kpml_init(void);
extern void kpml_shutdown(void);

#endif
