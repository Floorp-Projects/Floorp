/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "cpr_types.h"
#include "cpr_memory.h"
#include "cpr_timers.h"
#include "cpr_locks.h"
#include "phntask.h"
#include "ccsip_subsmanager.h"
#include "singly_link_list.h"
#include "ccapi.h"
#include "subapi.h"
#include "fsm.h"
#include "uiapi.h"
#include "rtp_defs.h"
#include "lsm.h"
#include "lsm_private.h"
#include "kpmlmap.h"
#include "dialplanint.h"
#include "uiapi.h"
#include "debug.h"
#include "phone_debug.h"
#include "kpml_common_util.h"
#include "gsm.h"

cc_int32_t KpmlDebug = 0;
static sll_handle_t s_kpml_list = NULL;
static int s_kpml_config;

static void kpml_generate_notify(kpml_data_t *kpml_data, boolean no_body,
                                 unsigned int resp_code, char *resp_text);
static void kpml_generate_subscribe_response(kpml_data_t *kpml_data,
                                             int resp_code);
static void kpmlmap_show(void);
void kpml_inter_digit_timer_callback(void *kpml_key_p);
void kpml_subscription_timer_callback(void *kpml_key);
static uint32_t g_kpml_id = CC_NO_CALL_ID;
static cprMutex_t kpml_mutex;

/*
 *  Function: kpml_get_state()
 *
 *  Parameters:
 *
 *  Description: Function called to check if the KPML is configured for signaling.
 *
 *
 *  Returns: none
 */
boolean
kpml_get_state (void)
{
    config_get_value(CFGID_KPML_ENABLED, &s_kpml_config, sizeof(s_kpml_config));

    if ((s_kpml_config == KPML_NONE) || (s_kpml_config == KPML_DTMF_ONLY)) {
        return FALSE;
    }
    return TRUE;
}

/*
 *  Function: kpml_get_config_value()
 *
 *  Parameters:
 *
 *  Description: Function called to check if the KPML is enabled.
 *
 *
 *  Returns: none
 */
kpml_config_e
kpml_get_config_value (void)
{
    int kpml_config = KPML_NONE;
    config_get_value(CFGID_KPML_ENABLED, &kpml_config, sizeof(kpml_config));

    return (kpml_config_e) kpml_config;
}

/*
 *  Function: kpml_get_new_data()
 *
 *  Parameters: none
 *
 *  Description: Get a new kpml data
 *
 *  Returns: kpml_data_t
 */
static kpml_data_t *
kpml_get_new_data (void)
{
    kpml_data_t *kpml_mem;

    kpml_mem = (kpml_data_t *) cpr_malloc(sizeof(kpml_data_t));
    if (kpml_mem == NULL)
    {
        return (NULL);
    }

    memset(kpml_mem, 0, sizeof(kpml_data_t));
	kpml_mem->kpml_id = ++g_kpml_id;

    return (kpml_mem);
}

/*
 *  Function: kpml_release_data()
 *
 *  Parameters: kpml_data_t
 *
 *  Description: release kpml data
 *
 *  Returns: None
 */

static void
kpml_release_data (kpml_data_t * kpml_data)
{
    cpr_free(kpml_data);
}

/*
 *  Function: kpml_create_sm_key()
 *
 *  Parameters: kpml_key_t - key information
 *              line - line number of the call
 *              call_id - call indentifier
 *
 *  Description: This routine fills line and callid in the key structures.
 *
 *  Returns: None
 */
static void
kpml_create_sm_key (kpml_key_t *key_p, line_t line, callid_t call_id,
                    void *tmr_ptr)
{
    static const char fname[] = "kpml_create_sm_key";

    KPML_DEBUG(DEB_L_C_F_PREFIX" timer=0x%0x\n",
			   DEB_L_C_F_PREFIX_ARGS(KPML_INFO, line, call_id, fname), tmr_ptr);

    key_p->line = line;
    key_p->call_id = call_id;
    key_p->timer = tmr_ptr;
}

/*
 *  Function: kpml_match_line_call_id()
 *
 *  Parameters: kpml_data - Kpml subscription data
 *              kpml_key_t - key information
 *
 *  Description: Callback function provided to link list
 *              to search for a particular line and call_id
 *
 *  Returns: ss_match_e
 */
static sll_match_e
kpml_match_line_call_id (kpml_data_t * kpml_data_p, kpml_key_t * key_p)
{
    static const char fname[] = "kpml_match_line_call_id";

    if ((kpml_data_p->call_id == key_p->call_id) &&
        (kpml_data_p->line == key_p->line)) {

        KPML_DEBUG(DEB_L_C_F_PREFIX"Match Found.\n",
                   DEB_L_C_F_PREFIX_ARGS(KPML_INFO, key_p->line, key_p->call_id, fname));
        return SLL_MATCH_FOUND;
    }

    return SLL_MATCH_NOT_FOUND;
}

/*
 *  Function: kpml_data_present_for_subid()
 *
 *  Parameters: sub_id_t - sub id sent int he message
 *
 *  Description: Callback function provided to link list
 *              to see if there kpml_data that matches a sub_id
 *
 *  Returns: kpml_data_t * - Return kpml_data if a match is found
 */
static kpml_data_t *
kpml_data_for_subid(sub_id_t sub_id)
{
    kpml_data_t *kpml_data;

    kpml_data = (kpml_data_t *) sll_next(s_kpml_list, NULL);

    while (kpml_data != NULL) {
        if (kpml_data->sub_id == sub_id) {
            return kpml_data;
        }
        kpml_data = (kpml_data_t *) sll_next(s_kpml_list, kpml_data);
    }
    return NULL;
}

/*
 *  Function: kpml_start_timer()
 *
 *  Parameters: line_num - line id on which timer to start
 *              callId   -  Callid of the call on which timer
 *                 to start
 *              timer_ptr - User allocated pointer
 *              duration - in seconds
 *              timer_callback- callback function once timer
 *                    expires
 *
 *  Description: start any given timer with duration in seconds
 *
 *  Returns: None
 */
static void
kpml_start_timer (line_t line, callid_t callId,
                  void *timer_ptr, unsigned int duration,
                  uint32_t kpml_id)
{
    if (timer_ptr) {

        (void) cprCancelTimer(timer_ptr);

        (void) cprStartTimer(timer_ptr, duration, (void *)(long)kpml_id);
    }
}

/*
 *  Function: kpml_stop_timer()
 *
 *  Parameters: timer_ptr - pointer to timer data to stop
 *
 *  Description: stop any specified timer
 *
 *  Returns: None
 */
static void
kpml_stop_timer (void *timer_ptr)
{
    if (timer_ptr) {
        (void) cprCancelTimer(timer_ptr);

        (void) cprDestroyTimer(timer_ptr);
    }
}

/*
 *  Function: kpml_clear_timers()
 *
 *  Parameters: kpml_data - pointer to kpml data
 *
 *  Description: stop and delete all kpml timers
 *
 *  Returns: None
 */
static void
kpml_clear_timers (kpml_data_t *kpml_data)
{
    static const char fname[] = "kpml_clear_timers";

    KPML_DEBUG(DEB_L_C_F_PREFIX"Release kpml timers.\n",
                DEB_L_C_F_PREFIX_ARGS(KPML_INFO, kpml_data->line, kpml_data->call_id, fname));

    kpml_stop_timer(kpml_data->inter_digit_timer);
    kpml_data->inter_digit_timer = NULL;

    kpml_stop_timer(kpml_data->critical_timer);
    kpml_data->critical_timer = NULL;

    kpml_stop_timer(kpml_data->extra_digit_timer);
    kpml_data->extra_digit_timer = NULL;

    kpml_stop_timer(kpml_data->sub_timer);
    kpml_data->sub_timer = NULL;
}

/*
 *  Function: kpml_start_timers()
 *
 *  Parameters: kpml_data_t - subscription data
 *              kpml_data_t - kpml data
 *
 *  Description: start different kpml related timers. Even though all the
 *              KPML timers are allocated, only interdigit timer will be in use
 *
 *  Returns: None
 */
static void
kpml_start_timers (kpml_data_t *kpml_data)
{
    static const char *fname ="kpml_start_timers";

    kpml_data->inter_digit_timer =
        cprCreateTimer("Interdigit timer", GSM_KPML_INTER_DIGIT_TIMER,
                       TIMER_EXPIRATION, gsm_msgq);

    kpml_data->critical_timer =
        cprCreateTimer("Criticaldigit timer", GSM_KPML_CRITICAL_DIGIT_TIMER,
                       TIMER_EXPIRATION, gsm_msgq);

    kpml_data->extra_digit_timer =
        cprCreateTimer("Extradigit timer", GSM_KPML_EXTRA_DIGIT_TIMER,
                       TIMER_EXPIRATION, gsm_msgq);

    /* Check if any of the timer cannot be allocated */
    if (kpml_data->inter_digit_timer == NULL ||
        kpml_data->critical_timer == NULL ||
        kpml_data->extra_digit_timer == NULL) {

        /* generate error to indicate timer cannot be allocated */
        KPML_ERROR(KPML_F_PREFIX"No memory to allocate timer\n",
                    fname);
        return;
    }

    /* Start interdigit timer */
    kpml_start_timer(kpml_data->line, kpml_data->call_id,
                     kpml_data->inter_digit_timer,
                     kpml_data->inttimeout, kpml_data->kpml_id);

    /* Start critical timer */
    kpml_start_timer(kpml_data->line, kpml_data->call_id,
                     kpml_data->critical_timer,
                     kpml_data->crittimeout, kpml_data->kpml_id);

    /* Start extra digit timer */
    kpml_start_timer(kpml_data->line, kpml_data->call_id,
                     kpml_data->extra_digit_timer,
                     kpml_data->extratimeout, kpml_data->kpml_id);

}

/*
 *  Function: kpml_restart_timers()
 *
 *  Parameters: kpml_data_t - subscription data
 *              kpml_data_t - kpml data
 *
 *  Description: Restart all the KPML related timers
 *
 *  Returns: None
 */
static void
kpml_restart_timers (kpml_data_t * kpml_data)
{
    static const char fname[] = "kpml_restart_timers";

    KPML_DEBUG(DEB_L_C_F_PREFIX"Restart all timers\n",
               DEB_L_C_F_PREFIX_ARGS(KPML_INFO, kpml_data->line, kpml_data->call_id, fname));

    kpml_stop_timer(kpml_data->critical_timer);

    kpml_stop_timer(kpml_data->inter_digit_timer);

    kpml_stop_timer(kpml_data->extra_digit_timer);

    kpml_start_timers(kpml_data);

    return;
}

/*
 *  Function: kpml_clear_data()
 *
 *  Parameters: kpml_data - Subscription related information
 *              kpml_data  - KPML realated information
 *              kpml_sub_type_e - Type of kpml subscription - persistent
 *                            single-notify or one shot
 *
 *  Description: Clear the KPML subscription data depending upon
 *            KPML request type. If the KPML request type is one shot
 *            clear the data or else maintain it for the duration of the
 *            subscription.
 *
 *  Returns: kpml_data next in the list if succesful
 *            NULL - if not
 */
static boolean
kpml_clear_data (kpml_data_t *kpml_data, kpml_sub_type_e sub_type)
{
    static const char fname[] = "kpml_clear_data";

    KPML_DEBUG(DEB_L_C_F_PREFIX"sub_type=%d",
               DEB_L_C_F_PREFIX_ARGS(KPML_INFO, kpml_data->line, kpml_data->call_id, fname), sub_type);

    switch (sub_type) {

    case KPML_ONE_SHOT:
        KPML_DEBUG(DEB_F_PREFIX"One shot\n", DEB_F_PREFIX_ARGS(KPML_INFO, fname));

        kpml_stop_timer(kpml_data->inter_digit_timer);
        kpml_data->inter_digit_timer = NULL;

        kpml_stop_timer(kpml_data->critical_timer);
        kpml_data->critical_timer = NULL;

        kpml_stop_timer(kpml_data->extra_digit_timer);
        kpml_data->extra_digit_timer = NULL;

        kpml_stop_timer(kpml_data->sub_timer);
        kpml_data->sub_timer = NULL;

        (void) sll_remove(s_kpml_list, kpml_data);

        kpml_release_data(kpml_data);

        kpmlmap_show();
        return (TRUE);

    case KPML_PERSISTENT:
        KPML_DEBUG(DEB_F_PREFIX"Persistent\n", DEB_F_PREFIX_ARGS(KPML_INFO, fname));
        /* FALLTHROUGH */

    case KPML_SINGLY_NOTIFY:
        KPML_DEBUG(DEB_F_PREFIX"Singly notify\n", DEB_F_PREFIX_ARGS(KPML_INFO, fname));

        /*
         * persistent KPML request so clear the
         * digit buffer and then restart all the timer
         */
        kpml_data->kpmlDialed[0] = 00;

        kpml_restart_timers(kpml_data);
        /*
         * if the persistent request is singly-notify
         * do not terminate the subscription, but do
         * not notify any more digits untill application
         * sends out new kpml document. At this time persistent
         * subsciption is not supported.
         */

        kpmlmap_show();
        return (FALSE);

    default:
        KPML_DEBUG(DEB_F_PREFIX"KPML type not specified\n", DEB_F_PREFIX_ARGS(KPML_INFO, fname));
        return (FALSE);
    }
}

/*
 *  Function: kpml_quarantine_digits()
 *
 *  Parameters: line - line number
 *              call_id - call id of the dialog
 *              digits - collected difits
 *              len - Length of collected digits, currently this length
 *               is 1
 *
 *  Description: Quarantine collected digits in a chain of subscription
 *              strucutre. if there is no subscription structure creaded
 *              already then create a new one. The maximum number of digits
 *              collected per dialog is 256. if there are more digits need to
 *              be collected then overwrite the oldest digit.
 *
 *  Returns: None
 */
void
kpml_quarantine_digits (line_t line, callid_t call_id, char digit)
{
    static const char fname[] = "kpml_quarantine_digits";
    kpml_data_t *kpml_data;
    kpml_key_t kpml_key;

    if (kpml_get_config_value() == KPML_NONE) {
        return;
    }

    KPML_DEBUG(DEB_L_C_F_PREFIX"digit=0x%0x\n",
               DEB_L_C_F_PREFIX_ARGS(KPML_INFO, line, call_id, fname), digit);

    kpml_create_sm_key(&kpml_key, line, call_id, NULL);

    kpml_data = (kpml_data_t *) sll_find(s_kpml_list, &kpml_key);

    if (!kpml_data) {

        kpml_data = kpml_get_new_data();

        if (kpml_data == NULL) {
            KPML_ERROR(KPML_F_PREFIX"No memory for subscription data\n",
                    fname);
            return;
        }

        (void) sll_append(s_kpml_list, kpml_data);

        kpml_data->line = line;

        kpml_data->call_id = call_id;

        kpml_data->pending_sub = FALSE;

        kpml_data->dig_head = kpml_data->dig_tail = 0;

    }

    if (kpml_data->dig_head == (kpml_data->dig_tail + 1) % MAX_DIALSTRING) {

        /* buffer is full so discard old collected digits */
        kpml_data->dig_head = (kpml_data->dig_head + 1) % MAX_DIALSTRING;
    }

    kpml_data->q_digits[kpml_data->dig_tail] = digit;

    kpml_data->dig_tail = (kpml_data->dig_tail + 1) % MAX_DIALSTRING;
}


/*
 *  Function: kpml_flush_quarantine_buffer()
 *
 *  Parameters: line - line number
 *              call_id - call id of the dialog
 *
 *  Description: This function will be called to flush any
 *       quarantined buffers.
 *
 *  Returns: None
 */
void
kpml_flush_quarantine_buffer (line_t line, callid_t call_id)
{
    static const char fname[] = "kpml_flush_quarantine_buffer";
    kpml_data_t *kpml_data;
    kpml_key_t kpml_key;

    if (kpml_get_config_value() == KPML_NONE) {
        return;
    }

    KPML_DEBUG(DEB_L_C_F_PREFIX"Flush buffer\n", DEB_L_C_F_PREFIX_ARGS(KPML_INFO, line, call_id, fname));

    kpml_create_sm_key(&kpml_key, line, call_id, NULL);

    kpml_data = (kpml_data_t *) sll_find(s_kpml_list, &kpml_key);

    if (kpml_data) {

        if (!kpml_data->pending_sub) {

            kpml_data->dig_head = kpml_data->dig_tail = 0;
            (void) kpml_clear_data(kpml_data, KPML_ONE_SHOT);
        }

    }

}

/*
 *  Function: kpml_update_quarantined_digits()
 *
 *  Parameters: kpml_data_t : subscription type
 *
 *  Description: This function will be called to update any
 *       quarantined buffer through KPML patter map.
 *
 *  Returns: None
 */
static void
kpml_update_quarantined_digits (kpml_data_t *kpml_data)
{
    static const char fname[] = "kpml_update_quarantined_digits";

    KPML_DEBUG(DEB_L_C_F_PREFIX"Update quarantined digits\n",
               DEB_L_C_F_PREFIX_ARGS(KPML_INFO, kpml_data->line, kpml_data->call_id, fname));

    while (kpml_data->dig_head != kpml_data->dig_tail) {

        kpml_update_dialed_digits(kpml_data->line,
                                          kpml_data->call_id,
                                          kpml_data->q_digits[kpml_data->dig_head]);

        kpml_data->dig_head = (kpml_data->dig_head + 1) % MAX_DIALSTRING;
    }
}

/*
 *  Function: kpml_digit_timer_callback()
 *
 *  Parameters: timer - pointer to timer
 *              line - Line number
 *              call_id - call id
 *
 *  Description: Handle different digit timer events. At this time it
 *          hanles only inter digit timer.
 *
 *  Returns: None
 */
void
kpml_inter_digit_timer_callback (void *kpml_key_p)
{
    (void) app_send_message(&kpml_key_p, sizeof(void *), CC_SRC_GSM,
                            SUB_MSG_KPML_DIGIT_TIMER);
}

kpml_data_t *kpml_get_kpml_data_from_kpml_id(uint32_t kpml_id)
{
    kpml_data_t *kpml_data;

    kpml_data = (kpml_data_t *) sll_next(s_kpml_list, NULL);

    while (kpml_data != NULL && kpml_data->kpml_id != kpml_id) {

        kpml_data = (kpml_data_t *) sll_next(s_kpml_list, kpml_data);
    }
    return(kpml_data);
}

/*
 *  Function: digit_timer_callback()
 *
 *  Parameters: timer - pointer to timer
 *              line - Line number
 *              call_id - call id
 *
 *  Description: Handle different digit timer events. At this time it
 *          hanles only inter digit timer.
 *
 *  Returns: None
 */
static void
kpml_inter_digit_timer_event (void **kpml_key_p)
{
    const char fname[] = "kpml_inter_digit_timer_callback";
    kpml_data_t *kpml_data;

    KPML_DEBUG("%s: kpml_id=%d \n ",
               fname, (long)*kpml_key_p);


    kpml_data = kpml_get_kpml_data_from_kpml_id((long)*kpml_key_p);

    if (kpml_data == NULL) {
        KPML_ERROR(KPML_F_PREFIX"KPML data not found.\n", fname);
        return;
    }

        KPML_DEBUG(": Interdigit Timer\n");

        kpml_generate_notify(kpml_data, FALSE, KPML_TIMER_EXPIRE,
                             KPML_TIMER_EXPIRE_STR);

        /* Timer expired so clear the KPML data */
        (void) kpml_clear_data(kpml_data, kpml_data->persistent);
}

/*
 *  Function: kpml_start_subscription_timer()
 *
 *  Parameters: kpml_data_t - subscription data
 *
 *  Description: Allocate and start subscription timer
 *
 *  Returns: Pointer to timer
 */
static void *
kpml_start_subscription_timer (kpml_data_t * kpml_data, unsigned long duration)
{
    static const char fname[] = "kpml_start_subscription_timer";

    KPML_DEBUG(DEB_L_C_F_PREFIX"duration=%u\n",
               DEB_L_C_F_PREFIX_ARGS(KPML_INFO, kpml_data->line, kpml_data->call_id, fname), duration);

    kpml_data->sub_timer = cprCreateTimer("sub timer",
                                          GSM_KPML_SUBSCRIPTION_TIMER,
                                          TIMER_EXPIRATION, gsm_msgq);

    kpml_data->sub_duration = duration;

    kpml_create_sm_key(&(kpml_data->subtimer_key), kpml_data->line,
                       kpml_data->call_id, kpml_data->sub_timer);

    /* Start subscription duration timer */
    kpml_start_timer(kpml_data->line,
                     kpml_data->call_id,
                     kpml_data->sub_timer,
                     kpml_data->sub_duration * 1000,
                     kpml_data->kpml_id);

    return (kpml_data->sub_timer);
}


/*
 *  Function: kpml_subscription_timer_callback()
 *
 *  Parameters: timer - timer pointer
 *
 *  Description: This routine called by timer callback once the subscription
 *          expires.
 *
 *  Returns: None
 */
void
kpml_subscription_timer_callback (void *kpml_key_p)
{
    (void) app_send_message(&kpml_key_p, sizeof(void *), CC_SRC_GSM,
                            SUB_MSG_KPML_SUBSCRIBE_TIMER);
}

/*
 *  Function: kpml_subscription_timer_event()
 *
 *  Parameters: timer - timer pointer
 *
 *  Description: This routine called by timer callback once the subscription
 *          expires.
 *
 *  Returns: None
 */
static void
kpml_subscription_timer_event (void **kpml_key_p)
{
    static const char fname[] = "kpml_subscription_timer_event";
    kpml_data_t *kpml_data;

    KPML_DEBUG("%s: kpml_id=%d \n ",
               fname, (long)*kpml_key_p);

    kpml_data = kpml_get_kpml_data_from_kpml_id((long)*kpml_key_p);

    /* Evaluate if we need to send empty notify */
    if (kpml_data) {
        kpml_generate_notify(kpml_data, FALSE, KPML_SUB_EXPIRE,
                             KPML_SUB_EXPIRE_STR);

        (void) kpml_clear_data(kpml_data, KPML_ONE_SHOT);
    }
}

/*
 *  Function: kpml_match_requested_digit()
 *
 *  Parameters: input - single char
 *
 *  Description: Determine if the char is 0-9 or *
 *               # is not matched here since it is
 *               primarily used as "dial immediately"
 *
 *  Returns: boolean
 */
static boolean
kpml_match_requested_digit (kpml_regex_match_t *regex_match, char input)
{
    uint32_t *bitmask;
    uint32_t result = 0;

    bitmask = (uint32_t *) &(regex_match->u.single_digit_bitmask);

    switch (input) {
    case '1':
        result = (*bitmask & REGEX_1);
        break;
    case '2':
        result = (*bitmask & REGEX_2);
        break;
    case '3':
        result = (*bitmask & REGEX_3);
        break;
    case '4':
        result = (*bitmask & REGEX_4);
        break;
    case '5':
        result = (*bitmask & REGEX_5);
        break;
    case '6':
        result = (*bitmask & REGEX_6);
        break;
    case '7':
        result = (*bitmask & REGEX_7);
        break;
    case '8':
        result = (*bitmask & REGEX_8);
        break;
    case '9':
        result = (*bitmask & REGEX_9);
        break;
    case '0':
        result = (*bitmask & REGEX_0);
        break;
    case '*':
        result = (*bitmask & REGEX_STAR);
        break;
    case '#':
        result = (*bitmask & REGEX_POUND);
        break;
    case 'A':
        result = (*bitmask & REGEX_A);
        break;
    case 'B':
        result = (*bitmask & REGEX_B);
        break;
    case 'C':
        result = (*bitmask & REGEX_C);
        break;
    case 'D':
        result = (*bitmask & REGEX_D);
        break;
    case '+':
        result = (*bitmask & REGEX_PLUS);
        break;
    default:
        result = 0;
        break;
    }

    if (result) {
        return (TRUE);
    }

    return (FALSE);
}

/*
 *  Function: kpml_match_pattern
 *
 *  Parameters: kpml_data_t - Data strcuture holding digits and KPML
 *          parameters
 *
 *  Description: At this time only one digit reporting is supported. So
 *        match the incoming digit to 1st template
 *
 *  Returns: kpml_match_action_e
 */
static kpml_match_action_e
kpml_match_pattern (kpml_data_t *ptempl)
{
    char *pinput;
    int regex_index = 0;

    pinput = &ptempl->kpmlDialed[0];

    if (strchr(pinput, '#') && ptempl->enterkey) {

        return (KPML_IMMEDIATELY);
    }

    if (kpml_match_requested_digit
        (&(ptempl->regex_match[regex_index]), pinput[0])) {
        return (KPML_FULLPATTERN);
    }

    return (KPML_NOMATCH);
}

/*
 *  Function: kpml_is_subscribed()
 *
 *  Parameters: line - line number on which SUB created
 *              call_id - call id of the call
 *
 *  Description: checks if kpml is susbcribed for this call.
 *
 *  Returns: status - TRUE - if there is valid subscription
 *                    FALSE - if there is no valid subscription
 */
boolean kpml_is_subscribed (callid_t call_id, line_t line)
{
    static const char fname[] = "kpml_is_subscribed";
    kpml_data_t *kpml_data, *kpml_next_data;

    KPML_DEBUG(DEB_L_C_F_PREFIX, DEB_L_C_F_PREFIX_ARGS(KPML_INFO, line, call_id, fname));

    kpml_data = (kpml_data_t *) sll_next(s_kpml_list, NULL);
    while (kpml_data) {
        kpml_next_data = (kpml_data_t *) sll_next(s_kpml_list, kpml_data);
        if (kpml_data->pending_sub &&
            kpml_data->line == line &&
            kpml_data->call_id == call_id) {
            return TRUE;
        }
        kpml_data = kpml_next_data;
    }
    return FALSE;
}

/*
 *  Function: kpml_update_dialed_digits()
 *
 *  Parameters: line - line number on which SUB created
 *              call_id - call id of the call
 *              digits - digit collected (it accepts only one digit now)
 *              len - length of the digit string =1
 *
 *  Description: Routine called by UI to update any collected digits. At this time
 *          this routine handles only one digits at a time. Any given pattern will be
 *          matched with incoming KPML patter. if there is a match notify will be
 *          sent out. If there is no match digits are buffered for the future use.
 *
 *  Returns: status - TRUE - if there is valid subscription
 *                    FALSE - if there is no valis subscription
 */
kpml_state_e
kpml_update_dialed_digits (line_t line, callid_t call_id, char digit)
{
    static const char fname[] = "kpml_update_dialed_digits";
    kpml_data_t *kpml_data, *kpml_next_data;
    kpml_match_action_e result = KPML_NOMATCH;
    int dial_len = 0;
    kpml_state_e state = NO_SUB_DATA;


    if (kpml_get_config_value() == KPML_NONE) {
        return (state);
    }

    KPML_DEBUG(DEB_L_C_F_PREFIX"digits=0x%x\n",
               DEB_L_C_F_PREFIX_ARGS(KPML_INFO, line, call_id, fname), digit);

    kpml_data = (kpml_data_t *) sll_next(s_kpml_list, NULL);

    while (kpml_data) {

        kpml_next_data = (kpml_data_t *) sll_next(s_kpml_list, kpml_data);

        if (kpml_data->pending_sub &&
            kpml_data->line == line &&
            kpml_data->call_id == call_id) {

            state = SUB_DATA_FOUND;

            /* update the digit string */
            dial_len = strlen(kpml_data->kpmlDialed);
            if (dial_len >= MAX_DIALSTRING-1)
            {  // not enough room
                KPML_ERROR(DEB_L_C_F_PREFIX"dial_len = [%d] too large\n", DEB_L_C_F_PREFIX_ARGS(KPML_INFO, line, call_id, fname), dial_len);
                return (state);
            }

            //If DIAL softkey is pressed, just send the NOTIFY with 423
            //notfying the CCM that user is done dialing
            if (digit == (char)0x82) {
                kpml_generate_notify(kpml_data, FALSE, KPML_TIMER_EXPIRE,
                             KPML_TIMER_EXPIRE_STR);
                memset(&kpml_data->kpmlDialed[0], 0, MAX_DIALSTRING);
                (void) kpml_clear_data(kpml_data, kpml_data->persistent);
                state = NOTIFY_SENT;
                kpml_data = kpml_next_data;
                continue;
            }

            if (digit == 0x0F) {

                kpml_data->kpmlDialed[dial_len] = '#';

            } else if (digit == 0x0E) {

                kpml_data->kpmlDialed[dial_len] = '*';

            } else {

                kpml_data->kpmlDialed[dial_len] = digit;
            }

            kpml_data->kpmlDialed[dial_len + 1] = 00;

            if (digit == BKSPACE_KEY) {

                kpml_data->last_dig_bkspace = TRUE;

                sstrncpy(kpml_data->kpmlDialed, "bs", sizeof(kpml_data->kpmlDialed));

                result = KPML_FULLPATTERN;

            } else {

                result = kpml_match_pattern(kpml_data);
            }

            switch (result) {

            case KPML_FULLPATTERN:

                kpml_generate_notify(kpml_data, FALSE, KPML_SUCCESS,
                                     KPML_SUCCESS_STR);

                dp_store_digits(line, call_id,
                                (unsigned char)((digit == (char) BKSPACE_KEY) ?
                                                 BKSPACE_KEY : kpml_data->
                                                 kpmlDialed[dial_len]));
                /* if not persistent request clear the subscription */
                memset(&kpml_data->kpmlDialed[0], 0, MAX_DIALSTRING);

                (void) kpml_clear_data(kpml_data, kpml_data->persistent);

                state = NOTIFY_SENT;
                break;

            case KPML_IMMEDIATELY:
                /* Do not sent out '#' key remove that from the buffer */
                kpml_data->kpmlDialed[dial_len] = 00;

                kpml_generate_notify(kpml_data, FALSE, KPML_USER_TERM_NOMATCH,
                                     KPML_USER_TERM_NOMATCH_STR);

                dp_store_digits(line, call_id,
                                (unsigned char)((digit == (char) BKSPACE_KEY) ?
                                                 BKSPACE_KEY : kpml_data->
                                                 kpmlDialed[dial_len]));

                memset(&kpml_data->kpmlDialed[0], 0, MAX_DIALSTRING);

                /* if not persistent request clear the subscription */
                (void) kpml_clear_data(kpml_data, kpml_data->persistent);

                state = NOTIFY_SENT;
                break;

            default:
                memset(&kpml_data->kpmlDialed[0], 0, MAX_DIALSTRING);
                /* Restart the digit timers */
                kpml_restart_timers(kpml_data);
                break;
            }
        }

        /* if there is only one kpml_data then we should stop here.
         * Memory for this is already released in kpml_generate_notify function
         */
        kpml_data = kpml_next_data;
    }

    return (state);
}

/*
 *  Function: kpml_set_subscription_reject()
 *
 *  Parameters: line, call_id to indentify line and call_id
 *
 *  Description:
 *      Set variable to specify that subscription should be
 *  rejected on this line. There are error cases wehere INV
 *  has been sent premeturaly because of error condition, then
 *  subscription on such a line shoule be rejected.
 *
 *  Returns: none
 */
void
kpml_set_subscription_reject (line_t line, callid_t call_id)
{
    static const char fname[] = "kpml_set_subscription_reject";
    kpml_data_t *kpml_data;
    kpml_key_t kpml_key;

    if (kpml_get_config_value() == KPML_NONE) {
        return;
    }

    KPML_DEBUG(DEB_L_C_F_PREFIX"Reject\n", DEB_L_C_F_PREFIX_ARGS(KPML_INFO, line, call_id, fname));

    kpml_create_sm_key(&kpml_key, line, call_id, NULL);

    kpml_data = (kpml_data_t *) sll_find(s_kpml_list, &kpml_key);

    if (kpml_data == NULL) {

        kpml_data = kpml_get_new_data();

        if (kpml_data == NULL) {
            KPML_ERROR(KPML_F_PREFIX"No memory for subscription data\n",
                        fname);
            return;
        }

        (void) sll_append(s_kpml_list, kpml_data);

        kpml_data->line = line;

        kpml_data->call_id = call_id;

        kpml_data->pending_sub = FALSE;

        kpml_data->dig_head = kpml_data->dig_tail = 0;
    }

    kpml_data->sub_reject = TRUE;
}

/*
 *  Function: check_subcription_create_error()
 *
 *  Parameters: Kpml - kpml related data
 *
 *  Description: Check subscription state to see if the subscription can be
 *          created.
 *
 *  Returns: Kpml response code
 *
 */

static kpml_resp_code_e
check_subcription_create_error (kpml_data_t *kpml_data)
{
    static const char fname[] = "check_subcription_create_error";
    lsm_states_t lsm_state;

    lsm_state = lsm_get_state(kpml_data->call_id);

    if (lsm_state == LSM_S_NONE) {

        KPML_ERROR(KPML_L_C_F_PREFIX"NO call with id\n",
                    kpml_data->line, kpml_data->call_id, fname);
        return (KPML_BAD_EVENT);
    }

    if ((lsm_state < LSM_S_CONNECTED) && kpml_data->sub_reject) {

        KPML_ERROR(KPML_L_C_F_PREFIX"Call not in connected state\n",
                    kpml_data->line, kpml_data->call_id, fname);
        return (KPML_BAD_EVENT);
    }

    /* Call is in connected state. So do not reject the subscription */
    kpml_data->sub_reject = FALSE;

    return (KPML_SUCCESS);
}

/*
 *  Function: check_if_kpml_attributes_supported()
 *
 *  Parameters: Kpml - kpml related data
 *
 *  Description: check if the attribute supported in the KPML request
 *
 *  Returns: Kpml response code
 *
 */
static kpml_resp_code_e
check_if_kpml_attributes_supported (KPMLRequest *kpml_sub_data)
{

    if (kpml_sub_data == NULL) {

        return (KPML_BAD_DOC);
    }

    /* 501 : bad document pre
     * header is not supported. If the request carry this reject it
     */
    if ((kpml_sub_data->pattern.longhold != 0) ||
        (kpml_sub_data->pattern.longrepeat != 0) ||
        (kpml_sub_data->pattern.nopartial != 0)) {
        return (KPML_BAD_DOC);
    }

    return (KPML_SUCCESS);
}

/*
 *  Function: check_attributes_range()
 *
 *  Parameters: Kpml - kpml related data
 *
 *  Description: check attribute range
 *
 *  Returns: kpml response code
 */
static kpml_resp_code_e
check_attributes_range (KPMLRequest * kpml_data)
{
    return (KPML_SUCCESS);
}

/*
 *  Function: check_kpml_config()
 *
 *  Parameters: line and call_id
 *
 *  Description: check kpml config
 *
 *  Returns: kpml response code
 */
static kpml_resp_code_e
check_kpml_config (line_t line, callid_t call_id)
{
    static const char fname[] = "check_kpml_config";
    lsm_states_t lsm_state;

    lsm_state = lsm_get_state(call_id);

    if (lsm_state == LSM_S_NONE) {
        KPML_ERROR(KPML_L_C_F_PREFIX"NO call\n",
                    line, call_id, fname);
        return (KPML_BAD_EVENT);
    }

    /* Get kpml configuration */
    config_get_value(CFGID_KPML_ENABLED, &s_kpml_config, sizeof(s_kpml_config));

    switch (lsm_state) {
    case LSM_S_OFFHOOK:
    case LSM_S_PROCEED:

        if ((s_kpml_config == KPML_SIGNAL_ONLY) || (s_kpml_config == KPML_BOTH)) {
            return (KPML_SUCCESS);
        }

        break;

    case LSM_S_RINGOUT:
    case LSM_S_CONNECTED:
    case LSM_S_HOLDING:

        if ((s_kpml_config == KPML_DTMF_ONLY) || (s_kpml_config == KPML_BOTH)) {
            return (KPML_SUCCESS);
        }

        break;
    default:

        break;
    }

    KPML_ERROR(KPML_L_C_F_PREFIX"KPML disabled - Check your conifg 0- None, \
            1-signaling, 2-dtmf 3-both\n", line, call_id, fname);
    return (KPML_BAD_EVENT);
}

/*
 *  Function: kpml_treat_enterkey()
 *
 *  Parameters: Kpml - kpml related data
 *
 *  Description: Currently we support only '#' in the enter key string.
 *             If there is anything apart from '#' reject the subscription
 *
 *
 *  Returns: kpml_resp_code_e
 */
static kpml_resp_code_e
kpml_treat_enterkey (kpml_data_t *kpml_data, char *enter_str)
{
    /* Enterkey can have only 2 values empty or "#" */

    if (enter_str[0] == NUL) {

        kpml_data->enterkey = FALSE;

    } else if (!strcmp(enter_str, KPML_ENTER_STR)) {

        kpml_data->enterkey = TRUE;

    } else {

        return (KPML_BAD_DOC);

    }

    return (KPML_SUCCESS);
}

/*
 *  Function: kpml_treat_regex()
 *
 *  Parameters: Kpml - kpml related data
 *
 *  Description: Skip white space, and check content of regex
 *             Check for backspace presence as well. Regular
 *             expression can contain only sequence of 'x', space,
 *             '|' and 'bs' (for backspace).
 *  Example : x | bs, xxx|bs, x|bs etc.
 *
 *  Returns: kpml_resp_code_e
 */
static kpml_resp_code_e
kpml_treat_regex (kpml_data_t *kpml_data)
{
    static const char fname[] = "kpml_treat_regex";
    short indx = 0, char_inx, i, regex_idx = 0;
    char regex_temp[32];

    kpml_data->enable_backspace = FALSE;

    KPML_DEBUG(DEB_L_C_F_PREFIX"regex=%u\n",
               DEB_L_C_F_PREFIX_ARGS(KPML_INFO, kpml_data->line, kpml_data->call_id, fname),
               kpml_data->regex[indx].regexData);

    /* skip white space and check for backspace */
    while (indx < NUM_OF_REGX) {
        char_inx = 0;
        i = 0;
        regex_idx = 0;

        while (kpml_data->regex[indx].regexData[char_inx]) {
            switch (kpml_data->regex[indx].regexData[char_inx]) {
            case ' ':
                break;
            case '|':
                break;
            case 'b':
                if (kpml_data->regex[indx].regexData[char_inx + 1] == 's') {
                    char_inx++;
                    kpml_data->enable_backspace = TRUE;
                } else {
                    return (KPML_BAD_DOC);
                }

                break;
            case 'x':
            default:
                regex_temp[i++] = kpml_data->regex[indx].regexData[char_inx];
                break;
            }
            char_inx++;
        }

        regex_temp[i] = NUL;

        /* parse regex string */
        if (kpml_parse_regex_str(&(regex_temp[0]),
                                 &(kpml_data->regex_match[indx])) !=
            KPML_STATUS_OK) {
            KPML_ERROR(KPML_F_PREFIX"Regex parse error.\n",fname);
            return (KPML_BAD_DOC);
        }

        /* create digit map string such as xx to match the digits */
        while (regex_idx < kpml_data->regex_match[indx].num_digits) {
            kpml_data->regex[indx].regexData[regex_idx++] = 'x';
        }

        kpml_data->regex[indx].regexData[regex_idx] = NUL;


        /* Check for next <regex> string */
        indx++;
    }
    return (KPML_SUCCESS);
}

/*
 *  Function: kpml_update_data()
 *
 *  Parameters: Kpml - kpml related data from incoming subscribe
 *
 *  Description: Add a dial template to the known list of templates
 *
 *  Returns: kpml_data_t
 */
static kpml_data_t *
kpml_update_data (kpml_data_t *kpml_data, KPMLRequest *kpml_sub_data)
{
    static const char fname[] = "kpml_update_data";

    if ((kpml_sub_data == NULL) || (kpml_data == NULL)) {
        return (kpml_data);
    }

    memcpy((char *) &(kpml_data->regex),
           (char *) &(kpml_sub_data->pattern.regex),
           sizeof(Regex) * NUM_OF_REGX);

    kpml_data->persistent = kpml_sub_data->pattern.persist;

    kpml_data->inttimeout = kpml_sub_data->pattern.interdigittimer;
    kpml_data->crittimeout = kpml_sub_data->pattern.criticaldigittimer;
    kpml_data->extratimeout = kpml_sub_data->pattern.extradigittimer;

    kpml_data->flush = kpml_sub_data->pattern.flush;

    /* clear all collected digits, SUB requested with flush */
    if (kpml_sub_data->pattern.flush) {

        kpml_data->kpmlDialed[0] = 00;
    }

    kpml_data->longhold = kpml_sub_data->pattern.longhold;

    kpml_data->longrepeat = kpml_sub_data->pattern.longrepeat;

    kpml_data->nopartial = kpml_sub_data->pattern.nopartial;

    KPML_DEBUG(DEB_L_C_F_PREFIX"regex=%u"
               "persistent=%d int-timer=%u critic-timer=%u, extra-timer=%u"
               "flush=%d longhold=%d longrepeat=%d nopartial=%d\n",
               DEB_L_C_F_PREFIX_ARGS(KPML_INFO, kpml_data->line, kpml_data->call_id, fname),
			   kpml_data->regex, kpml_data->persistent, kpml_data->inttimeout,
               kpml_data->crittimeout, kpml_data->extratimeout,
               kpml_data->flush, kpml_data->longhold,
               kpml_data->longrepeat, kpml_data->nopartial);

    return (kpml_data);
}

/*
 *  Function: kpml_terminate_subscription()
 *
 *  Parameters: ccsip_sub_not_data_t - msg passed from SIP stack
 *
 *  Description: Received terminate event from sip stack, probably
 *        something wrong and wants to terminate the subscription.
 *
 *  Returns: None
 */
static void
kpml_terminate_subscription (ccsip_sub_not_data_t *msg)
{
    static const char fname[] = "kpml_terminate_subscribe";
    kpml_data_t *kpml_data = NULL;
    boolean     normal_terminate;
    lsm_lcb_t *lcb;

    KPML_DEBUG(DEB_F_PREFIX"entered.\n", DEB_F_PREFIX_ARGS(KPML_INFO, fname));

    if (kpml_get_config_value() == KPML_NONE) {
        return;
    }

    if (msg->sub_id == (unsigned int)-1) {
        KPML_ERROR(KPML_L_C_F_PREFIX"Invalid sub_id=%d\n", msg->line_id,
                   msg->gsm_id, fname, msg->sub_id);
        return;
    }

    KPML_DEBUG(DEB_L_C_F_PREFIX"sub_id=%d, reason=%d\n",
                DEB_L_C_F_PREFIX_ARGS(KPML_INFO, msg->line_id, msg->gsm_id, fname),
                msg->sub_id, msg->reason_code);
    /*
     * If the terminate reason is caused by local action,
     * then there is no need to send any notify to the network.
     */
    switch (msg->reason_code) {
    case SM_REASON_CODE_SHUTDOWN:
    case SM_REASON_CODE_ROLLOVER:
    case SM_REASON_CODE_RESET_REG:
        /*
         * These errors are caused by failure or system being shutting down.
         * Subscription manager will automatically clean up the subscription
         * The application just needs to clean up the associated data
         * strutures.
         */
        normal_terminate = FALSE;
        break;
    default:
        normal_terminate = TRUE;
        break;
    }

    //Acquire kpml lock
    cprGetMutex(kpml_mutex);

    kpml_data = kpml_data_for_subid(msg->sub_id);

    if (kpml_data) {

        kpml_data->persistent = KPML_ONE_SHOT;

        /*
         * For ignore generating notify and release in the case
         * of non normal terminate as determined above.
         */
        if (normal_terminate) {
            kpml_generate_notify(kpml_data, FALSE,
                                 KPML_SUB_EXPIRE,
                                 KPML_SUB_EXPIRE_STR);

            /* Tell call control to generate reorder */
            lcb = lsm_get_lcb_by_call_id(kpml_data->call_id);
            if (lcb && lcb->state < LSM_S_RINGOUT) {
                cc_release(CC_SRC_GSM, kpml_data->call_id, kpml_data->line,
                           CC_CAUSE_CONGESTION, NULL, NULL);
            }
        }

        (void) kpml_clear_data(kpml_data, KPML_ONE_SHOT);
    }

    //Release kpml lock
    cprReleaseMutex(kpml_mutex);

    /*
     * For ignore generating notify and release in the case
     * of non normal terminate as determined above.
     */
    if (normal_terminate) {
        (void) sub_int_subscribe_term(msg->sub_id, TRUE,
                                      msg->request_id, msg->event);
    }
    KPML_DEBUG(DEB_F_PREFIX"exit.\n", DEB_F_PREFIX_ARGS(KPML_INFO, fname));

}

/*
 *  Function: kpml_receive_subscribe()
 *
 *  Parameters: ccsip_sub_not_data_t - msg passed from SIP stack
 *
 *  Description: Responsible for handling incoming SUBSCRIBE with KPML
 *              Checks if the subscription expired or it is a re-subscribe
 *              sents out intial notification, starts all the timers
 *
 *  Returns: None
 */
static void
kpml_receive_subscribe (ccsip_sub_not_data_t *msg)
{
    static const char fname[] = "kpml_receive_subscribe";
    kpml_data_t *kpml_data;
    kpml_key_t kpml_key;
    kpml_resp_code_e resp_code = KPML_SUCCESS;
    KPMLRequest *kpml_sub_data = NULL;
    lsm_states_t lsm_state;
    char      *regx_prnt = NULL;
    boolean is_empty_resubscribe = FALSE;

    if (kpml_get_config_value() == KPML_NONE) {
        KPML_DEBUG(DEB_L_C_F_PREFIX"KPML disabled in config.\n",
                   DEB_L_C_F_PREFIX_ARGS(KPML_INFO, msg->line_id, msg->gsm_id, fname));
        return;
    }

    if (msg->line_id == 0 || msg->gsm_id == 0) {

        KPML_ERROR(KPML_L_C_F_PREFIX"Line or call_id not correct\n",
                    msg->line_id, msg->gsm_id, fname);
        (void) sub_int_subscribe_ack(CC_SRC_GSM, CC_SRC_SIP, msg->sub_id,
                                     KPML_BAD_EVENT, msg->sub_duration);
        return;
    }

    kpml_create_sm_key(&kpml_key, (line_t) msg->line_id, (callid_t) msg->gsm_id,
                       NULL);

    kpml_data = (kpml_data_t *) sll_find(s_kpml_list, &kpml_key);

    if (msg->u.subs_ind_data.eventData) {

        kpml_sub_data = &(msg->u.subs_ind_data.eventData->u.kpml_request);
    }


    /* There is active KPML subscription on this dialog
     * so check if the existing subscription need to be
     * terminated or refreshed
     */
    if (kpml_data) {
        if (kpml_data->pending_sub == TRUE) {

            kpml_data->sub_duration = msg->sub_duration;
            /* generate SUBSCRIBE response */
            kpml_generate_subscribe_response(kpml_data, KPML_SUCCESS);

            /* KPML receives a new subscription for the same dialog.
             * Terminate the current subscription if the new subscription
             * is not related
             */
            if (kpml_data->sub_id != msg->sub_id) {

                KPML_DEBUG(DEB_L_C_F_PREFIX"Terminate previous subscription \
                           sub_id = %x\n",
						   DEB_L_C_F_PREFIX_ARGS(KPML_INFO, kpml_data->line, kpml_data->call_id, fname),
						   kpml_data->sub_id);

                kpml_generate_notify(kpml_data, FALSE,
                                     KPML_SUB_EXPIRE,
                                     KPML_SUB_EXPIRE_STR);

                (void) sub_int_subscribe_term(kpml_data->sub_id, TRUE,
                                              msg->request_id, msg->event);
            }

            KPML_DEBUG(DEB_L_C_F_PREFIX"Refresh Subscription\n",
                       DEB_L_C_F_PREFIX_ARGS(KPML_INFO, kpml_data->line, kpml_data->call_id, fname));
            /* Refresh subscription without kpml body. This is CCM current behavior
             * which seems not to follow rfc4730. We support this for compatibility
             * purpose, and intepret the body to be exactly the same as previous
             * subscription. Only new subscription time changes.
             */
            if (kpml_sub_data == NULL) {
                 kpml_clear_timers(kpml_data);
                 is_empty_resubscribe = TRUE;
            } else if (kpml_clear_data(kpml_data, KPML_ONE_SHOT)) {
                kpml_data = NULL;
            }
        } else {

            /* Already has subscription data created to quaratining
             * digits, but there is no active SIP subscription.
             */

            kpml_data = kpml_update_data(kpml_data, kpml_sub_data);

            KPML_DEBUG(DEB_L_C_F_PREFIX"Activate Subscription\n",
                       DEB_L_C_F_PREFIX_ARGS(KPML_INFO, kpml_data->line, kpml_data->call_id, fname));
        }

    }

    if (kpml_sub_data) {
        regx_prnt = kpml_sub_data->pattern.regex.regexData;
    }

    DEF_DEBUG(DEB_L_C_F_PREFIX"Regex=%s\n",
            DEB_L_C_F_PREFIX_ARGS(KPML_INFO, msg->line_id, msg->gsm_id, fname), regx_prnt);

    /* Above block can terminate existing subscription */
    if (!kpml_data) {

        /* New subscription - allocate data */
        kpml_data = kpml_get_new_data();

        if (kpml_data == NULL) {
            KPML_ERROR(KPML_L_C_F_PREFIX"No memory for subscription data\n",
                    msg->line_id, msg->gsm_id, fname);
            return;
        }

        (void) kpml_update_data(kpml_data, kpml_sub_data);

        (void) sll_append(s_kpml_list, kpml_data);

    }

    kpml_data->call_id = msg->gsm_id;

    kpml_data->line = msg->line_id;

    kpml_data->sub_id = msg->sub_id;

    /* Terminate the subcription */
    if (msg->sub_duration == 0) {

        /* Update KPML document in the record */
        kpml_data = kpml_update_data(kpml_data, kpml_sub_data);

        KPML_DEBUG(DEB_L_C_F_PREFIX"Terminate Subscription.\n",
                   DEB_L_C_F_PREFIX_ARGS(KPML_INFO, kpml_data->line, kpml_data->call_id, fname));

        /* check the regular expression and backspace */
        (void) kpml_treat_regex(kpml_data);

        /* Remove backspace softkey */
        ui_control_featurekey_bksp(msg->line_id, msg->gsm_id,
                                   kpml_data->last_dig_bkspace);

        /* Set call to proceeding state */
        cc_proceeding(CC_SRC_SIP, kpml_data->call_id, kpml_data->line, NULL);

        kpml_data->persistent = KPML_ONE_SHOT;

        kpml_generate_notify(kpml_data, FALSE,
                             KPML_SUB_EXPIRE,
                             KPML_SUB_EXPIRE_STR);
        /* Empty all the digits present in the list */
        kpml_data->dig_head = kpml_data->dig_tail = 0;

        (void) kpml_clear_data(kpml_data, KPML_ONE_SHOT);

        /* free event data allocated by SIP stack */
        if (msg->u.subs_ind_data.eventData) {
            cpr_free(msg->u.subs_ind_data.eventData);
        }

        return;

    }

    kpml_data->pending_sub = TRUE;

    (void) kpml_start_subscription_timer(kpml_data, msg->sub_duration);

    kpml_start_timers(kpml_data);

    /* check config to send appropriate response. Check subscription reject
     * status.
     */
    if (((resp_code = check_kpml_config(msg->line_id, msg->gsm_id)) != KPML_SUCCESS) ||
        ((resp_code = check_subcription_create_error(kpml_data)) != KPML_SUCCESS)) {

        kpml_generate_subscribe_response(kpml_data, resp_code);

        if (kpml_clear_data(kpml_data, KPML_ONE_SHOT)) {
            kpml_data = NULL;
        }

        if (msg->u.subs_ind_data.eventData) {
            cpr_free(msg->u.subs_ind_data.eventData);
        }
        return;

    } else {
        /*  Accept the subscription with a 200 OK response */
        kpml_generate_subscribe_response(kpml_data, SIP_SUCCESS_SETUP);
    }

    /* Error checking for incoming KPML data. If there are attributes
     * which are not supported or if the values are not in the range
     * or KPML document is not present then generate error
     */
    if (!is_empty_resubscribe && ((kpml_sub_data == NULL) ||
        ((resp_code = check_if_kpml_attributes_supported(kpml_sub_data)) != KPML_SUCCESS) ||
        ((resp_code = check_attributes_range(kpml_sub_data)) != KPML_SUCCESS) ||
        ((resp_code = kpml_treat_regex(kpml_data)) != KPML_SUCCESS) ||
        ((resp_code = kpml_treat_enterkey(kpml_data,
                                          kpml_sub_data->pattern.enterkey)) != KPML_SUCCESS))) {


        KPML_ERROR(KPML_F_PREFIX"Error Resp code = %d\n", fname, resp_code);

        kpml_generate_notify(kpml_data, FALSE, resp_code,
                             KPML_ATTR_NOT_SUPPORTED_STR);

        if (kpml_clear_data(kpml_data, KPML_ONE_SHOT)) {
            kpml_data = NULL;
        }

        cpr_free(msg->u.subs_ind_data.eventData);

        return;
    } else {

        lsm_state = lsm_get_state(kpml_data->call_id);

        /* When GSM receives FEATURE event transitions its state. To avoid
         * transition during DTMF phase do not send out the event if the
         * lsm state is > RINGOUT state. GSM has to know about the subscription
         * only to extend the collect info (KPML_COLLECT_INFO)
         */

        if ((lsm_state != LSM_S_NONE) && (lsm_state < LSM_S_RINGOUT)) {

            cc_feature(CC_SRC_GSM, kpml_data->call_id, kpml_data->line,
                       CC_FEATURE_SUBSCRIBE, NULL);
        }

        kpml_generate_notify(kpml_data, TRUE, KPML_SUCCESS, KPML_TRYING_STR);

        kpml_update_quarantined_digits(kpml_data);
    }

    /* If the backspace key request is set then enable backspace key */
    ui_control_featurekey_bksp(msg->line_id, msg->gsm_id,
                               kpml_data->enable_backspace);

    /* free event data allocated by SIP stack */
    if (msg->u.subs_ind_data.eventData) {
    cpr_free(msg->u.subs_ind_data.eventData);
    }
}

/*
 *  Function: kpml_generate_subscribe_response()
 *
 *  Parameters: ccsip_sub_not_data_t - msg data from SIP
 *
 *  Description: This routine is called to generate 200 Ok for incoming
 *              KPML SUBSCRIBE.
 *
 *  Returns: None
 */
static void
kpml_generate_subscribe_response (kpml_data_t * kpml_data, int resp_code)
{
    static const char fname[] = "kpml_generate_subscribe_response";

    KPML_DEBUG(DEB_L_C_F_PREFIX"SUB response\n",
		       DEB_L_C_F_PREFIX_ARGS(KPML_INFO, kpml_data->line, kpml_data->call_id, fname));

    (void) sub_int_subscribe_ack(CC_SRC_GSM, CC_SRC_SIP, kpml_data->sub_id,
                                 (uint16_t) resp_code, kpml_data->sub_duration);
}

/*
 *  Function: kpml_receive_notify_response()
 *
 *  Parameters: ccsip_sub_not_data_t - msg data from SIP
 *
 *  Description: This routine is called when there is a NOTIFY
 *              response from subscription manager.
 *
 *  Returns: None
 */
void
kpml_receive_notify_response (ccsip_sub_not_data_t *msg)
{
    static const char fname[] = "kpml_receive_notify_response";
    kpml_data_t *kpml_data;
    kpml_key_t kpml_key;

    KPML_DEBUG(DEB_L_C_F_PREFIX"Notify response\n",
               DEB_L_C_F_PREFIX_ARGS(KPML_INFO, msg->line_id, msg->gsm_id, fname));

    kpml_create_sm_key(&kpml_key, (line_t) msg->line_id, (callid_t) msg->gsm_id,
                       NULL);

    kpml_data = (kpml_data_t *) sll_find(s_kpml_list, &kpml_key);


    /* Do not terminate subscription if the subscription is
     *  persistent or result is not error
     */

    if (kpml_data) {
        if (kpml_data->last_dig_bkspace &&
            msg->u.notify_result_data.status_code == SIP_SUCCESS_SETUP) {
            /* remove last digit which is backspace */
            dp_delete_last_digit(msg->line_id, msg->gsm_id);
            kpml_data->last_dig_bkspace = FALSE;
        } else if (msg->u.notify_result_data.status_code == REQUEST_TIMEOUT) {

            (void) kpml_clear_data(kpml_data, KPML_ONE_SHOT);
            (void) sub_int_subscribe_term(msg->sub_id, TRUE, msg->request_id,
                                          msg->event);
            return;
        }

        /* See if there are digits collected before the subscription
         * if so then update the kpml_digit buffer and match the pattern
         */
        kpml_update_quarantined_digits(kpml_data);
    } else {
        (void) sub_int_subscribe_term(msg->sub_id, TRUE, msg->request_id,
                                      msg->event);
    }
}

/*
 *  Function: kpml_generate_notify()
 *
 *  Parameters: kpml_data - subscription data
 *              resp_code - response code for the NOTIFY
 *              resp_text - response text
 *
 *  Description: Notify KPML response data. This can be sucessful or error.
 *
 *  Returns: None
 */
static void
kpml_generate_notify (kpml_data_t *kpml_data, boolean no_body,
                      unsigned int resp_code, char *resp_text)
{
    static const char fname[] = "kpml_generate_notify";
    char resp_str[10];
    ccsip_event_data_t *peventData = NULL;

    DEF_DEBUG(DEB_L_C_F_PREFIX"RESP %u: \n",
        DEB_L_C_F_PREFIX_ARGS(KPML_INFO, kpml_data->line, kpml_data->call_id, fname), resp_code);

    if (no_body == FALSE) {
        // allocate memory to hold events and that will be freed in the stack
        peventData = (ccsip_event_data_t *)
            cpr_malloc(sizeof(ccsip_event_data_t));

        if (peventData == NULL) {
            KPML_ERROR(KPML_L_C_F_PREFIX"No memory for eventdata\n",
                kpml_data->line, kpml_data->call_id, fname);
            return;
        }

        memset(peventData, 0, sizeof(ccsip_event_data_t));

        sstrncpy(peventData->u.kpml_response.version, KPML_VER_STR, sizeof(peventData->u.kpml_response.version));

        snprintf(resp_str, 10, "%d", resp_code);
        sstrncpy(peventData->u.kpml_response.code, resp_str, sizeof(peventData->u.kpml_response.code));

        if (resp_code == KPML_SUCCESS) {

            sstrncpy(&(peventData->u.kpml_response.digits[0]),
                    &(kpml_data->kpmlDialed[0]), sizeof(peventData->u.kpml_response.digits));
        }

        if (kpml_data->flush == FALSE) {
            sstrncpy(peventData->u.kpml_response.forced_flush, "false",
                    sizeof(peventData->u.kpml_response.forced_flush));
        } else {
            sstrncpy(peventData->u.kpml_response.forced_flush, "true",
                    sizeof(peventData->u.kpml_response.forced_flush));
        }

        sstrncpy(peventData->u.kpml_response.tag,
                &(kpml_data->regex->tag[0]), sizeof(peventData->u.kpml_response.tag));

        sstrncpy(peventData->u.kpml_response.text,
                resp_text, sizeof(peventData->u.kpml_response.text));

        peventData->type = EVENT_DATA_KPML_RESPONSE;
        peventData->next = NULL;
    }

    (void) sub_int_notify(CC_SRC_GSM, CC_SRC_SIP, kpml_data->sub_id,
                          /* kpml_receive_notify_response */ NULL,
                          SUB_MSG_KPML_NOTIFY_ACK, peventData,
                          (kpml_data->persistent ==
                           KPML_ONE_SHOT ? SUBSCRIPTION_TERMINATE :
                           SUBSCRIPTION_NULL));
}

/**
 *
 *  Function to return the message command name for KPML module
 *
 * @param uint32_t command
 *
 * @return  char * pointer to command name
 *
 * @pre     (none)
 */

char *
kpml_get_msg_string (uint32_t cmd)
{
    switch (cmd) {

    case SUB_MSG_KPML_SUBSCRIBE:
        return("KPML_SUB");
    case SUB_MSG_KPML_TERMINATE:
        return("KPML_TERMINATE");
    case SUB_MSG_KPML_NOTIFY_ACK:
        return("KPML_NOT_ACK");
    case SUB_MSG_KPML_SUBSCRIBE_TIMER:
        return("KPML_SUB_TIMER");
    case SUB_MSG_KPML_DIGIT_TIMER:
        return("KPML_DIGIT_TIMER");
    default:
        return ("KPML_UNKNOWN_CMD");
    }
}

void
kpml_process_msg (uint32_t cmd, void *msg)
{
    static const char fname[] = "kpml_process_msg";

    KPML_DEBUG(DEB_F_PREFIX"cmd= %s\n", DEB_F_PREFIX_ARGS(KPML_INFO, fname), kpml_get_msg_string(cmd));

    if (s_kpml_list == NULL) {
        /* KPML is down do not process any message */
        KPML_DEBUG(DEB_F_PREFIX"KPML is down.\n", DEB_F_PREFIX_ARGS(KPML_INFO, fname));
        return;
    }

    switch (cmd) {

    case SUB_MSG_KPML_SUBSCRIBE:
        kpml_receive_subscribe((ccsip_sub_not_data_t *) msg);
        break;

    case SUB_MSG_KPML_TERMINATE:
        kpml_terminate_subscription((ccsip_sub_not_data_t *) msg);
        break;

    case SUB_MSG_KPML_NOTIFY_ACK:
        kpml_receive_notify_response((ccsip_sub_not_data_t *) msg);
        break;

    case SUB_MSG_KPML_SUBSCRIBE_TIMER:
        kpml_subscription_timer_event((void **) msg);
        break;

    case SUB_MSG_KPML_DIGIT_TIMER:
        kpml_inter_digit_timer_event((void **) msg);
        break;

    default:
        KPML_ERROR(KPML_F_PREFIX"Bad Cmd received: 0x%x.\n", fname, cmd);
        break;
    }
}

/*
 *  Function: kpmlmap_show
 *
 *  Parameters:  standard args
 *
 *  Description:  Display the current dialplan (if any)
 *
 *  Returns:
 *
 */
static void
kpmlmap_show (void)
{
    static const char *fname="kpmlmap_show";
    kpml_data_t *kpml_data;
    int counter;

    kpml_data = (kpml_data_t *) sll_next(s_kpml_list, NULL);

    while (kpml_data != NULL) {


        KPML_DEBUG(DEB_L_C_F_PREFIX"Pending sub duration=%-8d",
                   DEB_L_C_F_PREFIX_ARGS(KPML_INFO, kpml_data->line, kpml_data->call_id, fname),
                   kpml_data->sub_duration);

        for (counter = 0; counter < NUM_OF_REGX; counter++) {
            KPML_DEBUG(DEB_F_PREFIX"%-4s  %-10s  %-5s\n", DEB_F_PREFIX_ARGS(KPML_INFO, fname),
                       kpml_data->regex[counter].regexData,
                       kpml_data->regex->tag, kpml_data->kpmlDialed);
        }

        kpml_data = (kpml_data_t *) sll_next(s_kpml_list, kpml_data);
    }
}


/*
 *  Function: show_kpmlmap_cmd
 *
 *  Parameters:  standard args
 *
 *  Description:  Display the current kpml subscription details (if any)
 *
 *  Returns:
 *
 */
cc_int32_t
show_kpmlmap_cmd (cc_int32_t argc, const char *argv[])
{
    kpml_data_t *kpml_data;
    int counter;

    debugif_printf("Pending KPML requests are....\n");

    kpml_data = (kpml_data_t *) sll_next(s_kpml_list, NULL);

    debugif_printf("\n--------------- KPML SUBSCRIPTIONS-------------------");
    debugif_printf("\nLine    Call_Id    Expire   Regx    Tag       Digits ");
    debugif_printf
        ("\n------------------------------------------------------\n");

    while (kpml_data != NULL) {

        debugif_printf("%-4d %-5d  %-8lu ",
                       kpml_data->line, kpml_data->call_id,
                       kpml_data->sub_duration);

        for (counter = 0; counter < NUM_OF_REGX; counter++) {
            debugif_printf("%-4s  %-10s  %-5s\n",
                           kpml_data->regex[counter].regexData,
                           kpml_data->regex->tag, kpml_data->kpmlDialed);
        }

        kpml_data = (kpml_data_t *) sll_next(s_kpml_list, kpml_data);
    }
    return (0);
}


/*
 *  Function: kpml_init()
 *
 *  Parameters: none
 *
 *  Description: Register with Subscription manager for any imcoming
 *              KPML subscribe messages
 *
 *  Returns: None
 */
void
kpml_init (void)
{
    KPML_DEBUG(DEB_F_PREFIX"entered.\n", DEB_F_PREFIX_ARGS(KPML_INFO, "kpml_init"));

    if (!kpml_mutex) {
        kpml_mutex = cprCreateMutex("kpml lock");
        if (!kpml_mutex) {
            KPML_ERROR(DEB_F_PREFIX"unable to create kpml lock \n", "kpml_init");
        }
    }

    (void) sub_int_subnot_register(CC_SRC_GSM, CC_SRC_SIP,
                                   CC_SUBSCRIPTIONS_KPML,
                                   /*kpml_receive_subscribe */ NULL, CC_SRC_GSM,
                                   SUB_MSG_KPML_SUBSCRIBE,
                                   NULL, SUB_MSG_KPML_TERMINATE, 0, 0);

    /* allocate and initialize kpml list */
    s_kpml_list = sll_create((sll_match_e(*)(void *, void *))
                             kpml_match_line_call_id);

}

/*
 *  Function: kpml_shutdown()
 *
 *  Parameters: none
 *
 *  Description: The function shutdowns KPML and cleans up data.
 *
 *  NOTE:
 *
 *  The assumption is existing subscriptions with SUB/NOT are
 *  terminated and released elsewhere i.e. kpml shutdown does not
 *  support shuting down alone while the rest of the SIP/GSM
 *  components are up. Therefore the shutdown function will not
 *  send any subscription termination to SUB/NOT which it will
 *  be droped by SIP stack since it is also being shutdown.
 *
 *  Returns: None
 */
void
kpml_shutdown (void)
{
    kpml_data_t *kpml_data;
    KPML_DEBUG(DEB_F_PREFIX"entered.\n", DEB_F_PREFIX_ARGS(KPML_INFO, "kpml_shutdown"));

    //acquire kpml lock
    (void) cprGetMutex(kpml_mutex);


    kpml_data = (kpml_data_t *) sll_next(s_kpml_list, NULL);

    while (kpml_data != NULL) {

        /*
         * Clean up, remove from the list and deallocate the kpml_data
         */
        (void) kpml_clear_data(kpml_data, KPML_ONE_SHOT);

        /*
         * The kpml_data is already freed above, get the next one
         * from the list's head
         */
        kpml_data = (kpml_data_t *) sll_next(s_kpml_list, NULL);
    }

    sll_destroy(s_kpml_list);

    s_kpml_list = NULL;
    (void) cprReleaseMutex(kpml_mutex);

    KPML_DEBUG(DEB_F_PREFIX"exit.\n", DEB_F_PREFIX_ARGS(KPML_INFO, "kpml_shutdown"));
}
