/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "cpr_types.h"
#include "cpr_memory.h"
#include "cpr_stdio.h"
#include "cpr_string.h"
#include "cpr_timers.h"
#include "cpr_ipc.h"
#include "debug.h"
#include "phone_debug.h"
#include "phntask.h"
#include "prot_configmgr.h"
#include "logger.h"
#include "logmsg.h"
#include "upgrade.h"
#include "dialplan.h"
#include "singly_link_list.h"
#include "ccsip_subsmanager.h"
#include "kpmlmap.h"
#include "dialplanint.h"
#include "ccapi.h"
#include "lsm.h"
#include "gsm.h"
#include "fsm.h"
#include "regmgrapi.h"
#include "ccsip_register.h"

#define PRIME_LINE_ID 1
cc_int32_t DpintDebug = 0;
static dp_data_t g_dp_int;

/* note that we do not allow file with size more than DIALPLAN_MAX_SIZE (8k) */
uint8_t dpLoadArea[DIALPLAN_MAX_SIZE - 1];


#define KPML_DEFAULT_DIALPLAN "<DIALTEMPLATE><TEMPLATE MATCH=\".\"     Timeout=\"0\" User=\"Phone\"/></DIALTEMPLATE>"

#define DIAL_KEY 0x82

static void dp_check_dialplan(line_t line, callid_t call_id,
                              unsigned char digit);
static void dp_restart_dial_timer (line_t line, callid_t call_id, int timeout);

extern cprBuffer_t cc_get_msg_buf(int min_size);
extern cc_int32_t show_dialplan_cmd(cc_int32_t argc, const char *argv[]);


static void
dp_int_message (line_t line, callid_t call_id, char digit,
                char *digit_str, boolean collect_more, void *tmr_data,
                int msg_id, char *g_call_id, monitor_mode_t monitor_mode)
{
    char fname[] = "dp_int_message";
    dp_int_t *pmsg;

    pmsg = (dp_int_t *) cc_get_msg_buf(sizeof(dp_int_t));

    if (!pmsg) {
        CSFLogError("src-common", get_debug_string(CC_NO_MSG_BUFFER), fname);
        return;
    }

    pmsg->line = line;
    pmsg->call_id = call_id;
    pmsg->digit = digit;
    if (digit_str) {
        sstrncpy(pmsg->digit_str, digit_str, MAX_DIALSTRING);
    }
    pmsg->collect_more = collect_more;
    pmsg->tmr_ptr = tmr_data;
    if (digit_str) {
        sstrncpy(pmsg->global_call_id, g_call_id, CC_GCID_LEN);
    }

    pmsg->monitor_mode = monitor_mode;
    if (gsm_send_msg(msg_id, (cprBuffer_t) pmsg, sizeof(dp_int_t)) !=
        CPR_SUCCESS) {
        CSFLogError("src-common", get_debug_string(CC_SEND_FAILURE), fname);
    }
}

void
dp_int_init_dialing_data (line_t line, callid_t call_id)
{
    dp_int_message(line, call_id, 0, NULL, FALSE, NULL, DP_MSG_INIT_DIALING,
                   NULL, CC_MONITOR_NONE);
}

void
dp_dial_timeout (void *data)
{
    DPINT_DEBUG(DEB_F_PREFIX"", DEB_F_PREFIX_ARGS(DIALPLAN, "dp_dial_timeout"));
    dp_int_message(0, 0, 0, NULL, FALSE, &data, DP_MSG_DIGIT_TIMER, NULL, CC_MONITOR_NONE);
}

void
dp_int_update_key_string (line_t line, callid_t call_id, char *digits)
{
    dp_int_message(line, call_id, 0, digits, FALSE, NULL, DP_MSG_DIGIT_STR,
                   NULL, CC_MONITOR_NONE);
}

void
dp_int_store_digit_string (line_t line, callid_t call_id, char* digit_str)
{
    dp_int_message(line, call_id, 0, digit_str, FALSE, NULL, DP_MSG_STORE_DIGIT,
                   NULL, CC_MONITOR_NONE);
}

void
dp_int_update_keypress (line_t line, callid_t call_id, unsigned char digit)
{
    dp_int_message(line, call_id, digit, NULL, FALSE,
                   NULL, DP_MSG_DIGIT, NULL, CC_MONITOR_NONE);
}

void
dp_int_dial_immediate (line_t line, callid_t call_id, boolean collect_more,
                       char *digit_str, char *g_call_id,
                       monitor_mode_t monitor_mode)
{
    dp_int_message(line, call_id, 0, digit_str, collect_more, NULL,
                   DP_MSG_DIAL_IMMEDIATE, g_call_id, monitor_mode);
}

void
dp_int_do_redial (line_t line, callid_t call_id)
{
    dp_int_message(line, call_id, 0, NULL, FALSE,
                   NULL, DP_MSG_REDIAL, NULL, CC_MONITOR_NONE);
}

void
dp_int_onhook (line_t line, callid_t call_id)
{
    dp_int_message(line, call_id, 0, NULL, FALSE,
                   NULL, DP_MSG_ONHOOK, NULL, CC_MONITOR_NONE);
}

void
dp_int_offhook (line_t line, callid_t call_id)
{
    dp_int_message(line, call_id, 0, NULL, FALSE,
                   NULL, DP_MSG_OFFHOOK, NULL, CC_MONITOR_NONE);
}

void
dp_int_update (line_t line, callid_t call_id, string_t called_num)
{
    dp_int_message(line, call_id, 0, (char *) called_num, FALSE, NULL,
                   DP_MSG_UPDATE, NULL, CC_MONITOR_NONE);
}

void
dp_int_cancel_offhook_timer (line_t line, callid_t call_id)
{
    dp_int_message(line, call_id, 0, NULL, FALSE,
                   NULL, DP_MSG_CANCEL_OFFHOOK_TIMER, NULL, CC_MONITOR_NONE);
}

static void
dp_store_digit_string (line_t line, callid_t call_id, char *digit_str)
{
    const char fname[] = "dp_store_digit_string";

    if (g_dp_int.line != line && g_dp_int.call_id != call_id) {
        return;
    }

    sstrncpy(g_dp_int.gDialed, digit_str, MAX_DIALSTRING);

    DPINT_DEBUG(DEB_F_PREFIX"stored digits = %s", DEB_F_PREFIX_ARGS(DIALPLAN, fname), &g_dp_int.gDialed[0]);
}


void
dp_store_digits (line_t line, callid_t call_id, unsigned char digit)
{
    const char fname[] = "dp_store_digits";
    short len;

    if (g_dp_int.line != line && g_dp_int.call_id != call_id) {
        return;
    }

    if (digit == BKSPACE_KEY) {
        return;
    }

    g_dp_int.line = line;
    g_dp_int.call_id = call_id;

    len = (short) strlen(g_dp_int.gDialed);
    if (len >= MAX_DIALSTRING-1)
    {   // safety check to prevent going out of bounds
        CCAPP_ERROR(DEB_F_PREFIX"Unexpected dialstring [%s] (length [%d] > max [%d]) received", DEB_F_PREFIX_ARGS(DIALPLAN, fname), g_dp_int.gDialed,
            len, MAX_DIALSTRING);
        return;
    }

    g_dp_int.gDialed[len] = digit;
    g_dp_int.gDialed[len + 1] = 0;

    DPINT_DEBUG(DEB_F_PREFIX"digit = %c, dig_str = %s", DEB_F_PREFIX_ARGS(DIALPLAN, fname), digit,
                &g_dp_int.gDialed[0]);
}

/*
 *  Function: dp_check_plar_warmline()
 *
 *  Parameters: line - line number
 *                                call_id - call indentification
 *
 *  Description: Check if the line is warmline or plar. Since digit is
 *        detected, cancel the warmline timer.
 *
 *  Returns: boolean
 */
static boolean
dp_check_plar_warmline (line_t line, callid_t call_id)
{
    /* Check the timeout (timeout==0 for plar) to make sure it is not warm line.
     * If it is warm line then cancel the timer to stop sending a INV
     */
    if (g_dp_int.empty_rewrite[0] && (g_dp_int.offhook_timeout == 0)) {

        //plar
        return (TRUE);

    } else if (g_dp_int.empty_rewrite[0] && g_dp_int.offhook_timeout) {

        //warm line
        (void) cprCancelTimer(g_dp_int.dial_timer);
        memset(g_dp_int.empty_rewrite, 0, sizeof(g_dp_int.empty_rewrite));
    }
    return (FALSE);
}


/*
 *   Function: dp_delete_last_digit
 *
 *   Parameters:
 *          line_id : line identifier
 *          call_id : call identifier
 *   Description:
 *        remove last digit from dial buffer. tell UI to remove it from input box
 *
 */
void
dp_delete_last_digit (line_t line_id, callid_t call_id)
{
    int len;

    /* remove the last digit from gDialed */
    len = strlen(g_dp_int.gDialed);
    if (len) {
        g_dp_int.gDialed[len - 1] = 0;
    }

    /* tell ui to remove the digit from input box */
    ui_delete_last_digit(line_id, call_id);
}

/*
 *  Function: dp_update_keypress()
 *
 *  Parameters: line - line number
 *                                call_id - call indentification
 *                                digit - collected digit
 *
 *  Description: Dialplan interface function to pass the collected
 *                                digits. This routine will source collected digits
 *                                (including backspace) to dialplan and KPML.
 *                                Digits are passed during connected state and dialing
 *                                state.
 *
 *
 *  Returns: none
 */
static void
dp_update_keypress (line_t line, callid_t call_id, unsigned char digit)
{
    const char fname[] = "dp_update_keypress";
    lsm_states_t lsm_state;
    int skMask[MAX_SOFT_KEYS];

    DEF_DEBUG(DEB_L_C_F_PREFIX"KEY .", DEB_L_C_F_PREFIX_ARGS(DP_API, line, call_id, fname) );

    lsm_state = lsm_get_state(call_id);

    if (lsm_state == LSM_S_NONE) {
        DPINT_DEBUG(DEB_F_PREFIX"call not found", DEB_F_PREFIX_ARGS(DIALPLAN, fname));
        return;
    }

    /* If the call is in connected state, digits are DTMF key presses
     * pass this to GSM and KPML module
     */
    if (lsm_state == LSM_S_RINGOUT ||
        lsm_state == LSM_S_CONNECTED || lsm_state == LSM_S_HOLDING) {

        DPINT_DEBUG(DEB_F_PREFIX"digit received in LSM state %s",
                    DEB_F_PREFIX_ARGS(DIALPLAN, fname), lsm_state_name(lsm_state));
        cc_digit_begin(CC_SRC_GSM, g_dp_int.call_id, g_dp_int.line, digit);
        if (!kpml_update_dialed_digits(line, call_id, digit)) {
            kpml_quarantine_digits(line, call_id, digit);
        }
        return;
    }

    if (g_dp_int.line != line) {
        DPINT_DEBUG(DEB_F_PREFIX"line %d does not match dialplan line %d", DEB_F_PREFIX_ARGS(DIALPLAN, fname),
                    line, g_dp_int.line);
        return;
    }

    if (dp_check_plar_warmline(line, call_id)) {
        DPINT_DEBUG(DEB_F_PREFIX"warm line", DEB_F_PREFIX_ARGS(DIALPLAN, fname));
        return;
    }

    if (digit == 0) {
        DPINT_DEBUG(DEB_F_PREFIX"digit is 0", DEB_F_PREFIX_ARGS(DIALPLAN, fname));
        return;
    }

    ui_control_feature(line, call_id, skMask, 1, FALSE);


    /* if not in URL dialing mode pass it through kpml dialing
     * to quarantine digits
     */
    if (g_dp_int.url_dialing == FALSE) {

        if (!kpml_update_dialed_digits(line, call_id, digit)) {
            /*
             * no valid subscription case:
             * if user pressed backspace, we can approve the delete without waiting
             * for any response.
             */
            if (digit == BKSPACE_KEY) {
                dp_delete_last_digit(line, call_id);
            }

            kpml_quarantine_digits(line, call_id, digit);
        }
    } else {
        if (digit == BKSPACE_KEY) {
            /* no kpml for URL; go ahead and approve the delete */
            dp_delete_last_digit(line, call_id);
        }
    }

    /* no further processing for back space key */
    if (digit == BKSPACE_KEY) {
        return;
    }

    /* Make sure that call is in dialing state
     * if not then do not check the dial plan
     */
    switch (lsm_state) {
    case LSM_S_OFFHOOK:

        dp_check_dialplan(line, call_id, digit);
        break;

    default:
        break;
    }

}

/*
 *  Function: dp_get_dialing_call_id()
 *
 *  Parameters: void
 *
 *  Description: retun call_id of dialing call.
 *
 *  Returns: none
 */
static callid_t
dp_get_dialing_call_id (void)
{
    return (g_dp_int.call_id);
}

/*
 *  Function: dp_update_key_string()
 *
 *  Parameters: line - line number
 *                                call_id - call indentification
 *                                digitstr - Pointer to collected digits
 *
 *  Description: Dialplan interface function to pass the collected
 *                                digits. This routine will source collected digits
 *                                (including backspace) to dialplan and KPML.
 *                                Digits are passed during connected state and dialing
 *                                state.
 *
 *  Returns: none
 */
static void
dp_update_key_string (line_t line, callid_t call_id, char *digits)
{
    short indx = 0;

    /* Get dialing call id if the call id is null */

    if (call_id == CC_NO_CALL_ID) {

        call_id = dp_get_dialing_call_id();
    }

    while (digits[indx]) {
        dp_update_keypress(line, call_id, digits[indx++]);
    }
}

/*
 *  Function: dp_dial_timeout_event()
 *
 *  Parameters: timer - timer pointer
 *
 *  Description: Handles dial timeout for dialplan
 *
 *  Returns: none
 */
static void
dp_dial_timeout_event (void *data)
{
    const char fname[] = "dp_dial_timeout_event";

    DPINT_DEBUG(DEB_F_PREFIX"line=%d call_id=%d dialed digits=%s", DEB_F_PREFIX_ARGS(DIALPLAN, fname),
                g_dp_int.line, g_dp_int.call_id, g_dp_int.gDialed);


    /* if there is a timeout without dialing digit
     * then do the onhook
     */
    if (g_dp_int.empty_rewrite[0] != NUL) {

        kpml_flush_quarantine_buffer(g_dp_int.line, g_dp_int.call_id);

        cc_dialstring(CC_SRC_GSM,
                      g_dp_int.call_id, g_dp_int.line, g_dp_int.empty_rewrite);
    } else if ((g_dp_int.gDialed[0] == NUL) &&
               (g_dp_int.gDialplanDone == FALSE)) {

        /* Go onhook if no digits are dialed. Dialplandone is checked to see
         * if the timeout happens during KPML digit collection
         */
        cc_onhook(CC_SRC_GSM, g_dp_int.call_id, g_dp_int.line, FALSE);
    } else {
        /* KPML subscription will not be rejected in this case.
         * even though it is timeout which triggered invite
         * remote party may have more digits to collect through
         * Kpml
         */
        kpml_flush_quarantine_buffer(g_dp_int.line, g_dp_int.call_id);
        cc_dialstring(CC_SRC_GSM,
                      g_dp_int.call_id, g_dp_int.line, g_dp_int.gDialed);
    }
}

/*
 *  Function: dp_cancel_offhook_timer
 *
 *  Parameters:
 *
 *  Description:
 *     called to cancel offhook timer.

 *  Returns: none
 */
void dp_cancel_offhook_timer (void)
{
    if ((g_dp_int.gTimerType == DP_OFFHOOK_TIMER) && (g_dp_int.dial_timer)) {
        (void) cprCancelTimer(g_dp_int.dial_timer);
    }
}

/*
 *  Function: dp_restart_dial_timer()
 *
 *  Parameters: line - line number
 *              call_id - call indentification
 *              timeout - timeout value for the dial timer
 *
 *  Description: Start dial timer
 *
 *  Returns: none
 */
static void
dp_restart_dial_timer (line_t line, callid_t call_id, int timeout)
{
    const char fname[] = "dp_restart_dial_timer";

    DPINT_DEBUG(DEB_F_PREFIX"line=%d call_id=%d timeout=%u", DEB_F_PREFIX_ARGS(DIALPLAN, fname), line,
                call_id, timeout);

    g_dp_int.timer_info.index.line = line;
    g_dp_int.timer_info.index.call_id = call_id;

    if (g_dp_int.dial_timer) {
        (void) cprCancelTimer(g_dp_int.dial_timer);

        (void) cprStartTimer(g_dp_int.dial_timer, timeout,
                             &g_dp_int.timer_info);

    }
}

/*
 *  Function: dp_check_dialplan()
 *
 *  Parameters: line - line number
 *                                call_id - call indentification
 *                                cursor - indicates number digits in the dial string
 *                                strDigs - collected digit string.
 *
 *  Description: Check dial plan with collected digits.
 *
 *  Returns: None
 */
static void
dp_check_dialplan (line_t line, callid_t call_id, unsigned char digit)
{
    const char fname[] = "dp_check_dialplan";
    int timeout = DIAL_TIMEOUT;
    DialMatchAction action;
    vcm_tones_t tone;
    lsm_states_t lsm_state;

    /* if the dialing is done then don't
     * do anything
     */
    if (g_dp_int.gDialplanDone) {
        DPINT_DEBUG(DEB_F_PREFIX"Dialplan Match Completed: line=%d call_id=%d digits=%d",
                    DEB_F_PREFIX_ARGS(DIALPLAN, fname), line, call_id, digit);
        return;
    }

    DPINT_DEBUG(DEB_F_PREFIX"line=%d call_id=%d digits=%s", DEB_F_PREFIX_ARGS(DIALPLAN, fname), line,
                call_id, &g_dp_int.gDialed[0]);

    dp_store_digits(line, call_id, digit);

    /* get current digit in GSM format */
    if (digit == '*') {
        digit = 0x0E;
    } else if (digit == '#') {
        digit = 0x0F;
    } else {
        digit = digit - '0';
    }

    /* see if we match any dial plans */
    action =
        MatchDialTemplate(g_dp_int.gDialed, line, &timeout, NULL, 0, NULL,
                          &tone);

    switch (action) {

    case DIAL_FULLPATTERN:
        DPINT_DEBUG(DEB_F_PREFIX"Full pattern match", DEB_F_PREFIX_ARGS(DIALPLAN, fname));

        if (timeout <= 0) {
            cc_dialstring(CC_SRC_GSM, g_dp_int.call_id, g_dp_int.line,
                          g_dp_int.gDialed);
            /* flush collected digits. Invite has been sent */
            kpml_flush_quarantine_buffer(line, call_id);

            (void) cprCancelTimer(g_dp_int.dial_timer);

            g_dp_int.gDialplanDone = TRUE;
            return;
        }

        cc_digit_begin(CC_SRC_GSM, call_id, line, digit);
        break;

    case DIAL_IMMEDIATELY:
        DPINT_DEBUG(DEB_F_PREFIX"Dial immediately", DEB_F_PREFIX_ARGS(DIALPLAN, fname));
        /*
         * The user pressed the # key and the phone should dial immediately
         */
        (void) cprCancelTimer(g_dp_int.dial_timer);
        g_dp_int.gDialplanDone = TRUE;

        cc_dialstring(CC_SRC_GSM, g_dp_int.call_id, g_dp_int.line,
                      g_dp_int.gDialed);

        //kpml_set_subscription_reject(line, call_id);

        kpml_flush_quarantine_buffer(line, call_id);

        return;

    case DIAL_GIVETONE:
        DPINT_DEBUG(DEB_F_PREFIX"Give tone", DEB_F_PREFIX_ARGS(DIALPLAN, fname));

        /* we need new dial tone */
        lsm_state = lsm_get_state(call_id);

        if (lsm_state == LSM_S_NONE) {
            DPINT_DEBUG(DEB_F_PREFIX"call not found", DEB_F_PREFIX_ARGS(DIALPLAN, fname));
            return;
        }

        (void)cc_call_action(call_id, LSM_NO_LINE, CC_ACTION_STOP_TONE, NULL);
        vcmToneStart(tone, FALSE, CREATE_CALL_HANDLE(line, call_id), 0 /* group_id */,
                       0/* stream_id */, VCM_PLAY_TONE_TO_EAR);
        break;

    default:
        DPINT_DEBUG(DEB_F_PREFIX"No match", DEB_F_PREFIX_ARGS(DIALPLAN, fname));
        cc_digit_begin(CC_SRC_GSM, call_id, line, digit);
        break;
    }

    dp_restart_dial_timer(line, call_id, timeout * 1000);
    g_dp_int.gTimerType = DP_INTERDIGIT_TIMER;

    return;
}


/*
 *  Function: dp_get_kpml_state()
 *
 *  Parameters:
 *
 *  Description: Function called to check if the KPML is enabled. If the KPML is
 *    enabled then function returns true. But under certain conditions like redial
 *    or speed dial (dial immediately) dialing is completed and phone does not expect
 *    any KPML subscriptions. So do not collect any more digits in this and UI state
 *    will be set to proceed and frozen. To facilitate this allo_proceed variable is
 *    added, if this is set to TRUE then function returns false (as if KPML is disabled).
 *    As mentioned earlier this is called for redial and dial immediate functions.
 *    Allow_proceed value is reset in this function, and dp_clear_dialing_data() function
 *
 *  Returns: TRUE to indicate, more digits has to be collected
 *           FALSE to indicate no more digits to collect.
 */
boolean
dp_get_kpml_state (void)
{
    if (g_dp_int.allow_proceed) {

        g_dp_int.allow_proceed = FALSE;
        return (FALSE);

    } else {

        return (kpml_get_state());
    }
}

/*
 *  Function: dp_get_gdialed_digits()
 *
 *  Parameters:
 *
 *  Description: Return dialed digits to the caller.
 *
 *
 *  Returns: none
 */
char *
dp_get_gdialed_digits (void)
{
    const char fname[] = "dp_get_gdialed_digits";

    DPINT_DEBUG(DEB_F_PREFIX"Dialed digits:%s", DEB_F_PREFIX_ARGS(DIALPLAN, fname), g_dp_int.gDialed);

    /* Digits are copied to Redial buffer after 180 is received
     * after dp_update (). Afterthat gDialed buffer is cleared. So the dialed
     * value is moved into gRedialed buffer
     */

    if (g_dp_int.gDialed[0] != NUL) {

        return (&g_dp_int.gDialed[0]);
    }

    return (&g_dp_int.gReDialed[0]);
}

/*
 *  Function: dp_get_redial_line()
 *
 *  Parameters: void
 *
 *  Description: returns redial line.
 *
 *
 *  Returns: none
 */
line_t
dp_get_redial_line (void)
{
    return (g_dp_int.gRedialLine);
}

static void
dp_do_redial (line_t line, callid_t call_id)
{
    const char fname[] = "dp_do_redial";

    DPINT_DEBUG(DEB_F_PREFIX"line=%d call_id=%d", DEB_F_PREFIX_ARGS(DIALPLAN, fname), line, call_id);

    if (line == 0) {
        line = dp_get_redial_line();
    }
    //CSCsz63263, use prime line instead if last redialed line is unregistered
    if(ccsip_is_line_registered(line) == FALSE) {
         DPINT_DEBUG(DEB_F_PREFIX"line %d unregistered, use line %d instead", DEB_F_PREFIX_ARGS(DIALPLAN, fname), line,
        		 PRIME_LINE_ID);
         line = PRIME_LINE_ID;
         if( ccsip_is_line_registered(line) == FALSE) {
            DPINT_DEBUG(DEB_F_PREFIX" prime line %d unregistered", DEB_F_PREFIX_ARGS(DIALPLAN, fname), line);
            return;
       }
     }
    if (g_dp_int.gReDialed[0] == NUL) {
        DPINT_DEBUG(DEB_F_PREFIX"NO DIAL STRING line=%d call_id=%d", DEB_F_PREFIX_ARGS(DIALPLAN, fname), line,
                    call_id);

        return;
    }

    sstrncpy(g_dp_int.gDialed, g_dp_int.gReDialed, MAX_DIALSTRING);

    g_dp_int.line = line;

    g_dp_int.call_id = call_id;

    g_dp_int.allow_proceed = TRUE;

    /*
     * If we are doing redial, then we won't be entering any more digits
     */
    g_dp_int.gDialplanDone = TRUE;

    (void) cprCancelTimer(g_dp_int.dial_timer);

    kpml_set_subscription_reject(line, call_id);

    kpml_flush_quarantine_buffer(line, call_id);

    cc_dialstring(CC_SRC_GSM, call_id, line, &g_dp_int.gReDialed[0]);
}

/*
 *  Function: dp_start_dialing()
 *
 *  Parameters: line - line number
 *                                call_id - call indentification
 *
 *  Description: Start dialing and initilize dialing data.
 *
 *  Returns: none
 */
static void
dp_init_dialing_data (line_t line, callid_t call_id)
{
    const char fname[] = "dp_init_dialing_data";

    DPINT_DEBUG(DEB_F_PREFIX"line=%d call_id=%d", DEB_F_PREFIX_ARGS(DIALPLAN, fname), line, call_id);

    g_dp_int.call_id = call_id;
    g_dp_int.line = line;
    g_dp_int.gDialplanDone = FALSE;
    g_dp_int.url_dialing = FALSE;
    g_dp_int.gTimerType = DP_NONE_TIMER;

    memset(g_dp_int.gDialed, 0, sizeof(g_dp_int.gDialed));

    /* get offhook timeout */
    g_dp_int.offhook_timeout = DIAL_TIMEOUT;

    config_get_value(CFGID_OFFHOOK_TO_FIRST_DIGIT_TIMER,
                     &g_dp_int.offhook_timeout,
                     sizeof(g_dp_int.offhook_timeout));

    /* Flush any collected KPML digits on this line */
    kpml_flush_quarantine_buffer(line, call_id);
}

/*
 *  Function: dp_clear_dialing_data()
 *
 *  Parameters: line - line number
 *                                call_id - call indentification
 *
 *  Description: This routine is called to stop and clean up dialing data.
 *
 *
 *  Returns: none
 */
static void
dp_clear_dialing_data (line_t line, callid_t call_id)
{
    const char fname[] = "dp_clear_dialing_data";

    DPINT_DEBUG(DEB_F_PREFIX"line=%d call_id=%d", DEB_F_PREFIX_ARGS(DIALPLAN, fname), line, call_id);

    g_dp_int.call_id = 0;
    g_dp_int.line = 0;
    g_dp_int.gDialplanDone = FALSE;
    g_dp_int.allow_proceed = FALSE;

    memset(g_dp_int.gDialed, 0, sizeof(g_dp_int.gDialed));
    memset(g_dp_int.empty_rewrite, 0, sizeof(g_dp_int.empty_rewrite));

    /* Stop interdigit timer */
    (void) cprCancelTimer(g_dp_int.dial_timer);

    /* Flush any collected KPML digits on this line */
    //kpml_flush_quarantine_buffer (line, call_id);
}

/*
 *  Function: dp_onhook
 *
 *  Parameters: none
 *
 *  Description: Function called to indicate onhook.
 *
 *  Returns: none.
 */
static void
dp_onhook (line_t line, callid_t call_id)
{
    if ((g_dp_int.line == line) && (g_dp_int.call_id == call_id)) {
        dp_clear_dialing_data(line, call_id);
    }
}


/*
 *  Function: dp_check_plar_line
 *
 *  Parameters: line
 *
 *  Description: Function returns true if the call and line is plar.
 *
 *  Returns: TRUE - if plar configured and dialing completed.
 */
boolean
dp_check_for_plar_line (line_t line)
{
    static char fname[] = "dp_check_for_plar_line";
    DialMatchAction action;
    char empty_rewrite[MAX_DIALSTRING];
    char empty[1];
    int timeout = 0;

    empty[0] = '\0';            // This empty (i.e. "") pattern.
    //Check dialplan to generate empty invite
    action = MatchDialTemplate(empty, line, (int *) &timeout,
                               empty_rewrite, sizeof(empty_rewrite),
                               NULL, NULL);

    if (action == DIAL_FULLMATCH) {

        /* Plar line is only when timeout == 0 or else it is warm line
         */
        if (timeout <= 0) {

            DPINT_DEBUG(DEB_F_PREFIX"line=%d PLAR line", DEB_F_PREFIX_ARGS(DIALPLAN, fname), line);
            return (TRUE);

        }
    }
    return (FALSE);
}

/*
 *  Function: dp_check_and_handle_plar_dialing
 *
 *  Parameters: call_id and line
 *
 *  Description: Responsible for checking if the line is plar line and
 *         if so it will dial plar DN.
 *
 *  Returns: TRUE - if plar configured.
 */
boolean dp_check_and_handle_plar_dialing (line_t line, callid_t call_id)
{
    DialMatchAction action;
    char empty[1];
    int timeout = 0;

    timeout = g_dp_int.offhook_timeout;

    empty[0] = '\0';            // This empty (i.e. "") pattern.
    //Check dialplan to generate empty invite
    action = MatchDialTemplate(empty, line, (int *) &timeout,
                               g_dp_int.empty_rewrite,
                               sizeof(g_dp_int.empty_rewrite), NULL, NULL);


    if (timeout != (int) g_dp_int.offhook_timeout) {
        // Dialplan has timeout value convert that in to msec.
        g_dp_int.offhook_timeout = timeout * 1000;
    }
    if (action == DIAL_FULLMATCH) {
        /* This is either hotline (i.e. plar) or warmline. Timeout is
         * what tells us the difference.
         */
        if (g_dp_int.offhook_timeout <= 0) {

            g_dp_int.gDialplanDone = TRUE;

            g_dp_int.allow_proceed = FALSE;

            kpml_set_subscription_reject(g_dp_int.line, g_dp_int.call_id);

            kpml_flush_quarantine_buffer(line, call_id);

            cc_dialstring(CC_SRC_GSM, g_dp_int.call_id, g_dp_int.line,
                          g_dp_int.empty_rewrite);

            // copy rewrite into dialed digits so that it will be logged in the placed calls history.
            memcpy(g_dp_int.gDialed, g_dp_int.empty_rewrite, MAX_DIALSTRING);
            return (TRUE);

        }
    }

    return(FALSE);
}
/*
 *  Function: dp_offhook
 *
 *  Parameters: none
 *
 *  Description: Function called to indicate offhook.
 *
 *  Returns: TRUE - if plar configured and dialing completed.
 */
boolean dp_offhook (line_t line, callid_t call_id)
{
    const char fname[] = "dp_offhook";

    DPINT_DEBUG(DEB_F_PREFIX"line=%d call_id=%d", DEB_F_PREFIX_ARGS(DIALPLAN, fname), line, call_id);

    /*
     * If this line and call_id is same that means
     * dialing has been initialized already.  In addition, if
     * gDialplanDone is true, there won't be any further digit entry
     * so don't start the dial timer.
     * Example case where remote-cc or speedial initates the call
     * then caller will initialize the dp_int data structure. That
     * also initilizes gDialpalnDone to True. So Do not restart the
     * timer or process anything else in these cases.
     */
    if ((g_dp_int.line == line) && (g_dp_int.call_id == call_id)) {

        if (g_dp_int.gDialplanDone == FALSE) {
            dp_restart_dial_timer(line, call_id, g_dp_int.offhook_timeout);
            g_dp_int.gTimerType = DP_OFFHOOK_TIMER;
        }

        return (FALSE);
    }

    //Start offhook timer
    dp_init_dialing_data(line, call_id);

    if (dp_check_and_handle_plar_dialing(line, call_id) == TRUE) {
        return(TRUE);
    }

    /*
     * Only start the dial timer if dialing is not yet completed.
     */
    if (g_dp_int.gDialplanDone == FALSE) {
        dp_restart_dial_timer(line, call_id, g_dp_int.offhook_timeout);
        g_dp_int.gTimerType = DP_OFFHOOK_TIMER;
    }

    return (FALSE);
}

/*
 *  Function: dp_dial_immediate
 *
 *  Parameters:
 *        line_id - line number
 *        call_id - call indentification
 *
 *  Description:
 *     called when the client wants DP module to stop matching and
 *     send the dialstring. This used for example when User pressed Dial Softkey.
 *
 *  Returns: none
 */
static void
dp_dial_immediate (line_t line, callid_t call_id, boolean collect_more,
                   char *digit_str, char *global_call_id,
                   monitor_mode_t monitor_mode)
{
    const char fname[] = "dp_dial_immediate";

    if (g_dp_int.line != line || g_dp_int.call_id != call_id) {
        return;
    }

    DPINT_DEBUG(DEB_F_PREFIX"line=%d call_id=%d dialed digits=%s",
                DEB_F_PREFIX_ARGS(DIALPLAN, fname), line, call_id, g_dp_int.gDialed);

    /* Do not dial anything else even if UI asks to do so, if the line is PLAR */
    if (dp_check_and_handle_plar_dialing(line, call_id) == TRUE) {
        return;
    }

    if (g_dp_int.dial_timer) {
        (void) cprCancelTimer(g_dp_int.dial_timer);
    }


    if (g_dp_int.gDialplanDone) {

        // if used with CCM, we should allow user to continue to input more digits
        // after they've entered partial destination number onhook then press dial
        if (sip_regmgr_get_cc_mode(line) == REG_MODE_CCM) {
            return;
        }
        // KPML mode and user pressed DIAL softkey. So send 0x82 digit to KPML
        if (!kpml_update_dialed_digits(line, call_id, (char)DIAL_KEY)) {

            kpml_quarantine_digits(line, call_id, (char)DIAL_KEY);
        }
        return;
    }

    /* CTI applications can do a initcallreq without providing the dialstring
     * In this case generate just the offhook event to GSM
     */

    if (digit_str[0] == 0 && global_call_id[0] != 0) {

        cc_offhook_ext(CC_SRC_GSM, call_id, line,
                       global_call_id, monitor_mode);
        return;
    }

    g_dp_int.gDialplanDone = TRUE;

    if (digit_str[0] != 0) {
        sstrncpy(g_dp_int.gDialed, digit_str, MAX_DIALSTRING);
    }

    /* This case should not happen, as dial softkey is displayed only after the
     * 1st digit. dp_dial_immediate function is called for dial softkey invocation.
     */
    if (g_dp_int.gDialed[0] == 0) {
        return;

    } else {
        g_dp_int.line = line;
        g_dp_int.call_id = call_id;

        kpml_flush_quarantine_buffer(line, call_id);

        cc_dialstring_ext(CC_SRC_GSM, call_id, line, g_dp_int.gDialed,
                          global_call_id, monitor_mode);
    }

    if (collect_more == FALSE) {
        /*
         * Since no more digits are collected,
         * allow proceed event to go through UI.
         */
        g_dp_int.allow_proceed = TRUE;

        kpml_set_subscription_reject(line, call_id);

        kpml_flush_quarantine_buffer(line, call_id);
    }

}

/*
 *  Function: dp_update
 *
 *  Parameters: none
 *
 *  Description: Function called to indicate alerting.
 *
 *  Returns: none
 */
static void
dp_update (line_t line, callid_t call_id, string_t called_num)
{
    uint32_t          line_feature = 0;

    config_get_line_value(CFGID_LINE_FEATURE, &line_feature,
                          sizeof(line_feature), line);

    /* Do not store PLAR redial string.
     */

    if (g_dp_int.gDialed[0] &&
        (strcmp(g_dp_int.gDialed, CISCO_PLAR_STRING) != 0) &&
        (strncmp(g_dp_int.gDialed, CISCO_BLFPICKUP_STRING, (sizeof(CISCO_BLFPICKUP_STRING) - 1)) != 0)) {

        sstrncpy(g_dp_int.gReDialed, g_dp_int.gDialed, MAX_DIALSTRING);
        g_dp_int.gRedialLine = line;
    }

    // fix bug CSCsm56180
    if ((g_dp_int.line == line) && (g_dp_int.call_id == call_id)) {
        dp_clear_dialing_data(line, call_id);
        /* Clear kpml data and collected digits */
        kpml_flush_quarantine_buffer(line, call_id);
    }
}


/*
 *  Function: dp_reset()
 *
 *  Parameters: none
 *
 *  Description: Called during soft reset
 *
 *  Returns: none
 */
void
dp_reset (void)
{
    DPINT_DEBUG(DEB_F_PREFIX"Reset dp_int module", DEB_F_PREFIX_ARGS(DIALPLAN, "dp_reset"));
    /* cleanup redial buffer */
    memset(g_dp_int.gReDialed, 0, sizeof(g_dp_int.gReDialed));
}

/*
 *  Function: dp_init()
 *
 *  Parameters: gsmMsgQueue - the msg queue for the GSM task. Used
 *                  to tell CPR where to send the timer expiration
 *                  msgs for the dialplan timer.
 *
 *  Description: Initialize dialplan and KPML
 *
 *
 *  Returns: none
 */
void
dp_init (void *gsmMsgQueue)
{

    g_dp_int.dial_timer =
        cprCreateTimer("dial_timeout", GSM_DIAL_TIMEOUT_TIMER, TIMER_EXPIRATION,
                       (cprMsgQueue_t) gsmMsgQueue);

}

/*
 *  Function: dp_shutdown()
 *
 *  Parameters: none
 *
 *  Description: shutdown dialplan and cleanup memory
 *
 *
 *  Returns: none
 */
void
dp_shutdown (void)
{
    if (g_dp_int.dial_timer != NULL) {
        (void)cprDestroyTimer(g_dp_int.dial_timer);
        g_dp_int.dial_timer = NULL;
    }
    FreeDialTemplates();
}

/*
 * Function: dp_init_template
 *
 * Parameters: maxSize - maximum size of the dialplan
 *
 * Description: initialize dialplan template from downloaded file
 *              This is triggered by Adapter module when it triggers download of DP.
 *
 */
int dp_init_template(const char * dial_plan_string, int length) {
    static const char fname[] = "dp_init_template";
    char *TemplateData = (char *) &dpLoadArea;

    DPINT_DEBUG(DEB_F_PREFIX"Reading Dialplan string.  Length=[%d]", DEB_F_PREFIX_ARGS(DIALPLAN, fname), length);


    /* Clear existing dialplan templates before building a new one */
    FreeDialTemplates();
    memset(dpLoadArea, 0, sizeof(dpLoadArea));

    //Copy over data
    memcpy (dpLoadArea, dial_plan_string, length);

    if (length == 0 || ParseDialTemplate(TemplateData) == FALSE) {

        if (kpml_get_state()) {

            /**
             * There is a error in the parsing so set the default dialplan when KPML is enabled
             */
            DPINT_DEBUG(DEB_F_PREFIX"Loading Default Dialplan with KPML enabled",
                        DEB_F_PREFIX_ARGS(DIALPLAN, fname));
            memcpy(dpLoadArea, (const char *) KPML_DEFAULT_DIALPLAN,
                   sizeof(KPML_DEFAULT_DIALPLAN));
            length = sizeof(KPML_DEFAULT_DIALPLAN);
            TemplateData[length] = '\00';
            (void) ParseDialTemplate(TemplateData);
        } else {
            /**
             * There is a error either in parsing so release any parsed dialplan structures
             */
            DPINT_DEBUG(DEB_F_PREFIX"Loading Default Dialplan with KPML disabled",
                        DEB_F_PREFIX_ARGS(DIALPLAN, fname));
            FreeDialTemplates();
        }


        return (-1);
    }

    DPINT_DEBUG(DEB_F_PREFIX"Successfully Parsed Dialplan.", DEB_F_PREFIX_ARGS(DIALPLAN, fname));
    return (0);
}

char *
dp_get_msg_string (uint32_t cmd)
{
    switch (cmd) {

    case DP_MSG_INIT_DIALING:
        return ("DP_MSG_INIT_DIALING");

    case DP_MSG_DIGIT_TIMER:
        return ("DP_MSG_DIGIT_TIMER");

    case DP_MSG_DIGIT_STR:
        return ("DP_MSG_DIGIT_STR");

    case DP_MSG_STORE_DIGIT:
        return ("DP_MSG_STORE_DIGIT");

    case DP_MSG_DIGIT:
        return ("DP_MSG_DIGIT");

    case DP_MSG_DIAL_IMMEDIATE:
        return ("DP_MSG_DIAL_IMMEDIATE");

    case DP_MSG_REDIAL:
        return ("DP_MSG_REDIAL");

    case DP_MSG_ONHOOK:
        return ("DP_MSG_ONHOOK");

    case DP_MSG_OFFHOOK:
        return ("DP_MSG_OFFHOOK");

    case DP_MSG_CANCEL_OFFHOOK_TIMER:
        return ("DP_MSG_CANCEL_OFFHOOK_TIMER");


    case DP_MSG_UPDATE:
        return ("DP_MSG_UPDATE");

    default:
        return ("DP_MSG_UNKNOWN_CMD");
    }
}

void
dp_process_msg (uint32_t cmd, void *msg)
{
    static const char fname[] = "dp_process_msg";
    dp_int_t *dp_int_msg = (dp_int_t *) msg;

    DPINT_DEBUG(DEB_F_PREFIX"cmd= %s", DEB_F_PREFIX_ARGS(DIALPLAN, fname), dp_get_msg_string(cmd));

    switch (cmd) {

    case DP_MSG_INIT_DIALING:
        dp_init_dialing_data(dp_int_msg->line, dp_int_msg->call_id);
        break;

    case DP_MSG_DIGIT_TIMER:
        dp_dial_timeout_event(dp_int_msg->tmr_ptr);
        break;

    case DP_MSG_DIGIT_STR:
        dp_update_key_string(dp_int_msg->line, dp_int_msg->call_id,
                             &(dp_int_msg->digit_str[0]));
        break;

    case DP_MSG_STORE_DIGIT:
        dp_store_digit_string(dp_int_msg->line, dp_int_msg->call_id,
                         &(dp_int_msg->digit_str[0]));
        break;

    case DP_MSG_DIGIT:
        dp_update_keypress(dp_int_msg->line, dp_int_msg->call_id,
                           dp_int_msg->digit);
        break;

    case DP_MSG_DIAL_IMMEDIATE:
        dp_dial_immediate(dp_int_msg->line, dp_int_msg->call_id,
                          dp_int_msg->collect_more, &(dp_int_msg->digit_str[0]),
                          &(dp_int_msg->global_call_id[0]),
                          dp_int_msg->monitor_mode);
        break;

    case DP_MSG_REDIAL:
        dp_do_redial(dp_int_msg->line, dp_int_msg->call_id);
        break;

    case DP_MSG_ONHOOK:
        dp_onhook(dp_int_msg->line, dp_int_msg->call_id);
        break;

    case DP_MSG_OFFHOOK:
        (void) dp_offhook(dp_int_msg->line, dp_int_msg->call_id);
        break;

    case DP_MSG_CANCEL_OFFHOOK_TIMER:
        dp_cancel_offhook_timer();
        break;

    case DP_MSG_UPDATE:
        dp_update(dp_int_msg->line, dp_int_msg->call_id,
                  &(dp_int_msg->digit_str[0]));
        break;

    default:
        break;
    }
}
