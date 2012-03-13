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

#include "cpr_types.h"
#include "cpr_stdio.h"
#include "cpr_stdlib.h"
#include "cpr_timers.h"
#include "cpr_string.h"
#include "cpr_memory.h"
#include "cpr_ipc.h"
#include "cpr_in.h"
#include "util_string.h"
#include "task.h"
#include "phntask.h"
#include "ccsip_core.h"
#include "ccsip_messaging.h"
#include "phone_debug.h"
#include "ccsip_platform.h"
#include "ccsip_platform_timers.h"
#include "ccsip_macros.h"
#include "ccsip_pmh.h"
#include "ccsip_register.h"
#include "ccsip_task.h"
#include "ccsip_credentials.h"
#include "debug.h"
#include "logmsg.h"
#include "ccsip_pmh.h"
#include "ccsip_credentials.h"
#include "dns_utils.h"
#include "config.h"
#include "sip_common_transport.h"
#include "uiapi.h"
#include "sip_common_regmgr.h"
#include "text_strings.h"
#include "sip_interface_regmgr.h"
#include "phone_platform_constants.h"
#include "ccsip_common_cb.h"
#include "misc_util.h"

extern sipPlatformUITimer_t sipPlatformUISMTimers[];
extern void *new_standby_available;
extern boolean regall_fail_attempt;
extern boolean registration_reject;

extern void ui_set_sip_registration_state(line_t line, boolean registered);
extern void ui_update_registration_state_all_lines(boolean registered);

boolean dump_reg_msg = TRUE;
boolean refresh_reg_msg = FALSE;

static ccsip_register_states_t ccsip_register_state = SIP_REG_IDLE;

/* Need an additional ack timer for the backup proxy */
static cprTimer_t ack_tmrs[MAX_REG_LINES + 1];
ccm_act_stdby_table_t CCM_Active_Standby_Table;
extern ccm_failover_table_t CCM_Failover_Table;
static boolean start_standby_monitor = TRUE;
extern boolean Is794x;

static void show_register_data(void);

/* extern function for CCM clock handler */
extern void SipNtpUpdateClockFromCCM(void);

#define SIP_REG_TMR_EXPIRE_TICKS 55000 /* msec; re-registration timer */
#define REGISTER_CMD "register"
#define SIP_REG_TMR_ACK_TICKS    32000 /* msec; supervision timer equivalent to Timer F */

static const char *ccsip_register_state_names[] = {
    "IDLE",
    "REGISTERING",
    "REGISTERED",
    "UNREGISTERING",
    "PRE_FALLBACK",
    "IN_FAILOVER",
    "POST_FAILOVER",
    "STANDBY_FAILOVER",
    "NO_CC",
    "NO_STANDBY",
    "NO_REGISTER"
};

static const char *ccsip_register_reg_state_names[] = {
    "NONE",
    "IDLE",
    "REGISTERING",
    "REGISTERED",
    "UNREGISTERING",
    "PRE_FALLBACK",
    "IN_FAILOVER",
    "POST_FAILOVER",
    "STANDBY_FAILOVER",
    "NO_CC",
    "NO_STANDBY",
    "NO_REGISTER"
};


void ccsip_handle_ev_reg_req(ccsipCCB_t *ccb, sipSMEvent_t *event);
void ccsip_handle_ev_reg_cancel(ccsipCCB_t *ccb, sipSMEvent_t *event);
void ccsip_handle_ev_unreg_2xx(ccsipCCB_t *ccb, sipSMEvent_t *event);
void ccsip_handle_ev_1xx(ccsipCCB_t *ccb, sipSMEvent_t *event);
void ccsip_handle_ev_2xx(ccsipCCB_t *ccb, sipSMEvent_t *event);
void ccsip_handle_ev_3xx(ccsipCCB_t *ccb, sipSMEvent_t *event);
void ccsip_handle_ev_4xx(ccsipCCB_t *ccb, sipSMEvent_t *event);
void ccsip_handle_ev_failure_response(ccsipCCB_t *ccb, sipSMEvent_t *event);
void ccsip_handle_ev_tmr_expire(ccsipCCB_t *ccb, sipSMEvent_t *event);
void ccsip_handle_ev_tmr_ack(ccsipCCB_t *ccb, sipSMEvent_t *event);
void ccsip_handle_ev_tmr_retry(ccsipCCB_t *ccb, sipSMEvent_t *event);
void ccsip_handle_ev_unreg_tmr_ack(ccsipCCB_t *ccb, sipSMEvent_t *event);
void ccsip_handle_ev_standby_keepalive_tmr_expire(ccsipCCB_t *ccb, sipSMEvent_t *event);
void ccsip_handle_ev_idle_tmr_expire(ccsipCCB_t *ccb, sipSMEvent_t *event);

const char *ccsip_register_state_name(ccsip_register_states_t state);
const char *ccsip_register_reg_state_name(sipRegSMStateType_t state);
const char *sip_util_reg_event2string(sipRegSMEventType_t event);
char *sip_util_reg_state2string(sipRegSMStateType_t state);
void ccsip_register_retry_timer_start(ccsipCCB_t *ccb);
void ccsip_register_cleanup(ccsipCCB_t *ccb, boolean start);
cc_int32_t ccsip_register_cmd(cc_int32_t argc, const char *argv[]);
void ccsip_register_clear_all_logs(void);
boolean ccsip_register_all_registered(void);

/* convert sip line to reg line  */
#define SIP_REG_LINE2REGLINE(line)  ((line) - (REG_CCB_START))

/* convert reg line to sip line  */
#define SIP_REG_REGLINE2LINE(line)  ((line) + (REG_CCB_START))

// Date holder
static struct {
    boolean valid;
    char datestring[MAX_SIP_DATE_LENGTH];
} ccm_date;

static const sipSMEventActionFn_t
gSIPRegSMTable[SIP_REG_STATE_END - SIP_REG_STATE_BASE + 1]
              [SIPSPI_REG_EV_END - SIPSPI_REG_EV_BASE + 1] =
{
    /* SIP_REG_STATE_IDLE
     */
    {
     /* E_SIP_REG_REG_REQ          */ ccsip_handle_ev_reg_req,
     /* E_SIP_REG_CANCEL           */ ccsip_handle_ev_default,
     /* E_SIP_REG_1xx              */ ccsip_handle_ev_default,
     /* E_SIP_REG_2xx              */ ccsip_handle_ev_default,
     /* E_SIP_REG_3xx              */ ccsip_handle_ev_default,
     /* E_SIP_REG_4xx              */ ccsip_handle_ev_default,
     /* E_SIP_REG_FAILURE_RESPONSE */ ccsip_handle_ev_default,
     /* E_SIP_REG_TMR_ACK          */ ccsip_handle_ev_default,
     /* E_SIP_REG_TMR_EXPIRE       */ ccsip_handle_ev_idle_tmr_expire,
     /* E_SIP_REG_TMR_WAIT         */ ccsip_handle_ev_default,
     /* E_SIP_REG_TMR_RETRY        */ ccsip_handle_ev_default,
     /* E_SIP_REG_CLEANUP          */ sip_regmgr_ev_default,
     },

    /* SIP_REG_STATE_REGISTERING
     */
    {
     /* E_SIP_REG_REG_REQ          */ ccsip_handle_ev_reg_req,
     /* E_SIP_REG_CANCEL           */ ccsip_handle_ev_reg_cancel,
     /* E_SIP_REG_1xx              */ ccsip_handle_ev_1xx,
     /* E_SIP_REG_2xx              */ ccsip_handle_ev_2xx,
     /* E_SIP_REG_3xx              */ ccsip_handle_ev_3xx,
     /* E_SIP_REG_4xx              */ ccsip_handle_ev_4xx,
     /* E_SIP_REG_FAILURE_RESPONSE */ ccsip_handle_ev_failure_response,
     /* E_SIP_REG_TMR_ACK          */ ccsip_handle_ev_tmr_ack,
     /* E_SIP_REG_TMR_EXPIRE       */ ccsip_handle_ev_tmr_expire,
     /* E_SIP_REG_TMR_WAIT         */ ccsip_handle_ev_default,
     /* E_SIP_REG_TMR_RETRY        */ ccsip_handle_ev_tmr_retry,
     /* E_SIP_REG_CLEANUP          */ sip_regmgr_ev_default,
     },

    /* SIP_REG_STATE_REGISTERED
     */
    {
     /* E_SIP_REG_REG_REQ          */ ccsip_handle_ev_default,
     /* E_SIP_REG_CANCEL           */ ccsip_handle_ev_reg_cancel,
     /* E_SIP_REG_1xx              */ ccsip_handle_ev_default,
     /* E_SIP_REG_2xx              */ ccsip_handle_ev_default,
     /* E_SIP_REG_3xx              */ ccsip_handle_ev_default,
     /* E_SIP_REG_4xx              */ ccsip_handle_ev_default,
     /* E_SIP_REG_FAILURE_RESPONSE */ ccsip_handle_ev_default,
     /* E_SIP_REG_TMR_ACK          */ ccsip_handle_ev_default,
     /* E_SIP_REG_TMR_EXPIRE       */ ccsip_handle_ev_tmr_expire,
     /* E_SIP_REG_TMR_WAIT         */ ccsip_handle_ev_default,
     /* E_SIP_REG_TMR_RETRY        */ ccsip_handle_ev_tmr_retry,
     /* E_SIP_REG_CLEANUP          */ sip_regmgr_ev_default,
     },

    /* SIP_REG_STATE_UNREGISTERING
     */
    {
     /* E_SIP_REG_REG_REQ          */ ccsip_handle_ev_default,
     /* E_SIP_REG_CANCEL           */ ccsip_handle_ev_reg_cancel,
     /* E_SIP_REG_1xx              */ ccsip_handle_ev_1xx,
     /* E_SIP_REG_2xx              */ ccsip_handle_ev_unreg_2xx,
     /* E_SIP_REG_3xx              */ ccsip_handle_ev_3xx,
     /* E_SIP_REG_4xx              */ ccsip_handle_ev_4xx,
     /* E_SIP_REG_FAILURE_RESPONSE */ ccsip_handle_ev_failure_response,
     /* E_SIP_REG_TMR_ACK          */ ccsip_handle_ev_unreg_tmr_ack,
     /* E_SIP_REG_TMR_EXPIRE       */ ccsip_handle_ev_default,
     /* E_SIP_REG_TMR_WAIT         */ ccsip_handle_ev_standby_keepalive_tmr_expire,
     /* E_SIP_REG_TMR_RETRY        */ ccsip_handle_ev_tmr_retry,
     /* E_SIP_REG_CLEANUP          */ sip_regmgr_ev_default,
     },

    /* SIP_REG_STATE_IN_FALLBACK
     */
    {
     /* E_SIP_REG_REG_REQ          */ sip_regmgr_ev_default,
     /* E_SIP_REG_CANCEL           */ sip_regmgr_ev_cancel,
     /* E_SIP_REG_1xx              */ ccsip_handle_ev_1xx,
     /* E_SIP_REG_2xx              */ sip_regmgr_ev_in_fallback_2xx,
     /* E_SIP_REG_3xx              */ sip_regmgr_ev_default,
     /* E_SIP_REG_4xx              */ sip_regmgr_ev_fallback_retry,
     /* E_SIP_REG_FAILURE_RESPONSE */ sip_regmgr_ev_fallback_retry,
     /* E_SIP_REG_TMR_ACK          */ sip_regmgr_ev_tmr_ack_retry,
     /* E_SIP_REG_TMR_EXPIRE       */ sip_regmgr_ev_default,
     /* E_SIP_REG_TMR_WAIT         */ sip_regmgr_ev_default,
     /* E_SIP_REG_TMR_RETRY        */ sip_regmgr_ev_tmr_ack_retry,
     /* E_SIP_REG_CLEANUP          */ sip_regmgr_ev_default,
     },
    /* SIP_REG_STATE_STABILITY_CHECK
     */
    {
     /* E_SIP_REG_REG_REQ          */ sip_regmgr_ev_default,
     /* E_SIP_REG_CANCEL           */ sip_regmgr_ev_cancel,
     /* E_SIP_REG_1xx              */ ccsip_handle_ev_1xx,
     /* E_SIP_REG_2xx              */ sip_regmgr_ev_stability_check_2xx,
     /* E_SIP_REG_3xx              */ sip_regmgr_ev_default,
     /* E_SIP_REG_4xx              */ sip_regmgr_ev_default,
     /* E_SIP_REG_FAILURE_RESPONSE */ sip_regmgr_ev_default,
     /* E_SIP_REG_TMR_ACK          */ sip_regmgr_ev_tmr_ack_retry,
     /* E_SIP_REG_TMR_EXPIRE       */ sip_regmgr_ev_default,
     /* E_SIP_REG_TMR_WAIT         */ sip_regmgr_ev_stability_check_tmr_wait,
     /* E_SIP_REG_TMR_RETRY        */ sip_regmgr_ev_tmr_ack_retry,
     /* E_SIP_REG_CLEANUP          */ sip_regmgr_ev_default,
     },
    /* SIP_REG_STATE_TOKEN_WAIT
     */
    {
     /* E_SIP_REG_REG_REQ          */ sip_regmgr_ev_default,
     /* E_SIP_REG_CANCEL           */ sip_regmgr_ev_default,
     /* E_SIP_REG_1xx              */ ccsip_handle_ev_1xx,
     /* E_SIP_REG_2xx              */ sip_regmgr_ev_token_wait_2xx,
     /* E_SIP_REG_3xx              */ sip_regmgr_ev_default,
     /* E_SIP_REG_4xx              */ ccsip_handle_ev_4xx,
     /* E_SIP_REG_FAILURE_RESPONSE */ ccsip_handle_ev_failure_response,
     /* E_SIP_REG_TMR_ACK          */ sip_regmgr_ev_tmr_ack_retry,
     /* E_SIP_REG_TMR_EXPIRE       */ sip_regmgr_ev_default,
     /* E_SIP_REG_TMR_WAIT         */ sip_regmgr_ev_token_wait_tmr_wait,
     /* E_SIP_REG_TMR_RETRY        */ sip_regmgr_ev_tmr_ack_retry,
     /* E_SIP_REG_CLEANUP          */ sip_regmgr_ev_cleanup,
     }
};

/*
 * This function gets the supported option tags from the 200 OK response
 * to the REG message sent.
 */
void
sip_get_supported_options_2xx (ccsipCCB_t *ccb, sipMessage_t *response)
{
    const char *supported;

    ccb->supported_tags = 0;
    supported = sippmh_get_cached_header_val(response, SUPPORTED);
    if (supported != NULL) {
        /*
         * Supported header is found, find the interrested supported
         * function from 2xx response from the REG msg. sent. Note that
         * the sippmh_parse_supported_require() can be used but
         * decided to minimize the impact on performance parsing all other
         * tags that are not interested during registration times and
         * vice versa making changes to sippmh_parse_supported_require()
         * to parse the tags that are not interested for other messages
         * also impact the processing time for other messages.
         */
        if (strcasestr(supported, REQ_SUPP_PARAM_CISCO_SRTP_FALLBACK)) {
            /* cisco SRTP fallback is supported by the other end */
            ccb->supported_tags |= cisco_srtp_fallback_tag;
        }
    }
}

/*
 * This function starts the Register message
 * ack timer. If no response is received from
 * the register message within four minutes,
 * this timer will pop and resend the Register
 * message.
 */
void
sip_start_ack_timer (ccsipCCB_t *ccb)
{
    uint16_t ack_timer_index;

    if (ccb->index == REG_BACKUP_CCB) {
        ack_timer_index = MAX_REG_LINES;
    } else {
        ack_timer_index = ccb->dn_line - 1;
    }

    CCSIP_DEBUG_REG_STATE( DEB_L_C_F_PREFIX " ccb->index=%d ack_timer_index=%d ", 
        DEB_L_C_F_PREFIX_ARGS(SIP_STATE, ccb->dn_line, 0, "sip_start_ack_timer"),
        ccb->index, ack_timer_index);

    if (cprStartTimer(ack_tmrs[ack_timer_index], SIP_REG_TMR_ACK_TICKS,
                      (void *)(long)ccb->index) == CPR_FAILURE) {
        CCSIP_DEBUG_ERROR(get_debug_string(DEBUG_GENERAL_FUNCTIONCALL_FAILED),
                          "sip_start_ack_timer", "cprStartTimer");
    }
}


/*
 * This function stops the Register message
 * ack timer.
 */
void
sip_stop_ack_timer (ccsipCCB_t *ccb)
{
    uint16_t ack_timer_index;

    if (ccb->index == REG_BACKUP_CCB) {
        ack_timer_index = MAX_REG_LINES;
    } else {
        ack_timer_index = ccb->dn_line - 1;
    }

    CCSIP_DEBUG_REG_STATE( DEB_L_C_F_PREFIX " ccb->index=%d ack_timer_index=%d ", 
        DEB_L_C_F_PREFIX_ARGS(SIP_STATE, ccb->dn_line, 0, "sip_stop_ack_timer"),
        ccb->index, ack_timer_index);

    if (cprCancelTimer(ack_tmrs[ack_timer_index]) == CPR_FAILURE) {
        CCSIP_DEBUG_ERROR(get_debug_string(DEBUG_GENERAL_FUNCTIONCALL_FAILED),
                          "sip_stop_ack_timer", "cprCancelTimer");
    }
}


void
sip_reg_sm_change_state (ccsipCCB_t *ccb, sipRegSMStateType_t new_state)
{
    if (g_disable_mass_reg_debug_print == FALSE) {
    CCSIP_DEBUG_REG_STATE(DEB_L_C_F_PREFIX"Registration state change: %s ---> "
                          "%s\n", DEB_L_C_F_PREFIX_ARGS(SIP_STATE, ccb->index, ccb->dn_line, "sip_reg_sm_change_state"),
                          sip_util_reg_state2string((sipRegSMStateType_t)ccb->state),
                          sip_util_reg_state2string(new_state));
    }
    ccb->state = (sipSMStateType_t) new_state;

    if (ccb->index == REG_CCB_START) {
        if ((new_state > SIP_REG_STATE_REGISTERED) || (!refresh_reg_msg)) {
            dump_reg_msg = TRUE;
        } else {
            dump_reg_msg = FALSE;
        }
    }
}


void
ccsip_handle_ev_reg_req (ccsipCCB_t *ccb, sipSMEvent_t *event)
{
    static const char fname[] = "ccsip_handle_ev_reg_req";
    char line_name[MAX_LINE_NAME_SIZE];
    int value;

    config_get_value(CFGID_PROXY_REGISTER, &value, sizeof(value));
    if (value == 0) {
        ui_set_sip_registration_state(ccb->dn_line, FALSE);
        CCSIP_DEBUG_REG_STATE(get_debug_string(DEBUG_REG_DISABLED),
                              ccb->index, ccb->dn_line, fname);

        return;
    }

    ccsip_register_clear_all_logs();

    sip_stop_ack_timer(ccb);
    sip_start_ack_timer(ccb);

    (void) sip_platform_register_expires_timer_stop(ccb->index);


    sip_util_get_new_call_id(ccb);

    ccb->authen.cred_type = 0;
    ccb->retx_counter = 0;
    config_get_line_string(CFGID_LINE_NAME, line_name, ccb->dn_line,
                           sizeof(line_name));

    config_get_value(CFGID_TIMER_REGISTER_EXPIRES, &ccb->reg.tmr_expire,
                     sizeof(ccb->reg.tmr_expire));
    ccb->reg.act_time = (int) time(NULL);

    if (sipSPISendRegister(ccb, 0, line_name, ccb->reg.tmr_expire) != TRUE) {
        // set expire timer and cleanup ccb in non-ccm mode.
        // do not change the expire timer value in ccm mode
        if (ccb->cc_type != CC_CCM) {
            ccsip_register_cleanup(ccb, TRUE);
        }
        log_clear(LOG_REG_MSG);
        log_msg(LOG_REG_MSG);
    }

    sip_reg_sm_change_state(ccb, SIP_REG_STATE_REGISTERING);
}



void
ccsip_handle_ev_default (ccsipCCB_t *ccb, sipSMEvent_t *event)
{
    if ((event->type == (int) E_SIP_REG_CANCEL) && (ccb->state == (int) SIP_REG_STATE_IDLE)) {
        (void) sip_platform_register_expires_timer_stop(ccb->index);
        ccb->authen.cred_type = 0;
        ccb->retx_counter     = 0;
        ccb->reg.tmr_expire   = 0;
        ccb->reg.act_time     = 0;
        ccsip_register_cleanup(ccb, FALSE);
    }

    /* only free SIP messages, timeouts are internal */
    if (event->type < (int) E_SIP_REG_TMR_ACK) {
        free_sip_message(event->u.pSipMessage);
    }
}


void
ccsip_handle_ev_tmr_expire (ccsipCCB_t *ccb, sipSMEvent_t *event)
{
    // Initiate registration if prime line or not in failover state
    if ((!CCM_Failover_Table.failover_started) ||
        (ccb->index == REG_CCB_START)) {
        /*If the phone is registered with CSPS, the failover_started flag
         * will always be false. Therefore, this code will always be
         * executed. */

        /* Cleanup and re-initialize CCB */
        sip_sm_call_cleanup(ccb);

        refresh_reg_msg = TRUE;
        /* send a message to the SIP_REG SM to initiate registration */
        if (ccsip_register_send_msg(SIP_REG_REQ, ccb->index) != SIP_REG_OK) {
            ccsip_register_cleanup(ccb, TRUE);
        }
    } else {
        ccsip_handle_ev_default(ccb, event);
    }
}

void
ccsip_handle_ev_idle_tmr_expire (ccsipCCB_t *ccb, sipSMEvent_t *event)
{

    if (ccb->cc_type == CC_CCM) {
        if (ccb->index == REG_BACKUP_CCB) {
            ccsip_handle_ev_tmr_expire(ccb, event);
        } else {
            ccsip_handle_ev_default(ccb, event);
        }
    } else {
        /* assuming CSPS */
        /* Cleanup and re-initialize CCB */
        sip_sm_call_cleanup(ccb);

        /* send a message to the SIP_REG SM to initiate registration */
        if (ccsip_register_send_msg(SIP_REG_REQ, ccb->index) != SIP_REG_OK) {
            ccsip_register_cleanup(ccb, TRUE);
        }
    }
}


void
ccsip_handle_ev_tmr_ack (ccsipCCB_t *ccb, sipSMEvent_t *event)
{
    log_clear(LOG_REG_AUTH_ACK_TMR);
    log_msg(LOG_REG_AUTH_ACK_TMR);

    if (ccb->cc_type == CC_CCM) {
        /*
         * regmgr - Send tmr ack event to the regmgr/
         */
        sip_regmgr_ev_tmr_ack_retry(ccb, event);
    } else {
        /*
         * regmgr - Assume it is csps
         */
        /* Cleanup and re-initialize CCB */
        sip_sm_call_cleanup(ccb);
        /* send a message to the SIP_REG SM to initiate registration */
        if (ccsip_register_send_msg(SIP_REG_REQ, ccb->index) != SIP_REG_OK) {
            ccsip_register_cleanup(ccb, TRUE);
        }
    }
}


void
ccsip_handle_ev_1xx (ccsipCCB_t *ccb, sipSMEvent_t *event)
{
    static const char fname[] = "ccsip_handle_ev_1xx";
    sipMessage_t   *response = NULL;
    int             status_code = 0;
    char            status[LOG_MAX_LEN];

    response = event->u.pSipMessage;

    if (sipGetResponseCode(response, &status_code) < 0) {
        CCSIP_DEBUG_ERROR(get_debug_string(DEBUG_REG_SIP_RESP_CODE),
                          ccb->index, ccb->dn_line, fname);
        return;
    }

    free_sip_message(response);

    switch (status_code) {
    case SIP_1XX_TRYING:
        CCSIP_DEBUG_STATE(get_debug_string(DEBUG_FUNCTION_ENTRY2),
                          ccb->index, ccb->dn_line, fname,
                          sip_util_reg_state2string((sipRegSMStateType_t)ccb->state),
                          "SIP", status_code);
        return;

    default:
        CCSIP_DEBUG_STATE(get_debug_string(DEBUG_FUNCTION_ENTRY2), ccb->index,
                          ccb->dn_line, fname,
                          sip_util_reg_state2string((sipRegSMStateType_t)ccb->state),
                          "SIP BAD", status_code);

        snprintf(status, sizeof(status), "in %d, information", status_code);
        log_clear(LOG_REG_UNSUPPORTED);
        log_msg(LOG_REG_UNSUPPORTED, status);

        ccsip_register_cleanup(ccb, TRUE);

        return;
    }
}

static boolean
ccsip_get_exp_time_2xx (ccsipCCB_t *ccb, sipContact_t *contact_info,
                        const char *expires, uint32_t *exp_time_ret)
{
    static const char fname[] = "ccsip_get_exp_time_2xx";
    boolean         exp_found = FALSE;
    char            src_addr_str[MAX_IPADDR_STR_LEN];
    char            user[MAX_LINE_NAME_SIZE];
    int             i;
    uint32_t        gmt_time; //TODO convert to time_t
    //uint32_t          diff_time; //TODO convert to time_t
    int32_t         gmt_rc;
    uint32_t        exp_time;
    uint32_t        register_delta;

    /* did the server change the expires time? */

    /*
     * Form the contact header.
     * The proxy can return multiple contacts so we will need to see which one
     * matches this REGISTER.
     */
    config_get_value(CFGID_TIMER_REGISTER_EXPIRES, &exp_time, sizeof(uint32_t));
    if (contact_info != NULL) {
        ipaddr2dotted(src_addr_str, &ccb->src_addr);
        config_get_line_string(CFGID_LINE_NAME, user, ccb->dn_line,
                               sizeof(user));

        /* is expires header for this line included in the contact? */
        for (i = 0; i < contact_info->num_locations; i++) {
            if ((sippmh_cmpURLStrings(contact_info->locations[i]->genUrl->u.sipUrl->user,
                                      user, FALSE) == 0) &&
                (strcmp(contact_info->locations[i]->genUrl->u.sipUrl->host,
                        src_addr_str) == 0)) {
                if ((contact_info->params[i].expires > 0) &&
                    (contact_info->params[i].expires < exp_time)) {
                    exp_time = contact_info->params[i].expires;
                    exp_found = TRUE;
                    CCSIP_DEBUG_REG_STATE(get_debug_string(DEBUG_REG_PROXY_EXPIRES),
                                          ccb->index, ccb->dn_line, fname);
                    break;
                }
            }
        }
    }


    if (exp_found != TRUE) {
        //expires param is not found in the CONTACT header
        // look for Expires header.
        config_get_value(CFGID_TIMER_REGISTER_EXPIRES, &exp_time,
                         sizeof(exp_time));
        if (expires) {
            gmt_rc = gmt_string_to_seconds((char *)expires,
                                           (unsigned long *)&gmt_time);
            if (gmt_rc != -1) {
                // We only want to update the expires timeout if it is lower
                // than our predefined threshold.  We don't want to allow
                // people to keep us hung up for infinite periods of time
                if (gmt_rc == 1) {
                    // We got a numeric entry in the expires field
                    if (gmt_time < exp_time) {
                        exp_time = gmt_time;
                        exp_found = TRUE;
                    }
                }
            }
        }
    }

    /*
     * Subtract some user defined period of time to account for network delay
     * or the possibility that the registration server is down and this
     * request has to be processed by a backup server after the timeout
     * occurs. If the user has not defined this value in the config, it
     * defaults to 5 seconds.
     */
    config_get_value(CFGID_TIMER_REGISTER_DELTA, &register_delta,
                     sizeof(register_delta));

    /*
     * Sanity check the register delta value as the proxy could have returned
     * a much lower registration period in its' response. The minimum
     * registration expiration is 60 seconds to prevent the phone from being
     * flooded with msgs.
     */
  if ((exp_time - register_delta) < MIN_REGISTRATION_PERIOD) { 
        CCSIP_DEBUG_REG_STATE(DEB_L_C_F_PREFIX
                "Warning - Registration period received (%d) "
                "minus configured timer_register_delta (%d) is less than "
                "%d seconds.\n", DEB_L_C_F_PREFIX_ARGS(SIP_REG, ccb->index, ccb->dn_line, fname), exp_time,
                register_delta, MIN_REGISTRATION_PERIOD);
        exp_found = FALSE;
    } else {
        exp_time = exp_time - register_delta;
        exp_found = TRUE;
    }
    *exp_time_ret = exp_time;
    return (exp_found);
}

/*
 *  Function: ccsip_check_ccm_restarted
 *
 *  Parameters:
 *      new_reg_ccb  - pointer to ccsipCCB_t of REG CCB.
 *      contact_info - Pointer to sipContact_t.
 *
 *  Description:
 *      The function detects CCM just came up i.e. it sees the phone
 *  registration as a new registration. This means CCM has restarted
 *  between a REG refresh cycle.
 *
 *  Note:
 *      CCM only sends x-cisco-newreg to the first REG that it sees
 *  from the phone as a new REG. If the phone has more than one lines,
 *  the REG of the rest of the lines will not have the x-cisco-newreg.
 *
 *  Returns:
 *      TRUE - CCM restarted is detected.
 *     FALSE - CCM restarted is not detected.
 */
static boolean
ccsip_check_ccm_restarted (sipContact_t *contact_info)
{
    int i;

    if (contact_info == NULL) {
        /* No contact info. */
        return (FALSE);
    }

    for (i = 0; i < contact_info->num_locations; i++) {
        if (contact_info->params[i].flags & SIP_CONTACT_PARM_X_CISCO_NEWREG) {
            /*
             * CCM or proxy just indicates that it sees the REG
             * as a new registration to it.  This means that the CCM or
             * proxy has restarted.
             */
            return (TRUE);
        }
    }
    return (FALSE);
}

/*
 *  Function: update_sis_protocol_version
 *
 *  Parameters:
 *      response - 200 OK message pointer 
 *
 *  Description:
 *      The function parses theupported header in response and updates
 *  SIS protocol version received. If no version is received the default
 *  SEADRAGON version gets updated
 *
 *  Returns:
 *     void 
 */

void
update_sis_protocol_version (sipMessage_t *response)
{
    const char *supported;
    char * sipver;
    supported = sippmh_get_cached_header_val(response, SUPPORTED);
    if (supported != NULL) {
       sipver = strcasestr(supported, REQ_SUPP_PARAM_CISCO_SISTAG);
       if (sipver) {
          cc_uint32_t major = SIS_PROTOCOL_MAJOR_VERSION_SEADRAGON, minor = 0, addtnl = 0;
          if ( sscanf ( &sipver[strlen(REQ_SUPP_PARAM_CISCO_SISTAG)], "%d.%d.%d", 
			         &major, &minor, &addtnl) == 3) {
            platSetSISProtocolVer ( major, minor, addtnl,REQ_SUPP_PARAM_CISCO_SISTAG); 
            return;
          } 
        }
    }
    /* All other cases we set the version to 1.0.0 */
    platSetSISProtocolVer ( SIS_PROTOCOL_MAJOR_VERSION_SEADRAGON, 0, 0,REQ_SUPP_PARAM_CISCO_SISTAG);
}

void
update_cme_sis_version (sipMessage_t *response)
{
    const char *supported;
    char * sipver;

    supported = sippmh_get_cached_header_val(response, SUPPORTED);
    if (supported != NULL) {
      sipver = strcasestr(supported, REQ_SUPP_PARAM_CISCO_CME_SISTAG);
      if (sipver) {
        cc_uint32_t major = 0, minor = 0, addtnl = 0;
        if ( sscanf( &sipver[strlen(REQ_SUPP_PARAM_CISCO_CME_SISTAG)], "%d.%d.%d",
                            &major, &minor, &addtnl) ==3) {
          platSetSISProtocolVer(major, minor, addtnl,REQ_SUPP_PARAM_CISCO_CME_SISTAG);
        }
      }
    }
}

void
ccsip_handle_ev_2xx (ccsipCCB_t *ccb, sipSMEvent_t *event)
{
    static const char fname[] = "ccsip_handle_ev_2xx";
    sipMessage_t   *response = event->u.pSipMessage;
    const char     *pViaHeaderStr = NULL;
    sipVia_t       *via = NULL;
    uint32_t        exp_time;
    //uint32_t        line_feature;
    int             dns_err_code;
    int             nat_enable = 0;
    int             nat_rx_proc_enable = 0;
    //int            last_button_index =0;
    const char     *datehdr = NULL;
    const char     *contact = NULL;
    const char     *expires = NULL;
    sipContact_t   *contact_info = NULL;
    //line_t          line_index = 0;
    //ccsipCCB_t     *line_ccb = NULL;
  

    // Extract Date header and store
    datehdr = sippmh_get_header_val(response, SIP_HEADER_DATE, NULL);
    if (datehdr) {
        // Copy date header in a global variable since it would be needed
        // to set system time
        sstrncpy(ccm_date.datestring, datehdr, sizeof(ccm_date.datestring));
        ccm_date.valid = TRUE;

        // call ntp set time handler (TNP/cnu specific; stub for legacy)
        // do not call the handler if 2xx is from StdBy CCM
        if (ccb->index != REG_BACKUP_CCB) {
            SipNtpUpdateClockFromCCM();
        }
    }

    contact = sippmh_get_cached_header_val(response, CONTACT);
    if (contact != NULL) {
        /* There is a contact header pass it */
        contact_info = sippmh_parse_contact(contact);
    }

	/* Update the sip interface protocol version */
	update_sis_protocol_version(response);

    /*update the CME for the version negotiation feature */
    update_cme_sis_version(response);

    if (ccb->dn_line == PRIMARY_LINE && ccsip_check_ccm_restarted(contact_info)) {
        /* CCM has restarted detected */
        CCSIP_DEBUG_MESSAGE(DEB_F_PREFIX"Detected server has restarted via line %d\n",
                            DEB_F_PREFIX_ARGS(SIP_CCM_RESTART, fname), ccb->dn_line);
        sip_regmgr_ccm_restarted(ccb);
    }

    sip_stop_ack_timer(ccb);
    clean_method_request_trx(ccb, sipMethodRegister, TRUE);

    if (ccb->send_reason_header) {
        ccb->send_reason_header = FALSE;
        sipUnregisterReason[0] = '\0';
    }

    /*
     * Get expiration time. The expriation time are from contact info. or
     * from the expire header.
     */
    expires = sippmh_get_header_val(response, SIP_HEADER_EXPIRES, NULL);
    if (ccsip_get_exp_time_2xx(ccb, contact_info, expires, &exp_time)
            == FALSE) {
        /*
         * The expire time received in 200OK is smaller than the minimum
         * allowed.  The phone will be unregistered and a message will be
         * displayed under the Status Messages.  The phone will retry to
         * register after the configured expire-timer interval.
         */
        config_get_value(CFGID_TIMER_REGISTER_EXPIRES, &exp_time,
                         sizeof(uint32_t));
        ccb->reg.tmr_expire = exp_time;
        ccb->reg.act_time = (int) time(NULL);
        sip_reg_sm_change_state(ccb, SIP_REG_STATE_UNREGISTERING);

        if (ccsip_register_send_msg(SIP_REG_CANCEL, ccb->index)
            != SIP_REG_OK) {
            ccsip_register_cleanup(ccb, FALSE);
        }
        (void) sip_platform_register_expires_timer_start(ccb->reg.tmr_expire * 1000,
                                                         ccb->index);
        if (contact_info != NULL) {
            sippmh_free_contact(contact_info);
        }
        free_sip_message(response);
        log_clear(LOG_REG_EXPIRE);
        log_msg(LOG_REG_EXPIRE);
        return;
    }


           ccb->reg.registered = 1;
           sip_reg_sm_change_state(ccb, SIP_REG_STATE_REGISTERED);
           regall_fail_attempt = FALSE;
           
           if (ccb->index != REG_BACKUP_CCB) { 
           	   registration_reject = FALSE;                             
               ui_set_sip_registration_state(ccb->dn_line, TRUE);
               CCSIP_DEBUG_REG_STATE(DEB_F_PREFIX"Setting Reg state to TRUE for line=%d\n",
                              DEB_F_PREFIX_ARGS(SIP_REG_STATE, fname), ccb->dn_line);
              /* Get supported options from the supported header from the response */
               sip_get_supported_options_2xx(ccb, response);
           } else {
               log_clear(LOG_REG_BACKUP);
           }


    if (contact_info != NULL) {
        sippmh_free_contact(contact_info);
    }

    ccb->reg.tmr_expire = exp_time;
  
    ccb->reg.act_time = (int) time(NULL);
    CCSIP_DEBUG_REG_STATE(DEB_L_C_F_PREFIX"Starting expires timer (%d "
                          "sec)\n", DEB_L_C_F_PREFIX_ARGS(SIP_TIMER, ccb->index, ccb->dn_line, fname),
                          ccb->reg.tmr_expire);
    (void) sip_platform_register_expires_timer_start(ccb->reg.tmr_expire * 1000,
                                                     ccb->index);


    if (ccb->cc_type == CC_CCM) {
        /*
         * regmgr - A 200 OK will reach here only if the state is
         * REGISTERING, the unreg_200ok eve is handled
         * separately.
         */
        ti_config_table_t *ccm_table_ptr;

        ccm_table_ptr = (ti_config_table_t *) ccb->cc_cfg_table_entry;

        if ((ccm_table_ptr == CCM_Active_Standby_Table.active_ccm_entry) &&
                start_standby_monitor) {
            ccsipCCB_t     *standby_ccb;

            start_standby_monitor = FALSE;
            sip_platform_set_ccm_status();

            /*
             * Kickoff monitoring the standby
             */

            standby_ccb = sip_sm_get_ccb_by_index(REG_BACKUP_CCB);
            if (CCM_Active_Standby_Table.standby_ccm_entry) {
                config_get_value(CFGID_TIMER_REGISTER_EXPIRES,
                                 &exp_time, sizeof(uint32_t));
                standby_ccb->reg.tmr_expire = exp_time;
                standby_ccb->reg.act_time = (int) time(NULL);
                sip_reg_sm_change_state(standby_ccb,
                                        SIP_REG_STATE_UNREGISTERING);
                (void) ccsip_register_send_msg(SIP_REG_CANCEL, standby_ccb->index);
                (void) sip_platform_register_expires_timer_start(standby_ccb->reg.tmr_expire * 1000,
                                                                 standby_ccb->index);
            }
        } else if (ccm_table_ptr ==
                   CCM_Active_Standby_Table.standby_ccm_entry) {
            /*
             * REGMGR - Update stats Got a 200OK for a expires 0
             * Keepalive msg to standby, update stats ?
             */
        }
    }


    config_get_value(CFGID_NAT_ENABLE, &nat_enable, sizeof(nat_enable));
    config_get_value(CFGID_NAT_RECEIVED_PROCESSING, &nat_rx_proc_enable,
                     sizeof(nat_rx_proc_enable));
    if (nat_enable == 1 && nat_rx_proc_enable == 1) {
        pViaHeaderStr = sippmh_get_cached_header_val(response, VIA);
        if (pViaHeaderStr != NULL) {
            via = sippmh_parse_via(pViaHeaderStr);
            if (via != NULL) {
                if (via->recd_host != NULL) {
                    cpr_ip_addr_t received_ip;

                    memset(&received_ip, 0, sizeof(cpr_ip_addr_t));
                    dns_err_code = dnsGetHostByName(via->recd_host,
                                                       &received_ip, 100, 1);
                    if (dns_err_code == 0) {
                        util_ntohl(&received_ip, &received_ip);
                    } else {
                        sip_config_get_nat_ipaddr(&received_ip);
                    }

                    if (util_compare_ip(&received_ip, &(ccb->src_addr)) == FALSE) {
                        /*
                         * The address the proxy/registration server received
                         * is not the same as the one that we think we have
                         * so we'll update our address and re-register with
                         * the proxy/registration server
                         */

                        ccb->reg.rereg_pending = 1;

                        // Make sure our running config has the correct
                        // IP address that we got from the proxy server
                        sip_config_set_nat_ipaddr(&received_ip);

                        if (ccsip_register_send_msg(SIP_REG_CANCEL, ccb->index)
                            != SIP_REG_OK) {
                            ccsip_register_cleanup(ccb, FALSE);
                        }

                        // Update the tel ccb that this reg line belongs to
                        ccb = sip_sm_get_ccb_by_index((line_t)(ccb->index - REG_CCB_START + 1));
                        if (ccb) {
                            ccb->src_addr = received_ip;
                        }
                    }
                }

                // Cleanup memory
                sippmh_free_via(via);
            }
        }
    }

    free_sip_message(response);   
}

void
ccsip_handle_ev_3xx (ccsipCCB_t *ccb, sipSMEvent_t *event)
{
    static const char fname[] = "ccsip_handle_ev_3xx";
    sipMessage_t   *response = NULL;
    int             status_code = 0;
    char            status[LOG_MAX_LEN];
    char            user[MAX_LINE_NAME_SIZE];
    sipUrl_t       *url;
    const char     *contact = NULL;

    response = event->u.pSipMessage;
    clean_method_request_trx(ccb, sipMethodRegister, TRUE);

    if (sipGetResponseCode(response, &status_code) < 0) {
        CCSIP_DEBUG_ERROR(get_debug_string(DEBUG_REG_SIP_RESP_CODE),
                          ccb->index, ccb->dn_line, fname);
        free_sip_message(response);
        return;
    }

    switch (status_code) {
    case SIP_RED_MULT_CHOICES /* 300 */:
    case SIP_RED_MOVED_PERM   /* 301 */:
    case SIP_RED_MOVED_TEMP   /* 302 */:
    case SIP_RED_USE_PROXY    /* 305 */:
        CCSIP_DEBUG_STATE(get_debug_string(DEBUG_FUNCTION_ENTRY2),
                          ccb->index, ccb->dn_line, fname,
                          sip_util_reg_state2string((sipRegSMStateType_t)ccb->state),
                          "SIP", status_code);

        sip_sm_200and300_update(ccb, response, status_code);

        /*
         * Record the Contact header.  For 3xx messages we are recording
         * the Contact header here, not in the sipSPICheckResponse() header,
         * so that the ACK response to 3xx is sent to the original INVITE
         * destination (or to the Record-Route specified destination), rather
         * than to the new 302 Contact
         */
        contact = sippmh_get_cached_header_val(response, CONTACT);
        if (contact) {
            if (ccb->contact_info) {
                sippmh_free_contact(ccb->contact_info);
            }
            ccb->contact_info = sippmh_parse_contact(contact);
        }


        /*
         * Check the validity of Contact header
         */
        if (!ccb->contact_info) {
            CCSIP_DEBUG_ERROR("%s: Error: Invalid Contact field.  Aborting "
                              "REGISTER.\n", fname);
            free_sip_message(response);
            ccsip_register_cleanup(ccb, TRUE);
            return;
        }

        if (ccb->contact_info->locations[0]->genUrl->schema == URL_TYPE_SIP) {
            url = ccb->contact_info->locations[0]->genUrl->u.sipUrl;
        } else {
            CCSIP_DEBUG_ERROR("%s: Error: URL is not Sip.  Aborting "
                              "REGISTER.\n", fname);
            free_sip_message(response);
            ccsip_register_cleanup(ccb, TRUE);
            return;
        }

        sstrncpy(ccb->reg.proxy, url->host, MAX_IPADDR_STR_LEN);
        ccb->reg.port = url->port;

        if (ccb->contact_info) {
            sippmh_free_contact(ccb->contact_info);
            ccb->contact_info = NULL;
        }

        sip_util_get_new_call_id(ccb);

        /*
         * Send out a new REGISTER
         */
        ccb->authen.cred_type = 0;
        config_get_line_string(CFGID_LINE_NAME, user, ccb->dn_line,
                               sizeof(user));

        if (sipSPISendRegister(ccb, 0, user, ccb->reg.tmr_expire) != TRUE) {
            ccsip_register_cleanup(ccb, TRUE);
            free_sip_message(response);

            log_clear(LOG_REG_RED_MSG);
            log_msg(LOG_REG_RED_MSG);
        }
        return;

    default:
        free_sip_message(event->u.pSipMessage);

        snprintf(status, sizeof(status), "in %d, redirection", status_code);
        log_clear(LOG_REG_UNSUPPORTED);
        log_msg(LOG_REG_UNSUPPORTED, status);

        ccsip_register_cleanup(ccb, TRUE);
    }
}


void
ccsip_handle_ev_4xx (ccsipCCB_t *ccb, sipSMEvent_t *event)
{
    static const char fname[] = "ccsip_handle_ev_4xx";
    sipMessage_t   *response = NULL;
    int             status_code = 0;
    const char     *authenticate = NULL;
    char            user[MAX_LINE_NAME_SIZE];
    credentials_t   credentials;
    char            status[LOG_MAX_LEN];
    sip_authen_t   *sip_authen = NULL;
    char           *author_str = NULL;
    char            uri[MAX_IPADDR_STR_LEN + 10]; /* Proxy IP address string */
    char            tmp_str[STATUS_LINE_MAX_LEN];
    ti_config_table_t *ccm_table_ptr = NULL;
    
    ccm_table_ptr = (ti_config_table_t *) ccb->cc_cfg_table_entry;
    

    response = event->u.pSipMessage;
    clean_method_request_trx(ccb, sipMethodRegister, TRUE);

    if (sipGetResponseCode(response, &status_code) < 0) {
        CCSIP_DEBUG_ERROR(get_debug_string(DEBUG_REG_SIP_RESP_CODE),
                          ccb->index, ccb->dn_line, fname);
        free_sip_message(response);
        return;
    }
    
    /*
     * Set reject flag if we get 4xx from publisher cucm.
     * Note: if non-prime dn is congfigured with #, such as 1234##,
     * then phone will get 404 for that DN, even though the prime DN is
     * registered.
     */ 
    if (ccm_table_ptr &&
        ccb->index != REG_BACKUP_CCB) {
    	DEF_DEBUG(DEB_F_PREFIX"Receive 4xx in ccm mode. set registration_reject to TRUE.\n", 
            DEB_F_PREFIX_ARGS(SIP_REG, fname));	
    	registration_reject = TRUE;
    }

    switch (status_code) {
    case SIP_CLI_ERR_UNAUTH:
    case SIP_CLI_ERR_PROXY_REQD:
        CCSIP_DEBUG_STATE(get_debug_string(DEBUG_FUNCTION_ENTRY2),
                          ccb->index, ccb->dn_line, fname,
                          sip_util_reg_state2string((sipRegSMStateType_t)ccb->state),
                          "SIP", status_code);

        if (cred_get_credentials_r(ccb, &credentials) == FALSE) {
            CCSIP_DEBUG_REG_STATE(DEB_L_C_F_PREFIX"no more credentials, retry %d, %d\n",
                                  DEB_L_C_F_PREFIX_ARGS(SIP_CRED, ccb->index, ccb->dn_line, fname),
                                  ccb->authen.retries_401_407,
                                  MAX_RETRIES_401);


            if (ccb->cc_type == CC_CCM) {
                sip_regmgr_ev_failure_response(ccb, event);
                free_sip_message(response);
                return;
            }
            ccsip_register_cleanup(ccb, TRUE);
            free_sip_message(response);

            log_clear(LOG_REG_AUTH_NO_CRED);
            log_msg(LOG_REG_AUTH_NO_CRED);

            break;
        }

        authenticate = sippmh_get_header_val(response, AUTH_HDR(status_code),
                                             NULL);
        if (authenticate) {
            sip_authen = sippmh_parse_authenticate(authenticate);
        }

        if (sip_authen) {
            if (sip_authen->scheme == SIP_DIGEST) {
                if (authenticate != NULL) {
                    ccb->retx_counter = 0;
                    config_get_line_string(CFGID_LINE_NAME, user, ccb->dn_line,
                                           sizeof(user));
                    ccb->authen.cnonce[0] = '\0';
                    snprintf(uri, sizeof(uri), "sip:%s", ccb->reg.proxy); /* proxy */
                    if (sipSPIGenerateAuthorizationResponse(sip_authen, uri,
                                                            SIP_METHOD_REGISTER,
                                                            credentials.id,
                                                            credentials.pw,
                                                            &author_str,
                                                            &(ccb->authen.nc_count),
                                                            ccb)) {

                        if (author_str) {
                            if (ccb->authen.authorization != NULL) {
                                cpr_free(ccb->authen.authorization);
                                ccb->authen.authorization = NULL;
                            }
    
                            ccb->authen.authorization =
                                (char *) cpr_malloc(strlen(author_str) *
                                                    sizeof(char) + 1);
    
                            if (ccb->authen.authorization != NULL) {
                                sstrncpy(ccb->authen.authorization, author_str,
                                         strlen(author_str) * sizeof(char) + 1);
                                ccb->authen.status_code = status_code;
                            }
                            cpr_free(author_str);
                        }
                    } else {
                        CCSIP_DEBUG_ERROR(get_debug_string(DEBUG_SIP_FUNCTIONCALL_FAILED),
                                          ccb->index, ccb->dn_line, fname,
                                          "sipSPIGenerateAuthorizationResponse");
                    }

                    if (!sipSPISendRegister(ccb, TRUE, user,
                                            ccb->reg.tmr_expire)){
                          ccsip_register_cleanup(ccb, TRUE);

                          log_clear(LOG_REG_AUTH_MSG);
                          log_msg(LOG_REG_AUTH_MSG);
                    }

                } else {
                    CCSIP_DEBUG_ERROR(get_debug_string(DEBUG_SIP_FUNCTIONCALL_FAILED),
                                      ccb->index, ccb->dn_line, fname,
                                      "sippmh_parse_authenticate");

                    ccsip_register_cleanup(ccb, TRUE);
                    log_clear(LOG_REG_AUTH_HDR_MSG);
                    log_msg(LOG_REG_AUTH_HDR_MSG);
                }
            } else {
                CCSIP_DEBUG_REG_STATE(DEB_F_PREFIX"auth scheme unsupported= %d\n",
                                      DEB_F_PREFIX_ARGS(SIP_AUTH, fname), sip_authen->scheme);

                ccsip_register_cleanup(ccb, TRUE);
                log_clear(LOG_REG_AUTH_SCH_MSG);
                log_msg(LOG_REG_AUTH_SCH_MSG);
            }
            sippmh_free_authen(sip_authen);
        }

        free_sip_message(response);
        return;

    case SIP_CLI_ERR_INTERVAL_TOO_SMALL:
        {
            int         new_expires = MAX_REG_EXPIRES;
            const char *msg_ptr = NULL;
            char        line_name[MAX_LINE_NAME_SIZE];

            msg_ptr = sippmh_get_header_val(response,
                                            (const char *)SIP_HEADER_MIN_EXPIRES,
                                            NULL);
            if (msg_ptr) {
                new_expires = strtoul(msg_ptr, NULL, 10);
            }

            //ensure new Min-Expires is > what we set before in Expires
            if (new_expires > ccb->reg.tmr_expire) {
                ccb->reg.tmr_expire = new_expires;
            } else {
                ccb->reg.tmr_expire = MAX_REG_EXPIRES;
            }

            //We have to make a fresh request
            sip_stop_ack_timer(ccb);
            sip_start_ack_timer(ccb);

            (void) sip_platform_register_expires_timer_stop(ccb->index);

            sip_util_get_new_call_id(ccb);

            ccb->authen.cred_type = 0;
            ccb->retx_counter = 0;
            config_get_line_string(CFGID_LINE_NAME, line_name, ccb->dn_line,
                                   sizeof(line_name));

            ccb->reg.act_time = (int) time(NULL);

                 if (sipSPISendRegister(ccb, 0, line_name, ccb->reg.tmr_expire) != TRUE) {
                     // set expire timer and cleanup ccb in non-ccm mode.
                     // do not change the expire timer value in ccm mode

                     if (ccb->cc_type != CC_CCM) {
                         ccsip_register_cleanup(ccb, TRUE);
                     }

                     log_clear(LOG_REG_MSG);
                     log_msg(LOG_REG_MSG);
                 }

            //no change in state
        }
        free_sip_message(response);
        return;


        /*
         * 404 (Not Found)
         * 413 (Request Entity Too Large)
         * 480 (Temporarily Unavailable)
         * 486 (Busy here)
         */
    case SIP_CLI_ERR_NOT_FOUND:
    case SIP_CLI_ERR_LARGE_MSG:
    case SIP_CLI_ERR_NOT_AVAIL:
    case SIP_CLI_ERR_BUSY_HERE:
        if (ccb->cc_type == CC_CCM) {
            if (ccb->state == (int) SIP_REG_STATE_TOKEN_WAIT) {
                clean_method_request_trx(ccb, sipMethodRefer, TRUE);
                sip_regmgr_ev_token_wait_4xx_n_5xx(ccb, event);
            } else {
                /*
                 * Phone sends a REGISTER message with expires=0 as a
                 * Keepalive msg. Proxy and CCM will respond with a
                 * 200 OK.
                 */
                if (ccb->index != REG_BACKUP_CCB) {
                    if (ccsip_register_get_register_state() != SIP_REG_UNREGISTERING) {
                        if (((ti_config_table_t *)(ccb->cc_cfg_table_entry)) ||
                            ccb->dn_line == PRIMARY_LINE) {
                            if (platGetPhraseText(STR_INDEX_REGISTRATION_REJECTED,
                                 (char *)tmp_str, STATUS_LINE_MAX_LEN - 1)== CPR_SUCCESS) {
                                ui_set_notification(CC_NO_LINE, CC_NO_CALL_ID, tmp_str, 15, FALSE,
                                                DEF_NOTIFY_PRI);
                            }
                        }
                        log_clear(STR_INDEX_REGISTRATION_REJECTED);
                        log_msg(STR_INDEX_REGISTRATION_REJECTED);
                    }
                }
                if (process_retry_after(ccb, response) == FALSE) {
                    sip_regmgr_ev_failure_response(ccb, event);
                }
            }
        } else {
            if (process_retry_after(ccb, response) == FALSE) {
                ccsip_register_cleanup(ccb, TRUE);
            }
        }
        free_sip_message(response);
        return;

    default:
        if (ccb->cc_type == CC_CCM) {
            if (ccb->state == (int) SIP_REG_STATE_TOKEN_WAIT) {
                clean_method_request_trx(ccb, sipMethodRefer, TRUE);
                sip_regmgr_ev_token_wait_4xx_n_5xx(ccb, event);
            } else {
                sip_regmgr_ev_failure_response(ccb, event);
            }
            free_sip_message(response);
            return;
        }
        CCSIP_DEBUG_STATE(get_debug_string(DEBUG_FUNCTION_ENTRY2),
                          ccb->index, ccb->dn_line, fname,
                          sip_util_reg_state2string((sipRegSMStateType_t)ccb->state),
                          "SIP BAD", status_code);

        snprintf(status, sizeof(status), "in %d, client error", status_code);
        ccsip_register_cleanup(ccb, TRUE);

        snprintf(status, sizeof(status), "in %d, request failure", status_code);
        log_clear(LOG_REG_UNSUPPORTED);
        log_msg(LOG_REG_UNSUPPORTED, status);
        free_sip_message(response);
        break;
    }

    if (ccb->reg.rereg_pending != 0) {
        ccb->reg.rereg_pending = 0;
        if (ccsip_register_send_msg(SIP_REG_REQ, ccb->index) != SIP_REG_OK) { 
            ccsip_register_cleanup(ccb, TRUE);
        }
    }
}


void
ccsip_handle_ev_failure_response (ccsipCCB_t *ccb, sipSMEvent_t *event)
{
    static const char fname[] = "ccsip_handle_ev_failure_response";
    sipMessage_t   *response = NULL;
    int             status_code = 0;
    char            status[LOG_MAX_LEN];
    sipStatusCodeClass_t code_class = codeClassInvalid;
    ti_config_table_t *ccm_table_ptr = NULL;

    /*
     * Set reject flag if we get 5xx/6xx from publisher cucm.
     */ 
    ccm_table_ptr = (ti_config_table_t *) ccb->cc_cfg_table_entry;
    if (ccm_table_ptr && 
        ccb->index != REG_BACKUP_CCB) {
        registration_reject = TRUE;
        DEF_DEBUG(DEB_F_PREFIX"registration has been rejected. Set registration_reject to TRUE.\n", 
            DEB_F_PREFIX_ARGS(SIP_REG, fname));	
    }

    response = event->u.pSipMessage;
    clean_method_request_trx(ccb, sipMethodRegister, TRUE);

    if (sipGetResponseCode(response, &status_code) < 0) {
        CCSIP_DEBUG_ERROR(get_debug_string(DEBUG_REG_SIP_RESP_CODE),
                          ccb->index, ccb->dn_line, fname);
        free_sip_message(response);
        ccsip_register_cleanup(ccb, TRUE);
        return;
    }

    code_class = sippmh_get_code_class((uint16_t)status_code);

    log_clear(LOG_REG_AUTH);

    switch (code_class) {
    case codeClass5xx:
        CCSIP_DEBUG_ERROR(get_debug_string(DEBUG_REG_SIP_RESP_FAILURE),
                          ccb->index, ccb->dn_line, fname, status_code);

        log_msg(LOG_REG_AUTH_SERVER_ERR, status_code);

        break;

    case codeClass6xx:
        CCSIP_DEBUG_ERROR(get_debug_string(DEBUG_REG_SIP_RESP_FAILURE),
                          ccb->index, ccb->dn_line, fname, status_code);

        log_msg(LOG_REG_AUTH_GLOBAL_ERR, status_code);

        break;

    default:
        CCSIP_DEBUG_ERROR(get_debug_string(DEBUG_REG_SIP_RESP_FAILURE),
                          ccb->index, ccb->dn_line, fname, status_code);

        snprintf(status, sizeof(status), "in %d, ??? error", status_code);
        log_msg(LOG_REG_AUTH_UNKN_ERR, status_code);

        break;
    }

    if (ccb->cc_type == CC_CCM) {
        if (ccb->state == (int) SIP_REG_STATE_TOKEN_WAIT) {

            /* Handle only 503 error condition for other errors
             * use default state 
             */
            if (status_code == SIP_SERV_ERR_UNAVAIL) {
                clean_method_request_trx(ccb, sipMethodRefer, TRUE);
                sip_regmgr_ev_token_wait_4xx_n_5xx(ccb, event);
                free_sip_message(response);
            } else {
                sip_regmgr_ev_default(ccb, event);
            }
            return;
        }

        /* retry registration using value in the Retry-After header
         * if response code is 503 with Retry-After
         */
        if ((status_code == SIP_SERV_ERR_UNAVAIL) &&
            (process_retry_after(ccb, response) == TRUE)) {            
            free_sip_message(response);
            return;
        }
        /*
         * regmgr - Pass the event to the regmgr
         */
        sip_regmgr_ev_failure_response(ccb, event);
        free_sip_message(response);
    } else {
        /*
         * Assume CSPS for now
         */

        switch (status_code) {

            /*
             * 500 (Server Internal Error)
             * 503 (Service Unavailable)
             * 600 (Busy),
             * 603 (Declined
             */
        case SIP_SERV_ERR_INTERNAL:
        case SIP_SERV_ERR_UNAVAIL:

        case SIP_FAIL_BUSY:
        case SIP_FAIL_DECLINE:
            if (process_retry_after(ccb, response) == FALSE) {
                ccsip_register_cleanup(ccb, TRUE);
            }
            free_sip_message(response);
            return;

        default:
            break;
        }
        ccsip_register_cleanup(ccb, TRUE);
        free_sip_message(response);

        if (ccb->reg.rereg_pending != 0) {
            ccb->reg.rereg_pending = 0;
            if (ccsip_register_send_msg(SIP_REG_REQ, ccb->index)
                    != SIP_REG_OK) {
                ccsip_register_cleanup(ccb, TRUE);
            }
        }
    }
}

/*
 * ccsip_handle_ev_tmr_retry()
 *
 * This function is called when registration re-try timer
 * pops or an ICMP unreachable is received after sending
 * out the registration. The ICMP unreachable code sets the
 * retx_counter to MAX to force the code to try the next
 * proxy in the list. Event.u.usrInfo is set to the IP
 * address that bounced or -1 if is a timeout.
 */
void
ccsip_handle_ev_tmr_retry (ccsipCCB_t *ccb, sipSMEvent_t *event)
{
    static const char fname[] = "ccsip_handle_ev_tmr_retry";
    boolean         start_timer;
    uint32_t        value;
    int             dns_err_code = DNS_ERR_HOST_UNAVAIL;
    cpr_ip_addr_t   ip_addr;

    CPR_IP_ADDR_INIT(ip_addr);

    start_timer = (ccsip_register_get_register_state() == SIP_REG_UNREGISTERING) ?
        (FALSE) : (TRUE);

    config_get_value(CFGID_SIP_RETX, &value, sizeof(value));
    if (value > MAX_NON_INVITE_RETRY_ATTEMPTS) {
        value = MAX_NON_INVITE_RETRY_ATTEMPTS;
    }
    if (ccb->retx_counter >= value) {
        if ((ccb->cc_type == CC_CCM) /* RAMC Some other state */) {
            /*
             * regmgr - Send event to the regmgr
             */
            CCSIP_DEBUG_REG_STATE(DEB_F_PREFIX"Reached here for ccb->index=%d \n", 
                    DEB_F_PREFIX_ARGS(SIP_MSG_SEND, fname), ccb->index);
            sip_regmgr_ev_tmr_ack_retry(ccb, event);
            return;
        } else {
            /*
             * Assume CSPS for now
             */
            ccb->retx_counter = 0;

            /*
             * Did the registration msg bounce trying to reach the outbound
             * proxy (1st check) or did it timeout trying to reach the
             * outbound proxy (2nd check)?
             */
            util_ntohl(&ip_addr, &(event->u.UsrInfo));
            if (util_compare_ip(&ip_addr, &(ccb->outBoundProxyAddr)) ||
                ((event->u.UsrInfo.type == CPR_IP_ADDR_INVALID) && 
                util_check_if_ip_valid(&(ccb->outBoundProxyAddr)))) {
                /*
                 * If there are more records, reset address to force code
                 * to use next entry. If not then bail.
                 */
                ccb->outBoundProxyPort = 0;
                dns_err_code = DNS_OK;

                /*
                 * Trouble reaching the normal proxy. Get another proxy
                 * if records are available
                 */
            } else if (str2ip(ccb->reg.proxy, &ccb->reg.addr) != 0) {
                dns_err_code = sipTransportGetServerAddrPort(ccb->reg.proxy,
                                                             &ccb->reg.addr,
                                                             (uint16_t *) &ccb->reg.port,
                                                             &ccb->SRVhandle,
                                                             TRUE);
                if (dns_err_code == 0) {
                    util_ntohl(&(ccb->reg.addr), &(ccb->reg.addr));
                } else {
                    ccb->reg.addr = ip_addr_invalid;
                }

                /*
                 * Modify destination fields in call back timer struct
                 */
                (void) sip_platform_msg_timer_update_destination(ccb->index,
                                                                 &(ccb->reg.addr),
                                                                 ccb->reg.port);
            } else {
                /* No other proxy to try */
                dns_err_code = DNS_ERR_HOST_UNAVAIL;
            }

            /*
             * Cleanup if unable to find another proxy
             */
            if (dns_err_code != DNS_OK) {
                /*
                 * All retransmit attempts have been exhausted.
                 * Cleanup the call and move to the IDLE state
                 */
                ccsip_register_cleanup(ccb, start_timer);

                log_clear(LOG_REG_RETRY);
                log_msg(LOG_REG_RETRY);
                if ((ccb->index == REG_BACKUP_CCB) &&
                    (ccb->cc_type != CC_CCM)) {
                    log_clear(LOG_REG_BACKUP);
                    log_msg(LOG_REG_BACKUP);
                }
                if (ccb->reg.rereg_pending != 0) {
                    ccb->reg.rereg_pending = 0;
                    if (ccsip_register_send_msg(SIP_REG_REQ, ccb->index)
                        != SIP_REG_OK) {
                        ccsip_register_cleanup(ccb, TRUE);
                    }
                }

                return;
            }
        }
    }

    if (ccb->state != (int) SIP_REG_STATE_REGISTERED) {
        /*
         * Handling Retry timer in REGISTERED state is not needed
         * the event has come due to race condition
         */
        ccb->retx_counter++;
        CCSIP_DEBUG_REG_STATE(DEB_F_PREFIX"Resending message: #%d\n", 
                              DEB_F_PREFIX_ARGS(SIP_MSG_SEND, fname), ccb->retx_counter);

        if (sipSPISendLastMessage(ccb) != TRUE) {
            if (ccb->cc_type == CC_CCM) {
                sip_regmgr_ev_tmr_ack_retry(ccb, event);
            } else {
                ccsip_register_cleanup(ccb, start_timer);
            }
            return;
        }

        ccsip_register_retry_timer_start(ccb);
    }

}


void
ccsip_handle_ev_reg_cancel (ccsipCCB_t *ccb, sipSMEvent_t *event)
{
    char user[MAX_LINE_NAME_SIZE];
    int  exp_time = 0;

    ccsip_register_clear_all_logs();

    sip_stop_ack_timer(ccb);
    sip_start_ack_timer(ccb);

    (void) sip_platform_register_expires_timer_stop(ccb->index);

    sip_util_get_new_call_id(ccb);

    ccb->authen.cred_type = 0;
    ccb->retx_counter     = 0;
    ccb->reg.tmr_expire   = 0;
    ccb->reg.act_time     = 0;
    config_get_line_string(CFGID_LINE_NAME, user, ccb->dn_line, sizeof(user));

    if (sipSPISendRegister(ccb, 0, user, exp_time) != TRUE) {
        log_clear(LOG_REG_CANCEL_MSG);
        log_msg(LOG_REG_CANCEL_MSG);
    } else if ((ccb->index == REG_BACKUP_CCB) && (ccb->cc_type != CC_CCM)) {
        log_clear(LOG_REG_BACKUP);
        log_msg(LOG_REG_BACKUP);
    }

    sip_reg_sm_change_state(ccb, SIP_REG_STATE_UNREGISTERING);
}


void
ccsip_handle_ev_unreg_2xx (ccsipCCB_t *ccb, sipSMEvent_t *event)
{
    static const char fname[] = "ccsip_handle_ev_unreg_2xx";
    sipMessage_t *response = event->u.pSipMessage;
    int           timeout;

    free_sip_message(response);

    ccb->reg.registered = 0;
    ccb->reg.tmr_expire = 0;
    ccb->reg.act_time = 0;
    clean_method_request_trx(ccb, sipMethodRegister, TRUE);

    if (ccb->cc_type == CC_CCM) {
        if (ccb->index == REG_BACKUP_CCB) {
            /*
             * Stay in unregistering state and wait for the
             * expires timer to pop to send the next keepalive.
             */
            sip_stop_ack_timer(ccb);
            if (new_standby_available) {
                sip_regmgr_replace_standby(ccb);
            }

            timeout = sip_config_get_keepalive_expires();

            CCSIP_DEBUG_REG_STATE(DEB_L_C_F_PREFIX"Keep alive timer (%d sec)\n",
                    DEB_L_C_F_PREFIX_ARGS(SIP_TIMER, ccb->index, ccb->dn_line, fname), timeout);
            (void) sip_platform_standby_keepalive_timer_start(timeout * 1000);
        } else {
            sip_reg_sm_change_state(ccb, SIP_REG_STATE_IDLE);

            sip_stop_ack_timer(ccb);

            if (ccsip_register_all_unregistered() == TRUE) {
                ccsip_register_set_register_state(SIP_REG_IDLE);
                /*
                 * regmgr - FALLBACK: Pick the ccb of the ccm that has
                 * come up (falling back) and post the fallback event,
                 * now that the unregistering is done.
                 * Will commit change after integration with ccm.
                 */
            }
            platSetSISProtocolVer(1,0,0,NULL);
            CCSIP_DEBUG_REG_STATE(DEB_F_PREFIX"set[1] the SIS protocol ver to 1.0.0\n",
                                  DEB_F_PREFIX_ARGS(SIP_REG, fname));
        }
    } else {
        sip_reg_sm_change_state(ccb, SIP_REG_STATE_IDLE);

        sip_stop_ack_timer(ccb);

        if (ccsip_register_all_unregistered() == TRUE) {
            ccsip_register_set_register_state(SIP_REG_IDLE);
        }

        /*
         * Our unregistration from the proxy has succeeded now we
         * can reregister again
         */
        if (ccb->reg.rereg_pending != 0) {
            ccb->reg.rereg_pending = 0;
            if (ccsip_register_send_msg(SIP_REG_REQ, ccb->index)
                    != SIP_REG_OK) {
                ccsip_register_cleanup(ccb, TRUE);
            }
        }
        platSetSISProtocolVer(1,0,0,NULL);
        CCSIP_DEBUG_REG_STATE(DEB_F_PREFIX"set[2] the SIS protocol ver to 1.0.0\n",
                              DEB_F_PREFIX_ARGS(SIP_REG, fname));
    }
}


void
ccsip_handle_ev_unreg_tmr_ack (ccsipCCB_t *ccb, sipSMEvent_t *event)
{
    log_clear(LOG_REG_AUTH_UNREG_TMR);
    log_msg(LOG_REG_AUTH_UNREG_TMR);

    if (ccb->cc_type == CC_CCM) {
        /*
         * regmgr - Send tmr ack event to the regmgr
         */
        sip_regmgr_ev_tmr_ack_retry(ccb, event);
        return;
    }
    ccsip_register_cleanup(ccb, FALSE);
    /*
     * We are unregistering ourselves from the proxy when our own registration
     * timer has expired.  This means that we do not need to complete our
     * unregistration from the proxy since it is being done automatically.
     * We can just send our new registration now
     */
    if (ccb->reg.rereg_pending != 0) {
        ccb->reg.rereg_pending = 0;
        if (ccsip_register_send_msg(SIP_REG_REQ, ccb->index) != SIP_REG_OK) {
            ccsip_register_cleanup(ccb, TRUE);
        }
    }
}

void
ccsip_handle_ev_standby_keepalive_tmr_expire (ccsipCCB_t *ccb,
                                              sipSMEvent_t *event)
{
    /*
     * regmgr - If there is a fallback ccb in TOKEN_WAIT
     * state waiting to fallback notify it so it can
     * continue with the fallback procedure.
     */
    if (ccb->cc_type == CC_CCM) {
        /*
         * REGMGR - CHECK if this event gets called.
         * Will get in here only for REG_BACKUP_CCB in
         * unregistering state i.e, the standby cc which
         * is periodically monitored.
         */
        /*
         * Stay in unregistering state and post a message
         * to send another keep-alive message.
         */
        (void) sip_platform_standby_keepalive_timer_stop();
        (void) ccsip_register_send_msg(SIP_REG_CANCEL, ccb->index);
    } else {
        ccsip_handle_ev_default(ccb, event);
    }
}

int
sip_reg_sm_process_event (sipSMEvent_t *pEvent)
{
    static const char fname[] = "sip_reg_sm_process_event";
    ccsipCCB_t *ccb = NULL;

    ccb = pEvent->ccb;
    if (!ccb) {
        CCSIP_DEBUG_ERROR("%s: Error: ccb is null. Unable to process event "
                          "<%d>\n", fname, pEvent->type);
        return (-1);
    }

    /* Unbind UDP ICMP response handler */
    if ((REG_CHECK_EVENT_SANITY((int) ccb->state, (int) pEvent->type)) &&
        (REG_EVENT_ACTION(ccb->state, (int) pEvent->type))) {
        if (dump_reg_msg == TRUE) {
            DEF_DEBUG(DEB_L_C_F_PREFIX"%s <- %s\n",
                    DEB_L_C_F_PREFIX_ARGS(SIP_REG_STATE, ccb->dn_line, ccb->index, fname),
                    sip_util_reg_state2string((sipRegSMStateType_t)ccb->state),
                    sip_util_reg_event2string((sipRegSMEventType_t)pEvent->type));
        }
        REG_EVENT_ACTION(ccb->state, pEvent->type) (ccb, pEvent);
    } else {
        /* Invalid State/Event pair */
        CCSIP_DEBUG_ERROR("%s: Error: illegal state/event pair: (%d <-- %d)\n",
                          fname, ccb->state, pEvent->type);
        return (-1);
    }

    return (0);
}


const char *
sip_util_reg_event2string (sipRegSMEventType_t event)
{
    switch (event) {
    case E_SIP_REG_REG_REQ:
        return ("E_SIP_REG_REG_REQ");

    case E_SIP_REG_CANCEL:
        return ("E_SIP_REG_CANCEL");

    case E_SIP_REG_1xx:
        return ("E_SIP_REG_1xx");

    case E_SIP_REG_2xx:
        return ("E_SIP_REG_2xx");

    case E_SIP_REG_3xx:
        return ("E_SIP_REG_3xx");

    case E_SIP_REG_4xx:
        return ("E_SIP_REG_4xx");

    case E_SIP_REG_FAILURE_RESPONSE:
        return ("E_SIP_REG_FAILURE_RESPONSE");

    case E_SIP_REG_TMR_EXPIRE:
        return ("E_SIP_REG_TMR_EXPIRE");

    case E_SIP_REG_TMR_ACK:
        return ("E_SIP_REG_TMR_ACK");

    case E_SIP_REG_TMR_WAIT:
        return ("E_SIP_REG_TMR_WAIT");

    case E_SIP_REG_TMR_RETRY:
        return ("E_SIP_REG_TMR_RETRY");

    case E_SIP_REG_CLEANUP:
        return ("E_SIP_REG_CLEANUP");

    default:
        return ("SIP_REG_STATE_UNKNOWN");

    }
}


char *
sip_util_reg_state2string (sipRegSMStateType_t state)
{
    switch (state) {
    case SIP_REG_STATE_NONE:
        return ("SIP_REG_STATE_NONE");

    case SIP_REG_STATE_IDLE:
        return ("SIP_REG_STATE_IDLE");

    case SIP_REG_STATE_REGISTERING:
        return ("SIP_REG_STATE_REGISTERING");

    case SIP_REG_STATE_REGISTERED:
        return ("SIP_REG_STATE_REGISTERED");

    case SIP_REG_STATE_UNREGISTERING:
        return ("SIP_REG_STATE_UNREGISTERING");

    case SIP_REG_STATE_IN_FALLBACK:
        return ("SIP_REG_STATE_IN_FALLBACK");

    case SIP_REG_STATE_STABILITY_CHECK:
        return ("SIP_REG_STATE_STABILITY_CHECK");

    case SIP_REG_STATE_TOKEN_WAIT:
        return ("SIP_REG_STATE_TOKEN_WAIT");

    default:
        return ("SIP_REG_STATE_UNKNOWN");

    }
}


sipRegSMEventType_t
ccsip_register_sip2sipreg_event (int sip_event)
{
    static const char fname[] = "ccsip_register_sip2sipreg";
    sipRegSMEventType_t reg_event = E_SIP_REG_NONE;

    switch (sip_event) {
    case E_SIP_1xx:
        reg_event = E_SIP_REG_1xx;
        break;

    case E_SIP_2xx:
        reg_event = E_SIP_REG_2xx;
        break;

    case E_SIP_3xx:
        reg_event = E_SIP_REG_3xx;
        break;

    case E_SIP_FAILURE_RESPONSE:
        reg_event = E_SIP_REG_4xx;
        break;

    default:
        CCSIP_DEBUG_ERROR("%s: Error: Unknown event.\n", fname);
        reg_event = E_SIP_REG_NONE;
        break;
    }
    return (reg_event);
}


void
ccsip_register_timeout_retry (void *data)
{
    static const char fname[] = "ccsip_register_timeout_retry";
    ccsipCCB_t *ccb;

    CCSIP_DEBUG_REG_STATE(DEB_F_PREFIX"\n", DEB_F_PREFIX_ARGS(SIP_REG, fname));

    ccb = (ccsipCCB_t *) data;
    if (ccb == NULL) {
        CCSIP_DEBUG_ERROR(get_debug_string(DEBUG_GENERAL_FUNCTIONCALL_FAILED),
                          fname, "ccsip_register_timeout_retry");
        return;
    }

    if (ccsip_register_send_msg(SIP_TMR_REG_RETRY, ccb->index) != SIP_REG_OK) {
        ccsip_register_cleanup(ccb, TRUE);
    }
}


/*
 * timer callback function
 *
 * The function will search for the line that set the timer and then
 * (1) send expire event to sm or
 * (2) set another 60s timer. The setting of another timer is necessary
 *     because the max ticks for an OS timer is 10m. The expire timer can
 *     be larger. Therefore, set x 1m timers and count down.
 */
int
ccsip_register_send_msg (uint32_t cmd, line_t ndx)
{
    static const char fname[] = "ccsip_register_send_msg";
    ccsip_registration_msg_t *register_msg;
    ccsipCCB_t *ccb = NULL;
    CCM_ID ccm_id = UNUSED_PARAM;
    ti_config_table_t * cc_cfg_table;

    /*
     * Get the ccm_id type from ccb corresponding to ndx.
     * Currently, only SIP_TMR_REG_RETRY msg in SIPTaskProcessListEvent is
     * making use of ccm_id field. No other reg msg make use of ccm_id. Also in
     * case of SIP_REG_UPDATE, ccm_id will not be determined below but remain
     * initialized to UNUSED_PARAM because, in case of SIP_REG_UPDATE, ndx is
     * not ccb->index but number of available lines when lkem is attached.
     */
    if (cmd != SIP_REG_UPDATE) {
        ccb = sip_sm_get_ccb_by_index(ndx);
        if (ccb != NULL) {
            cc_cfg_table =  (ti_config_table_t *)(ccb->cc_cfg_table_entry);
            if (cc_cfg_table != NULL) {
                ccm_id = cc_cfg_table->ti_specific.ti_ccm.ccm_id;
            }
            else {
                CCSIP_DEBUG_ERROR("%s: Error: cc_cfg_table is null.\n", fname);
                return SIP_ERROR;
            }
        }
        else {
            CCSIP_DEBUG_ERROR("%s: Error: ccb is null.\n", fname);
            return SIP_ERROR;
        }
    }

    CCSIP_DEBUG_REG_STATE(DEB_F_PREFIX"cmd=%d=%s ccb->index=%d ccm_id=%s\n", 
        DEB_F_PREFIX_ARGS(SIP_MSG_SEND, fname),
        cmd, REG_CMD_PRINT(cmd), ndx, CCM_ID_PRINT(ccm_id));

    register_msg = (ccsip_registration_msg_t *) 
        SIPTaskGetBuffer(sizeof(ccsip_registration_msg_t));

    if (!register_msg) {
        CCSIP_DEBUG_ERROR("%s: Error: get buffer failed.\n", fname);
        return SIP_ERROR;
    }
    register_msg->ccb_index = ndx;
    register_msg->ccm_id = ccm_id;

    if (SIPTaskSendMsg(cmd, register_msg, sizeof(ccsip_registration_msg_t), NULL)
            == CPR_FAILURE) {
        cprReleaseBuffer(register_msg);
        CCSIP_DEBUG_ERROR("%s: Error: send buffer failed.\n", fname);
        return SIP_ERROR;
    }

    return SIP_REG_OK;
}

void
ccsip_register_retry_timer_start (ccsipCCB_t *ccb)
{
    static const char fname[] = "ccsip_register_retry_timer_start";
    int timeout;
    int time_t1;
    int time_t2;

    /* Double the timeout value and do exponential time increases
     * The doubling is used because the invite timeout is too short
     */
    config_get_value(CFGID_TIMER_T1, &time_t1, sizeof(time_t1));
    timeout = time_t1 * (1 << ccb->retx_counter);
    config_get_value(CFGID_TIMER_T2, &time_t2, sizeof(time_t2));
    if (timeout > time_t2) {
        timeout = time_t2;
    }
    CCSIP_DEBUG_REG_STATE(DEB_L_C_F_PREFIX"Starting reTx timer (%d msec)\n",
                          DEB_L_C_F_PREFIX_ARGS(SIP_TIMER, ccb->index, ccb->dn_line, fname), timeout);

    ccb->retx_flag = TRUE;
    if (sip_platform_msg_timer_start(timeout, (void *)ccb, ccb->index,
                                     sipPlatformUISMTimers[ccb->index].message_buffer,
                                     sipPlatformUISMTimers[ccb->index].message_buffer_len,
                                     sipMethodRegister,
                                     &(sipPlatformUISMTimers[ccb->index].ipaddr),
                                     sipPlatformUISMTimers[ccb->index].port,
                                     TRUE) != SIP_OK) {
        CCSIP_DEBUG_ERROR(get_debug_string(DEBUG_SIP_FUNCTIONCALL_FAILED),
                          ccb->index, ccb->dn_line, fname,
                          "sip_platform_msg_timer_start");
        ccb->retx_flag = FALSE;
    }
}


void
ccsip_register_cleanup (ccsipCCB_t *ccb, boolean start)
{
    static const char fname[] = "ccsip_register_cleanup";
    int exp_time;

    config_get_value(CFGID_TIMER_REGISTER_EXPIRES, &exp_time, sizeof(exp_time));

    ccb->reg.registered = 0;

    if (ccb->index != REG_BACKUP_CCB) {
        ui_set_sip_registration_state(ccb->dn_line, FALSE);
    }

    sip_stop_ack_timer(ccb);

    if ((start) && (ccb->state != (int) SIP_REG_STATE_UNREGISTERING)) {
        if (ccb->index == REG_BACKUP_CCB) {
            ccb->reg.tmr_expire = (exp_time > 5) ? exp_time - 5 : exp_time;
        } else {
            ccb->reg.tmr_expire = 60;
        }
        ccb->reg.act_time = (int) time(NULL);

        CCSIP_DEBUG_STATE(DEB_L_C_F_PREFIX"Starting expires timer (%d "
                          "sec)\n", DEB_L_C_F_PREFIX_ARGS(SIP_TIMER, ccb->index, ccb->dn_line, fname),
                          ccb->reg.tmr_expire);
        (void) sip_platform_register_expires_timer_start(ccb->reg.tmr_expire * 1000,
                                                         ccb->index);
    }

    sip_reg_sm_change_state(ccb, SIP_REG_STATE_IDLE);

    if (ccsip_register_all_unregistered() == TRUE) {
        ccsip_register_set_register_state(SIP_REG_IDLE);
    }

    /*
     * Always check how sip_sm_call_cleanup is implemented.
     * The function has a tendency to change.
     */

    /* Cleanup and re-initialize CCB */
    sip_sm_call_cleanup(ccb);
}


cc_int32_t
ccsip_register_cmd (cc_int32_t argc, const char *argv[])
{ 
    /*
     * On 7970 phones the command is reg line [option] [line]
     * On 7940/60 the command is reg [option] [line]
     * Thus the location in the argument array is different
     * based on the phone type. OFFSET is used to get around
     * this difference while still having common code.
     */
#define OFFSET 1
    ccsipCCB_t *ccb = NULL;
    line_t ndx = 0;
    line_t temp_line = 0;
    char str_val[MAX_LINE_NAME_SIZE];
    //line_t available = 0;
    //line_t present = 0;
    //line_t configured = 0;

    /*
     * check if need help
     */
    if ((argc == 2 + OFFSET) && (argv[1 + OFFSET][0] == '?')) {
        debugif_printf("reg [option] [line]\n");
        debugif_printf("    options = 0: unregister\n");
        debugif_printf("              1: register\n");
        debugif_printf("    line    = 1 through 6\n");
        debugif_printf("            = backup (line 1 to backup proxy)\n");
        debugif_printf("\n");
        return (0);
    }else if (cpr_strcasecmp(argv[0 + OFFSET], "line") == 0) {

       /*
        make sure the user entered a complete command
       */
        if (argc < 3 + OFFSET) {
            debugif_printf("Incomplete command\n");
            debugif_printf("reg [option] [line]\n");
            debugif_printf("    options = 0: unregister\n");
            debugif_printf("              1: register\n");
            debugif_printf("    line    = 1 through 6\n");
            debugif_printf("            = backup (line 1 to backup proxy)\n");
            debugif_printf("\n");
            return (0);
        }

        /* make sure we have at least one line configured */
        config_get_line_string(CFGID_LINE_NAME, str_val, 1, sizeof(str_val));
        if ((strcmp(str_val, UNPROVISIONED) == 0) || (str_val[0] == '\0')) {
            debugif_printf("Error:No lines configured\n");
            return (0);
        }

        if (cpr_strcasecmp(argv[2 + OFFSET], "backup") == 0) {
            temp_line = REG_BACKUP_LINE;
        } else {
            temp_line = (line_t) atoi(argv[2 + OFFSET]);
            if (!sip_config_check_line(temp_line)) {
                debugif_printf("Error:Invalid Line\n");
                return (0);
            }
        }
   

            ndx = SIP_REG_REGLINE2LINE(temp_line - 1);
            ccb = sip_sm_get_ccb_by_index(ndx);
       
        if (ccb == NULL) {
            debugif_printf("Unable to retrieve registration information for this line.\n");
            debugif_printf("Command aborted.\n");
            return (0);
        }

        if ((temp_line == REG_BACKUP_LINE) && util_check_if_ip_valid(&(ccb->dest_sip_addr)) == FALSE) {
            debugif_printf("Backup Proxy is invalid or not configured.\n");
            return (0);
        }

        switch (argv[1 + OFFSET][0]) {
        case '0':

            if (ccb->index != REG_BACKUP_CCB) {
                ui_set_sip_registration_state(ccb->dn_line, FALSE);
            }

            if (temp_line != REG_BACKUP_LINE) {
                debugif_printf("Unregistering line %d\n", temp_line);
            } else {
                debugif_printf("Unregistering line 1 to backup proxy\n");
            }
            (void) sip_platform_register_expires_timer_stop(ccb->index);
            sip_stop_ack_timer(ccb);

            if (ccsip_register_send_msg(SIP_REG_CANCEL, ndx) != SIP_REG_OK) {
                ccsip_register_cleanup(ccb, FALSE);
            }
            break;

        case '1':

            /* Cleanup and re-initialize CCB */
            sip_sm_call_cleanup(ccb);

            if (temp_line != REG_BACKUP_LINE) {
                debugif_printf("Registering line %d\n", temp_line);
            } else {
                debugif_printf("Registering line 1 to backup proxy\n");
            }
            (void) ccsip_register_send_msg(SIP_REG_REQ, ndx);
            break;

        default:
            debugif_printf("Invalid Option Value - please choose 1 or 0\n");
            return (0);
        }
    }
    return (0);
}

/*
 *  Function: build_reg_flags
 *
 *  Parameters:
 *
 *  Description: builds a 3 character string representing various flags
 *               for use by the show_reg command
 *
 *  Returns: none
 *
 */
static void
build_reg_flags (char *buf, int buf_size, int prox_reg, ccsipCCB_t *ccb,
                 boolean provisioned)
{
    strncpy(buf, "...", buf_size);
    if (!provisioned || !prox_reg) {
        return;
    }

    buf[1] = '1'; // Set the Provisioned bit

    if (ccb->authen.authorization != NULL) {
        buf[0] = '1';
    } else {
        if (ccb->authen.status_code != 0) {
            buf[0] = 'x';
        }
    }
    if (ccb->reg.registered) {
        buf[2] = '1';
    } else {
        buf[2] = 'x';
    }
}

/*
 *  Function: show_register_data
 *
 *  Parameters:
 *
 *  Description:  Internal print routine for show_register_cmd. Also
 *                called by reg command
 *
 *  Returns:
 *
 */
#define TMP_BUF_SIZE 8
static void
show_register_data (void)
{
    ccsipCCB_t *ccb;
    char       buf[TMP_BUF_SIZE];
    line_t     ndx = 0;
    int        proxy_register = 0;
    boolean    valid_line = FALSE;
    int        timer_count_down = 0;

    config_get_value(CFGID_PROXY_REGISTER, &proxy_register,
                     sizeof(proxy_register));

    debugif_printf("\nLINE REGISTRATION TABLE\nProxy Registration: ");
    if (proxy_register == 1) {
        debugif_printf("ENABLED");
    } else {
        debugif_printf("DISABLED");
    }
    debugif_printf(", state: %s\n",
                   ccsip_register_state_name(ccsip_register_get_register_state()));
    debugif_printf("line  APR  state          timer       expires     proxy:port\n");
    debugif_printf("----  ---  -------------  ----------  ----------  ----------------------------\n");
    for (ndx = REG_CCB_START; ndx <= REG_BACKUP_CCB; ndx++) {
        ccb = sip_sm_get_ccb_by_index(ndx);
        valid_line = FALSE;
        if (ndx == REG_BACKUP_CCB) {
            valid_line = TRUE;
        } else {
            if (sip_config_check_line((line_t)(ndx - TEL_CCB_END))) {
                valid_line = TRUE;
            }
        }
        // calc the time until registration expires
        if (ccb && valid_line) {
            if (!proxy_register || !valid_line) {
                timer_count_down = 0;
            } else {
                if (ccb->reg.act_time == 0) {
                    timer_count_down = 0;
                } else if ((sipRegSMStateType_t)ccb->state ==
                        SIP_REG_STATE_REGISTERING) {
                    timer_count_down =
                        (SIP_REG_TMR_ACK_TICKS / 1000) -
                        (((int) time(NULL)) - ccb->reg.act_time);
                } else {
                    timer_count_down = ccb->reg.tmr_expire -
                        (((int) time(NULL)) - ccb->reg.act_time);
                }
            }

	    /*
             * Ensure that the timer_count_down value is valid. There
             * are cases where the phone completes registration before an
             * SNTP/NTP response is received which makes the act_time and 
             * time(NULL) values inconsistent.
             */
	    if ((timer_count_down < 0) ||
		(timer_count_down > ccb->reg.tmr_expire)) {
                 
		timer_count_down = 0;
	    } 
        }

        // build the flag string
        if (ccb) {
            build_reg_flags((char *)&buf, TMP_BUF_SIZE, proxy_register,
                            ccb, valid_line);
        }

        if (ndx != REG_BACKUP_CCB) {
            debugif_printf("%-4d", ndx - TEL_CCB_END);
        } else {
            debugif_printf("1-BU");
        }
        if (ccb && valid_line) {
            debugif_printf("  %-3s  %-13s  %-10d  %-10d  %s:%d\n",
                           buf,
                           ccsip_register_reg_state_name((sipRegSMStateType_t)ccb->state),
                           ccb->reg.tmr_expire,
                           timer_count_down,
                           ((ccb->reg.proxy[0] != '\0') ? (ccb->reg.proxy) : ("undefined")),
                           ccb->reg.port);
        } else {
            debugif_printf("  %-3s  %-13s  %-10d  %-10d  %s:%d\n",
                           buf, "NONE", 0, 0, "undefined", 0);
        }
    }
    debugif_printf("\nNote: APR is Authenticated, Provisioned, Registered\n");
}

/*
 *  Function: show_register_cmd
 *
 *  Parameters:  argc,argv
 *
 *  Description:  Shows the current registration table
 *
 *  Returns:
 *
 */
cc_int32_t
show_register_cmd (cc_int32_t argc, const char *argv[])
{

    /*
     * check if need help
     */
    if ((argc == 2) && (argv[1][0] == '?')) {
        debugif_printf("sh reg\n");
        return (0);
    }
    show_register_data();

    return (0);
}

void
ccsip_register_clear_all_logs (void)
{
    log_clear(LOG_REG_MSG);
    log_clear(LOG_REG_AUTH);
    log_clear(LOG_REG_RETRY);
    log_clear(LOG_REG_UNSUPPORTED);
}


void
ccsip_register_reset_proxy (void)
{
    ccsipCCB_t *ccb;
    line_t ndx;
    line_t line_end;

    /*
     * If this is a restart (phone is resetting) then reset the proxy
     * string for each line.  The proxy string is used to determine
     * what address to use when registering. The proxy remains the
     * same throughout a boot cycle and on restarts there is a new
     * boot cycle so the proxy address is reset.
     */
    line_end = 1;

    /*
     * REG CCBs start after the TEL CCBs.
     * EG., TEL CCBs are at 1 and 2 and REG CCBs are at 3 and 4.
     * So, line_end equals the number of lines and then add the TEL_CCB_END
     * to get the ending of the REG CCBs
     */
    line_end += TEL_CCB_END;

    //ccsip_register_lines = line_end - REG_CCB_START + 1;
    for (ndx = REG_CCB_START; ndx <= line_end; ndx++) {
        if (sip_config_check_line((line_t)(ndx - TEL_CCB_END))) {
            ccb = sip_sm_get_ccb_by_index(ndx);
            if (ccb) {
                ccb->reg.proxy[0] = '\0';
            }
        }
    }

    //reset backup proxy registration;
    ccb = sip_sm_get_ccb_by_index(REG_BACKUP_CCB);
    if (ccb) {
        ccb->reg.proxy[0] = '\0';
    }
}

int
ccsip_register_init (void)
{
    static const char fname[] = "ccsip_register_init";
    static const char sipAckTimerName[] = "sipAck";
    int i;

    ccsip_register_set_register_state(SIP_REG_IDLE);
    
    /*
     * Create acknowledgement timers
     */
    for (i = 0; i < MAX_REG_LINES + 1; i++) {
        ack_tmrs[i] = cprCreateTimer(sipAckTimerName, SIP_ACK_TIMER,
                                     TIMER_EXPIRATION, sip_msgq);
        if (ack_tmrs[i] == NULL) {
            CCSIP_DEBUG_ERROR("%s: timer NOT created: %d\n",
                              fname, i);
            return SIP_ERROR;
        }
    }

    // Initialize date holder structure
    ccm_date.valid = FALSE;
    ccm_date.datestring[0] = '\0';
    start_standby_monitor = TRUE;

    return SIP_OK;
}


ccsip_register_states_t
ccsip_register_get_register_state (void)
{
    return (ccsip_register_state);
}


void
ccsip_register_set_register_state (ccsip_register_states_t state)
{
    ccsip_register_state = state;
}

void
ccsip_register_cancel (boolean cancel_reg, boolean backup_proxy)
{
    static const char fname[] = "ccsip_register_cancel";
    ccsipCCB_t *ccb;
    line_t     ndx;
    line_t     line_end;


    CCSIP_DEBUG_REG_STATE(DEB_F_PREFIX"\n", DEB_F_PREFIX_ARGS(SIP_REG, fname));

    if (ccsip_register_get_register_state() == SIP_REG_IDLE) {
        return;
    }

    ccsip_register_set_register_state(SIP_REG_UNREGISTERING);

    line_end = 1;

    ui_set_sip_registration_state((line_t) (line_end + 1), FALSE);

    CCSIP_DEBUG_REG_STATE(DEB_F_PREFIX"unregistering %d lines\n", DEB_F_PREFIX_ARGS(SIP_REG, fname), line_end);

    /*
     * REG CCBs start after the TEL CCBs.
     * EG., TEL CCBs are at 1 and 2 and REG CCBs are at 3 and 4.
     * So, line_end equals the number of lines and then add the TEL_CCB_END
     * to get the ending of the REG CCBs
     */
    line_end += TEL_CCB_END;

    for (ndx = REG_CCB_START; ndx <= line_end; ndx++) {
        if (sip_config_check_line((line_t) (ndx - TEL_CCB_END))) {
            ccb = sip_sm_get_ccb_by_index(ndx);
            if (!ccb) {
                continue;
            }
            ui_set_sip_registration_state(ccb->dn_line, FALSE);

            CCSIP_DEBUG_REG_STATE(DEB_F_PREFIX"cancelling timers, line= %d\n",
                                  DEB_F_PREFIX_ARGS(SIP_TIMER, fname), ccb->index);
            (void) sip_platform_register_expires_timer_stop(ccb->index);
            sip_stop_ack_timer(ccb);

            if (cancel_reg) {
                if (ccb->index == REG_CCB_START) {
                    ccb->send_reason_header = TRUE;
                }
                if (ccb->index == REG_CCB_START) {
                     ccb->send_reason_header = TRUE;
                } 

                    if (ccsip_register_send_msg(SIP_REG_CANCEL, ndx)
                        != SIP_REG_OK) {
                        ccsip_register_cleanup(ccb, FALSE);
                    }

            } else {
                /* Cleanup and re-initialize CCB */
                sip_sm_call_cleanup(ccb);
                if (ndx == line_end) {
                    ccsip_register_set_register_state(SIP_REG_IDLE);
                }
            }
        }
    }

    if (backup_proxy == FALSE) {
        return;
    }

    /* cancel backup registration as well */
    ccb = sip_sm_get_ccb_by_index(REG_BACKUP_CCB);

    CCSIP_DEBUG_REG_STATE(DEB_F_PREFIX"cancelling timers, line= %d\n",
                          DEB_F_PREFIX_ARGS(SIP_TIMER, fname), ccb->index);

    (void) sip_platform_register_expires_timer_stop(ccb->index);
    sip_stop_ack_timer(ccb);

    if (cancel_reg) {
        if (ccsip_register_send_msg(SIP_REG_CANCEL, REG_BACKUP_CCB)
                != SIP_REG_OK) {
            ccsip_register_cleanup(ccb, FALSE);
        }
    } else {
        /* Cleanup and re-initialize CCB */
        sip_sm_call_cleanup(ccb);
    }
}


void
ccsip_register_all_lines (void)
{
    static const char fname[] = "ccsip_register_all_lines";
    ccsipCCB_t     *ccb = 0;
    line_t          ndx;
    line_t          line;
    line_t          line_end = 1;
    int             value = 0;
    char            proxyaddress[MAX_IPADDR_STR_LEN];

    CCSIP_DEBUG_REG_STATE(DEB_F_PREFIX"\n", DEB_F_PREFIX_ARGS(SIP_REG, fname));

    /* Do not register if
     * - no need to register
     * - and already registered
     */

    line_end = 1;
    config_get_value(CFGID_PROXY_REGISTER, &value, sizeof(value));
    if (value == 0) {
        CCSIP_DEBUG_REG_STATE(get_debug_string(DEBUG_REG_DISABLED), NULL,
                              NULL, fname);

        // mark all unregistered
        for (line = 1; line <= line_end; line++) {
            if (sip_config_check_line(line)) {
                ui_set_sip_registration_state(line, FALSE);
            }
        }
        ccsip_register_reset_proxy();
        return;
    } else if (ccsip_register_get_register_state() == SIP_REG_REGISTERED) {
        CCSIP_DEBUG_REG_STATE(DEB_F_PREFIX"lines already registered\n", DEB_F_PREFIX_ARGS(SIP_REG, fname));

        return;
    }


    ccsip_register_reset_proxy();
    ccsip_register_set_register_state(SIP_REG_REGISTERING);

    CCSIP_DEBUG_REG_STATE(DEB_F_PREFIX"registering %d line%c\n", 
                          DEB_F_PREFIX_ARGS(SIP_REG, fname), line_end, line_end > 1 ? 's' : ' ');

    /* register line 1 to the backup proxy.
     * Start with the backup as DNS lookups hold
     * the task meaning it could take a while to
     * register the six regular lines.
     */
    ndx = REG_BACKUP_CCB;
    ccb = sip_sm_get_ccb_by_index(ndx);
    /* Cleanup and re-initialize CCB */
    sip_sm_call_cleanup(ccb);
    if (ccb->cc_type == CC_CCM) {
        /*
         * regmgr - Set the Backup (standby ccm) info.
         */
        ti_config_table_t *standby_ccm =
            CCM_Active_Standby_Table.standby_ccm_entry;

        if (standby_ccm) {
            ti_ccm_t *ti_ccm = &standby_ccm->ti_specific.ti_ccm;

            sip_regmgr_setup_new_standby_ccb(ti_ccm->ccm_id);
        } else {
            CCSIP_DEBUG_REG_STATE(DEB_F_PREFIX"ERROR: Standby ccm entry is NULL\n",
                                  DEB_F_PREFIX_ARGS(SIP_REG, fname));
        }
    } else {
        /*
         * Assume CSPS since only those two for now.
         */
        if (util_check_if_ip_valid(&(ccb->dest_sip_addr))) {
            CCSIP_DEBUG_REG_STATE(DEB_L_C_F_PREFIX"%d, 0x%x\n", 
                                  DEB_L_C_F_PREFIX_ARGS(SIP_REG, ccb->index, ccb->dn_line, fname),
                                  ndx, ccb);

            ccb->reg.addr = ccb->dest_sip_addr;
            ccb->reg.port = (uint16_t) ccb->dest_sip_port;

            if (ccb->index == REG_CCB_START) {
                ccb->send_reason_header = TRUE;
            } else {
                ccb->send_reason_header = FALSE;
            }

            if (ccsip_register_send_msg(SIP_REG_REQ, ndx) != SIP_REG_OK) {
                ccsip_register_cleanup(ccb, TRUE);
            }
        } else {
            CCSIP_DEBUG_REG_STATE(DEB_L_C_F_PREFIX"%d: Backup Proxy not configured\n",
                                  DEB_L_C_F_PREFIX_ARGS(SIP_REG, ccb->index, ccb->dn_line, fname), ndx);
        }
    }

    /*
     * REG CCBs start after the TEL CCBs.
     * EG., TEL CCBs are at 1 and 2 and REG CCBs are at 3 and 4.
     * So, line_end equals the number of lines and then add the TEL_CCB_END to
     * get the ending of the REG CCBs
     */
    line_end += TEL_CCB_END;

    DEF_DEBUG(DEB_F_PREFIX"Disabling mass reg state", DEB_F_PREFIX_ARGS(SIP_REG, fname));
    for (ndx = REG_CCB_START; ndx <= line_end; ndx++) {
        if (sip_config_check_line((line_t)(ndx - TEL_CCB_END))) {
            ccb = sip_sm_get_ccb_by_index(ndx);
            if (!ccb) {
                continue;
            }

            CCSIP_DEBUG_REG_STATE(DEB_L_C_F_PREFIX"%d, 0x%x\n", 
                                  DEB_L_C_F_PREFIX_ARGS(SIP_REG, ccb->index, ccb->dn_line, fname),
                                  ndx, ccb);


            /*
             * Clean up and re-initialize the proxy address.
             * It is possible that the REGISTER can be redirected
             * and this address needs to be reused for later register attempts.
             * But, on restarts and re-enabling of registering we reset back
             * to the original configured proxy
             */
            if (ndx == REG_CCB_START || ndx == (line_end)) {
                g_disable_mass_reg_debug_print = FALSE;
            } else {
                g_disable_mass_reg_debug_print = TRUE;
            }
            sip_sm_call_cleanup(ccb);

            /*
             *  Look up the name of the proxy
             */
            sipTransportGetPrimServerAddress(ccb->dn_line, proxyaddress);

            sstrncpy(ccb->reg.proxy, proxyaddress, MAX_IPADDR_STR_LEN);

            ccb->reg.addr = ccb->dest_sip_addr;
            ccb->reg.port = (uint16_t) ccb->dest_sip_port;

            if (ccb->index == REG_CCB_START) {
                ccb->send_reason_header = TRUE;
            } else {
                ccb->send_reason_header = FALSE;
            }

                ui_set_sip_registration_state(ccb->dn_line, FALSE);
                if (ccsip_register_send_msg(SIP_REG_REQ, ndx) != SIP_REG_OK) {
                    ccsip_register_cleanup(ccb, TRUE);
                } 

        }
    }
    g_disable_mass_reg_debug_print = FALSE;
   
    sip_platform_cc_mode_notify();  
}


boolean
ccsip_register_all_registered (void)
{
    ccsipCCB_t *ccb;
    line_t ndx;
    line_t line_end;

    line_end = 1;

    /*
     * REG CCBs start after the TEL CCBs.
     * EG., TEL CCBs are at 1 and 2 and REG CCBs are at 3 and 4.
     * So, line_end equals the number of lines and then add the TEL_CCB_END
     * to get the ending of the REG CCBs
     */
    line_end += TEL_CCB_END;

    for (ndx = REG_CCB_START; ndx <= line_end; ndx++) {
        if (sip_config_check_line((line_t)(ndx - TEL_CCB_END))) {
            ccb = sip_sm_get_ccb_by_index(ndx);

            if (ccb && (ccb->reg.registered == 0)) {
                return (FALSE);
            }
        }
    }
    return (TRUE);
}


boolean
ccsip_register_all_unregistered (void)
{
    ccsipCCB_t *ccb;
    line_t ndx;
    line_t line_end;

    line_end = 1;

    /*
     * REG CCBs start after the TEL CCBs.
     * EG., TEL CCBs are at 0 and 1 and REG CCBs are at 6 and 7.
     * So, line_end equals the number of lines and then add the TEL_CCB_END
     * to get the ending of the REG CCBs
     */
    line_end += TEL_CCB_END;

    for (ndx = REG_CCB_START; ndx <= line_end; ndx++) {
        if (sip_config_check_line((line_t)(ndx - TEL_CCB_END))) {
            ccb = sip_sm_get_ccb_by_index(ndx);

            if (ccb && (ccb->reg.registered == 1)) {
                return (FALSE);
            }
        }
    }
    return (TRUE);
}


void
ccsip_register_commit (void)
{
    static const char fname[] = "ccsip_register_commit";
    int register_get;

    CCSIP_DEBUG_REG_STATE(DEB_F_PREFIX"\n", DEB_F_PREFIX_ARGS(SIP_REG, fname));

    /*
     * Determine if lines need to register or unregister
     *
     * register if
     * - proxy_register was changed from 0 to 1
     *
     * unregister if
     * - proxy_register was changed from 1 to 0 and already registered
     */
    config_get_value(CFGID_PROXY_REGISTER, &register_get,
                     sizeof(register_get));

    switch (register_get) {
    case 0:
        if (ccsip_register_get_register_state() != SIP_REG_IDLE) {
            ccsip_register_cancel(TRUE, TRUE);
        } else {
            CCSIP_DEBUG_REG_STATE(DEB_F_PREFIX"lines already unregistered.\n", DEB_F_PREFIX_ARGS(SIP_REG, fname));
        }

        break;

    case 1:
        if (ccsip_register_get_register_state() != SIP_REG_REGISTERED) {
            ccsip_register_cancel(FALSE, TRUE);
            ccsip_register_all_lines();
        } else {
            CCSIP_DEBUG_REG_STATE(DEB_F_PREFIX"lines already registered.\n", DEB_F_PREFIX_ARGS(SIP_REG, fname));
        }
        break;

    default:
        CCSIP_DEBUG_ERROR(DEB_F_PREFIX"Error: invalid register_get= %d\n", DEB_F_PREFIX_ARGS(SIP_REG, fname),
                          register_get);
    }
}

void
ccsip_backup_register_commit (void)
{
    static const char fname[] = "ccsip_backup_register_commit";
    line_t ndx;
    ccsipCCB_t *ccb = 0;

    CCSIP_DEBUG_REG_STATE(DEB_F_PREFIX"\n", DEB_F_PREFIX_ARGS(SIP_REG, fname));

    ndx = REG_BACKUP_CCB;
    ccb = sip_sm_get_ccb_by_index(ndx);

    /* cancel an existing registration if it exists */
    if (util_check_if_ip_valid(&(ccb->reg.addr))) {
        CCSIP_DEBUG_REG_STATE(DEB_F_PREFIX"cancelling registration, line= %d\n",
                              DEB_F_PREFIX_ARGS(SIP_REG, fname), ccb->index);
        if (ccsip_register_send_msg(SIP_REG_CANCEL, ndx) != SIP_REG_OK) {
            ccsip_register_cleanup(ccb, FALSE);
        }
    }

    /* Cleanup and re-initialize CCB */
    sip_sm_call_cleanup(ccb);

    if (util_check_if_ip_valid(&(ccb->dest_sip_addr))) {
        CCSIP_DEBUG_REG_STATE(DEB_L_C_F_PREFIX"%d, 0x%x\n", 
                              DEB_L_C_F_PREFIX_ARGS(SIP_REG, ccb->index, ccb->dn_line, fname), 
                              ndx, ccb);
        ccb->reg.addr = ccb->dest_sip_addr;
        ccb->reg.port = (uint16_t) ccb->dest_sip_port;

        if (ccsip_register_send_msg(SIP_REG_REQ, ndx) != SIP_REG_OK) {
            ccsip_register_cleanup(ccb, TRUE);
        }
    } else {
        log_clear(LOG_REG_BACKUP);
    }
}


void
cred_get_line_credentials (line_t line, credentials_t *pcredentials,
                           int id_len, int pw_len)
{
    config_get_line_string(CFGID_LINE_AUTHNAME, pcredentials->id, line,
                           id_len);

    if ((strcmp(pcredentials->id, "") == 0) ||
        (strcmp(pcredentials->id, UNPROVISIONED) == 0)) {
        config_get_line_string((CFGID_LINE_AUTHNAME), pcredentials->id, 1,
                               id_len);
    }

    config_get_line_string(CFGID_LINE_PASSWORD, pcredentials->pw, line,
                           pw_len);
    if ((strcmp(pcredentials->pw, "") == 0) ||
        (strcmp(pcredentials->pw, UNPROVISIONED) == 0)) {
        config_get_line_string(CFGID_LINE_PASSWORD, pcredentials->pw, 1,
                               pw_len);
    }
}


boolean
cred_get_credentials_r (ccsipCCB_t *ccb, credentials_t *pcredentials)
{
    if (!(ccb->authen.cred_type & CRED_LINE) ||
        (ccb->authen.retries_401_407 < MAX_RETRIES_401)) {
        ccb->authen.cred_type |= CRED_LINE;
        ccb->authen.retries_401_407++;
        cred_get_line_credentials(ccb->dn_line, pcredentials,
                                  sizeof(pcredentials->id),
                                  sizeof(pcredentials->pw));
        return (TRUE);
    }

    return (FALSE);
}


const char *
ccsip_register_state_name (ccsip_register_states_t state)
{
    if ((state < SIP_REG_IDLE) || (state >= SIP_REG_NO_REGISTER)) {
        return ("UNDEFINED");
    }

    return (ccsip_register_state_names[state]);
}


const char *
ccsip_register_reg_state_name (sipRegSMStateType_t state)
{
    if ((state < SIP_REG_STATE_NONE) || (state >= SIP_REG_STATE_UNREGISTERING)) {
        return ("UNDEFINED");
    }

    return (ccsip_register_reg_state_names[state]);
}

void
ccsip_register_shutdown ()
{
    int i;

    for (i = 0; i < MAX_REG_LINES + 1; i++) {
        (void) cprCancelTimer(ack_tmrs[i]);
        (void) cprDestroyTimer(ack_tmrs[i]);
        ack_tmrs[i] = NULL;
    }
}

boolean
ccsip_get_ccm_date (char *date_value)
{
    if (date_value) {
        if (ccm_date.valid) {
            sstrncpy(date_value, ccm_date.datestring,
                     sizeof(ccm_date.datestring));
            return (TRUE);
        } else {
            date_value[0] = '\0';
        }
    }
    return (FALSE);
}

/*
 *  Function: ccsip_is_line_registered
 *
 *  Parameters:
 *      dn_line - line number.
 *
 *  Description:  The function determines whether the current line
 *      is registered or not.
 *
 *  Returns:
 *      TRUE if the line is registered.
 *      FALSE if the line is not registered or the given line is invalid.
 *
 */
boolean
ccsip_is_line_registered (line_t dn_line)
{
    line_t ndx;
    line_t line_end;
    ccsipCCB_t *ccb;

    /* Get the number of lines configured */
    line_end = 1;
    /*
     * REG CCBs start after the TEL CCBs.
     * So, line_end equals the number of lines and then add the TEL_CCB_END
     * to get the ending of the REG CCBs
     */
    line_end += TEL_CCB_END;

    for (ndx = REG_CCB_START; ndx <= line_end; ndx++) {
        ccb = sip_sm_get_ccb_by_index(ndx);
        if ((ccb != NULL) && (ccb->dn_line == dn_line)) {
            /* found the requested REG ccb */
            if (ccb->state == (int) SIP_REG_STATE_REGISTERED) {
                return (TRUE);
            } else {
                /* the line is not in registered state */
                break;
            }
        }
    }

    /* Found no matching REG ccb or the line is not in registered state */
    return (FALSE);
}

/*
 *  Function: process_retry_after
 *
 *  Parameters:
 *      ccsipCCB_t *ccb - The reg ccb.
 *      sipMessage_t *response - The received response.
 *
 *  Description:  The function processes the Retry-After header that may
 *       be present in the response.
 *
 *  Returns:
 *      TRUE if a valid Retry-After header was present and processed correctly.
 *      FALSE if a valid Retry-After header was not present and therefore
 *            was not processed.
 *
 */
boolean
process_retry_after (ccsipCCB_t *ccb, sipMessage_t *response)
{
    const char *msg_ptr = NULL;
    int32_t    retry_after = 0;
    static const char fname[] = "process_retry_after";

    msg_ptr = sippmh_get_header_val(response,
                                    (const char *)SIP_HEADER_RETRY_AFTER,
                                    NULL);

    if (msg_ptr) {
        retry_after = strtoul(msg_ptr, NULL, 10);

    } else {
        return (FALSE);
    }

    if (retry_after > 0) {
        sip_stop_ack_timer(ccb);
        (void) sip_platform_register_expires_timer_start(retry_after * 1000,
                                                         ccb->index);
        CCSIP_DEBUG_REG_STATE(DEB_L_C_F_PREFIX"Retrying after %d\n",
                              DEB_L_C_F_PREFIX_ARGS(SIP_REG, ccb->index, ccb->dn_line, fname), retry_after);
        return (TRUE);
    } else {
        CCSIP_DEBUG_ERROR("REG %d/%d: %-35s: Error: invalid Retry-After "
                          "header in response.\n",
                          ccb->index, ccb->dn_line, fname);
        return (FALSE);
    }
}


