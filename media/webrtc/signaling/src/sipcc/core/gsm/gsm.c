/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "cpr_types.h"
#include "cpr_memory.h"
#include "cpr_stdio.h"
#include "cpr_stdlib.h"
#include "cpr_ipc.h"
#include "cpr_errno.h"
#include "cpr_time.h"
#include "cpr_rand.h"
#include "cpr_timers.h"
#include "cpr_threads.h"
#include "phone.h"
#include "phntask.h"
#include "gsm.h"
#include "lsm.h"
#include "vcm.h"
#include "fsm.h"
#include "phone_debug.h"
#include "debug.h"
#include "fim.h"
#include "gsm_sdp.h"
#include "ccsip_subsmanager.h"
#include "dialplanint.h"
#include "kpmlmap.h"
#include "subapi.h"
#include "platform_api.h"

static void sub_process_feature_msg(uint32_t cmd, void *msg);
static void sub_process_feature_notify(ccsip_sub_not_data_t *msg, callid_t call_id,
                            callid_t other_call_id);
static void sub_process_b2bcnf_msg(uint32_t cmd, void *msg);
void fsmb2bcnf_get_sub_call_id_from_ccb(fsmcnf_ccb_t *ccb, callid_t *cnf_call_id,
                callid_t *cns_call_id);
extern cprMsgQueue_t gsm_msgq;
void destroy_gsm_thread(void);
void dp_shutdown();
extern void dcsm_process_jobs(void);
extern void dcsm_init(void);
extern void dcsm_shutdown(void);

/* Flag to see whether we can start processing events */

static boolean gsm_initialized = FALSE;
extern cprThread_t gsm_thread;
static media_timer_callback_fp* media_timer_callback = NULL;

/**
 * Add media falsh one time timer call back. It's for ROUNDTABLE only.
 */
void
gsm_set_media_callback(media_timer_callback_fp* callback) {
    media_timer_callback = callback;
}

void
gsm_set_initialized (void)
{
    gsm_initialized = TRUE;
}

boolean
gsm_get_initialize_state (void)
{
    return gsm_initialized;
}

cprBuffer_t
gsm_get_buffer (uint16_t size)
{
    return cpr_malloc(size);
}


cpr_status_e
gsm_send_msg (uint32_t cmd, cprBuffer_t buf, uint16_t len)
{
    phn_syshdr_t *syshdr;

    syshdr = (phn_syshdr_t *) cprGetSysHeader(buf);
    if (!syshdr) {
        return CPR_FAILURE;
    }
    syshdr->Cmd = cmd;
    syshdr->Len = len;

    if (cprSendMessage(gsm_msgq, buf, (void **) &syshdr) == CPR_FAILURE) {
        cprReleaseSysHeader(syshdr);
        return CPR_FAILURE;
    }
    return CPR_SUCCESS;
}


boolean
gsm_process_msg (uint32_t cmd, void *msg)
{
    static const char fname[] = "gsm_process_msg";
    boolean release_msg = TRUE;
    cc_msgs_t       msg_id   = ((cc_setup_t *)msg)->msg_id;
    int             event_id = msg_id;

    GSM_DEBUG(DEB_F_PREFIX"cmd= 0x%x\n", DEB_F_PREFIX_ARGS(GSM, fname), cmd);

    switch (cmd) {
    case GSM_GSM:
    case GSM_SIP:
        if (gsm_initialized) {

            if (event_id == CC_MSG_FEATURE &&
                (((cc_feature_t *) msg)->feature_id == CC_FEATURE_CAC_RESP_PASS)) {

                fsm_cac_process_bw_avail_resp ();

                /* Release all memory for CC_FEATURE_CAC_..message */
                release_msg = TRUE;

                GSM_DEBUG(DEB_F_PREFIX"CAC Message Processed: 0x%x\n", DEB_F_PREFIX_ARGS(GSM, fname), cmd);
            } else  if (event_id == CC_MSG_FEATURE &&
                (((cc_feature_t *) msg)->feature_id == CC_FEATURE_CAC_RESP_FAIL)) {

                fsm_cac_process_bw_failed_resp ();

                /* Release all memory for CC_FEATURE_CAC_..message */
                release_msg = TRUE;

                GSM_DEBUG(DEB_F_PREFIX"CAC Message Processed: 0x%x\n", DEB_F_PREFIX_ARGS(GSM, fname), cmd);
            } else {

                release_msg = fim_process_event(msg, FALSE);
                GSM_DEBUG(DEB_F_PREFIX"Message Processed: 0x%x\n", DEB_F_PREFIX_ARGS(GSM, fname), cmd);
            }
        }
        if (release_msg == TRUE) {
            fim_free_event(msg);
        }
        break;

    default:
        GSM_DEBUG(DEB_F_PREFIX"Unknown Cmd received: 0x%x\n", DEB_F_PREFIX_ARGS(GSM, fname), cmd);
        break;
    }

    return(release_msg);
}


void
gsm_process_timer_expiration (void *msg)
{
    static const char fname[] = "gsm_process_timer_expiration";
    cprCallBackTimerMsg_t *timerMsg;
    void *timeout_msg = NULL;

    timerMsg = (cprCallBackTimerMsg_t *) msg;
    TMR_DEBUG(DEB_F_PREFIX"Timer %s expired\n", DEB_F_PREFIX_ARGS(GSM, fname), timerMsg->expiredTimerName);

    switch (timerMsg->expiredTimerId) {

    case GSM_MULTIPART_TONES_TIMER:
    case GSM_CONTINUOUS_TONES_TIMER:
        lsm_tmr_tones_callback(timerMsg->usrData);
        break;

    case GSM_ERROR_ONHOOK_TIMER:
        fsmdef_error_onhook_timeout(timerMsg->usrData);
        break;

    case GSM_AUTOANSWER_TIMER:
        fsmdef_auto_answer_timeout(timerMsg->usrData);
        break;

    case GSM_REVERSION_TIMER:
        fsmdef_reversion_timeout((callid_t)(long)timerMsg->usrData);
        break;

    case GSM_CAC_FAILURE_TIMER:
        fsm_cac_process_bw_fail_timer(timerMsg->usrData);
        break;

    case GSM_DIAL_TIMEOUT_TIMER:
        dp_dial_timeout(timerMsg->usrData);
        break;

    case GSM_KPML_INTER_DIGIT_TIMER:
        kpml_inter_digit_timer_callback(timerMsg->usrData);
        break;
    case GSM_KPML_CRITICAL_DIGIT_TIMER:
    case GSM_KPML_EXTRA_DIGIT_TIMER:
        break;

    case GSM_KPML_SUBSCRIPTION_TIMER:
        kpml_subscription_timer_callback(timerMsg->usrData);
        break;

    case GSM_REQ_PENDING_TIMER:
        timeout_msg = fsmdef_feature_timer_timeout(
                          CC_FEATURE_REQ_PEND_TIMER_EXP,
                          timerMsg->usrData);
        break;

    case GSM_RINGBACK_DELAY_TIMER:
        timeout_msg = fsmdef_feature_timer_timeout(
                          CC_FEATURE_RINGBACK_DELAY_TIMER_EXP,
                          timerMsg->usrData);
        break;
    case GSM_FLASH_ONCE_TIMER:
        if (media_timer_callback != NULL) {
            (* ((media_timer_callback_fp)(media_timer_callback)))();
        }
        break;
	case GSM_TONE_DURATION_TIMER:
		lsm_tone_duration_tmr_callback(timerMsg->usrData);
		break;
    default:
        GSM_ERR_MSG(GSM_F_PREFIX"unknown timer %d\n", fname,
                    timerMsg->expiredTimerName);
        break;
    }

    /*
     * If there is a timer message to be processed by state machine,
     * hands it to GSM state machine here.
     */
    if (timeout_msg != NULL) {
        /* Let state machine handle glare timer expiration */
        gsm_process_msg(GSM_GSM, timeout_msg);
        cpr_free(timeout_msg);
    }
}

static void
gsm_init (void)
{
    /* Placeholder for any initialization tasks */
}

void
gsm_shutdown (void)
{
    gsm_initialized = FALSE;

    lsm_shutdown();
    fsm_shutdown();
    fim_shutdown();
    dcsm_shutdown();
}

void
gsm_reset (void)
{
    dp_reset();
    lsm_reset();
    fsmutil_free_all_shown_calls_ci_map();
}

void
GSMTask (void *arg)
{
    static const char fname[] = "GSMTask";
    void           *msg;
    phn_syshdr_t   *syshdr;
    boolean        release_msg = TRUE;

    MOZ_ASSERT (gsm_msgq == (cprMsgQueue_t) arg);

    if (!gsm_msgq) {
        GSM_ERR_MSG(GSM_F_PREFIX"invalid input, exiting\n", fname);
        return;
    }

    if (platThreadInit("GSMTask") != 0) {
        return;
    }
    /*
     * Adjust relative priority of GSM thread.
     */
    (void) cprAdjustRelativeThreadPriority(GSM_THREAD_RELATIVE_PRIORITY);

    /*
     * Initialize all the GSM modules
     */
    lsm_init();
    fsm_init();
    fim_init();
    gsm_init();
    dcsm_init();

    cc_init();

    fsmutil_init_shown_calls_ci_map();
    /*
     * On Win32 platform, the random seed is stored per thread; therefore,
     * each thread needs to seed the random number.  It is recommended by
     * MS to do the following to ensure randomness across application
     * restarts.
     */
    cpr_srand((unsigned int)time(NULL));

    /*
     * Cache random numbers for SRTP keys
     */
    gsmsdp_cache_crypto_keys();

    while (1) {

        release_msg = TRUE;

        msg = cprGetMessage(gsm_msgq, TRUE, (void **) &syshdr);
        if (msg) {
            switch (syshdr->Cmd) {
            case TIMER_EXPIRATION:
                gsm_process_timer_expiration(msg);
                break;

            case GSM_SIP:
            case GSM_GSM:
                release_msg = gsm_process_msg(syshdr->Cmd, msg);
                break;

            case DP_MSG_INIT_DIALING:
            case DP_MSG_DIGIT_STR:
            case DP_MSG_STORE_DIGIT:
            case DP_MSG_DIGIT:
            case DP_MSG_DIAL_IMMEDIATE:
            case DP_MSG_REDIAL:
            case DP_MSG_ONHOOK:
            case DP_MSG_OFFHOOK:
            case DP_MSG_UPDATE:
            case DP_MSG_DIGIT_TIMER:
            case DP_MSG_CANCEL_OFFHOOK_TIMER:
                dp_process_msg(syshdr->Cmd, msg);
                break;

            case SUB_MSG_B2BCNF_SUBSCRIBE_RESP:
            case SUB_MSG_B2BCNF_NOTIFY:
            case SUB_MSG_B2BCNF_TERMINATE:
                sub_process_b2bcnf_msg(syshdr->Cmd, msg);
                break;

            case SUB_MSG_FEATURE_SUBSCRIBE_RESP:
            case SUB_MSG_FEATURE_NOTIFY:
            case SUB_MSG_FEATURE_TERMINATE:
                sub_process_feature_msg(syshdr->Cmd, msg);
                break;

            case SUB_MSG_KPML_SUBSCRIBE:
            case SUB_MSG_KPML_TERMINATE:
            case SUB_MSG_KPML_NOTIFY_ACK:
            case SUB_MSG_KPML_SUBSCRIBE_TIMER:
            case SUB_MSG_KPML_DIGIT_TIMER:
                kpml_process_msg(syshdr->Cmd, msg);
                break;

            case REG_MGR_STATE_CHANGE:
                gsm_reset();
                break;
            case THREAD_UNLOAD:
                destroy_gsm_thread();
                break;

            default:
                GSM_ERR_MSG(GSM_F_PREFIX"Unknown message\n", fname);
                break;
            }

            cprReleaseSysHeader(syshdr);
            if (release_msg == TRUE) {
                cpr_free(msg);
            }

            /* Check if there are pending messages for dcsm
             * if it in the right state perform its operation
             */
            dcsm_process_jobs();
        }
    }
}

/**
 * This function will process SUBSCRIBED feature NOTIFY messages.
 *
 * @param[in] msg - pointer to ccsip_sub_not_data_t
 *
 * @return none
 *
 * @pre (msg != NULL)
 */
static void sub_process_b2bcnf_sub_resp (ccsip_sub_not_data_t *msg)
{
    static const char fname[] = "sub_process_b2bcnf_sub_resp";

    callid_t                 call_id = CC_NO_CALL_ID;
    callid_t                other_call_id = CC_NO_CALL_ID;
    cc_causes_t            cause;

    fsmb2bcnf_get_sub_call_id_from_ccb ((fsmcnf_ccb_t *)(msg->request_id),
                                        &call_id, &other_call_id);

    if (msg->u.subs_result_data.status_code == 200 ||
        msg->u.subs_result_data.status_code == 202 )  {

        cause = CC_CAUSE_OK;

        GSM_DEBUG(DEB_F_PREFIX"B2BCNF subs response  = OK\n",
                DEB_F_PREFIX_ARGS(GSM,fname));

    } else {

        GSM_DEBUG(DEB_F_PREFIX"B2BCNF subs response  = ERROR\n",
                DEB_F_PREFIX_ARGS(GSM,fname));

        cause = CC_CAUSE_ERROR;
    }

    cc_feature_ack(CC_SRC_GSM, call_id, msg->line_id, CC_FEATURE_B2BCONF, NULL, cause);
}

/**
 * This function will process b2bcnf feature NOTIFY messages.
 *
 * @param[in] cmd - command
 * @param[in] msg - pointer to ccsip_sub_not_data_t
 *
 * @return none
 *
 * @pre (msg != NULL)
 */
static void sub_process_b2bcnf_msg (uint32_t cmd, void *msg)
{
    static const char fname[] = "sub_process_b2bcnf_msg";
    cc_feature_data_t data;
    callid_t          call_id, other_call_id = CC_NO_CALL_ID;

    fsmb2bcnf_get_sub_call_id_from_ccb((fsmcnf_ccb_t *)((ccsip_sub_not_data_t *)msg)->request_id,
                                &call_id, &other_call_id);
    switch (cmd) {
    case SUB_MSG_B2BCNF_SUBSCRIBE_RESP:
        GSM_DEBUG(DEB_F_PREFIX"B2BCNF subs response\n",
                DEB_F_PREFIX_ARGS(GSM,fname));
        sub_process_b2bcnf_sub_resp((ccsip_sub_not_data_t *)msg);
        break;

    case SUB_MSG_B2BCNF_NOTIFY:
        GSM_DEBUG(DEB_F_PREFIX"B2BCNF subs notify\n",
                DEB_F_PREFIX_ARGS(GSM,fname));
        sub_process_feature_notify((ccsip_sub_not_data_t *)msg, call_id, other_call_id);
        break;
    case SUB_MSG_B2BCNF_TERMINATE:
         /*
          * This is posted by SIP stack if it is shutting down or rolling over.
          * if so, notify b2bcnf to cleanup state machine.
          */
        GSM_DEBUG(DEB_F_PREFIX"B2BCNF subs terminate\n",
                DEB_F_PREFIX_ARGS(GSM,fname));

        data.notify.subscription = CC_SUBSCRIPTIONS_REMOTECC;
        data.notify.method = CC_RCC_METHOD_REFER;
        data.notify.data.rcc.feature = CC_FEATURE_B2BCONF;

        data.notify.cause = CC_CAUSE_ERROR;

        cc_feature(CC_SRC_GSM, call_id, 0, CC_FEATURE_NOTIFY, &data);

        break;
    default:
        GSM_DEBUG(DEB_F_PREFIX"B2BCNF subs unknown event\n",
                DEB_F_PREFIX_ARGS(GSM,fname));
         break;
    }
}

/**
 * This function will process SUBSCRIBED feature NOTIFY messages.
 *
 * @param[in] cmd - command
 * @param[in] msg - pointer to ccsip_sub_not_data_t
 *
 * @return none
 *
 * @pre (msg != NULL)
 */
static void sub_process_feature_msg (uint32_t cmd, void *msg)
{
    callid_t   call_id;
    cc_feature_ack_t temp_msg;

     switch (cmd) {
     case SUB_MSG_FEATURE_SUBSCRIBE_RESP:
         /*
          * if the response in non-2xx final, we should reset the active feature.
          */
         call_id = (callid_t)(((ccsip_sub_not_data_t *)msg)->request_id);
         if (((ccsip_sub_not_data_t *)msg)->u.subs_result_data.status_code > 299) {
             memset(&temp_msg, 0, sizeof(temp_msg));
             temp_msg.msg_id     = CC_MSG_FEATURE_ACK;
             temp_msg.src_id     = CC_SRC_GSM;
             temp_msg.call_id    = call_id;
             fim_process_event((void *)&temp_msg, FALSE);
         }
         break;
     case SUB_MSG_FEATURE_NOTIFY:
         call_id = (callid_t)(((ccsip_sub_not_data_t *)msg)->request_id);
         sub_process_feature_notify((ccsip_sub_not_data_t *)msg, call_id,
                        CC_NO_CALL_ID);
         break;
     case SUB_MSG_FEATURE_TERMINATE:
         /*
          * This is posted by SIP stack if it is shutting down or rolling over.
          * if so, sip stack already cleaned up the subscription. so do nothing.
          */
         break;
     }
}

/**
 * This function will process SUBSCRIBED feature NOTIFY messages.
 *
 * @param[in] msg - pointer to ccsip_sub_not_data_t
 *
 * @return none
 *
 * @pre (msg != NULL)
 */
static void sub_process_feature_notify (ccsip_sub_not_data_t *msg, callid_t call_id,
                                        callid_t other_call_id)
{
    static const char fname[] = "sub_process_feature_notify";
    ccsip_event_data_t      *ev_data;
    cc_feature_ack_t temp_msg;


    /*
     * send response to NOTIFY.
     */
    (void)sub_int_notify_ack(msg->sub_id, SIP_STATUS_SUCCESS, msg->u.notify_ind_data.cseq);

    /*
     * if the subscription state is terminated, clean up the subscription.
     */
    if (msg->u.notify_ind_data.subscription_state == SUBSCRIPTION_STATE_TERMINATED) {
        /*
         * post SIPSPI_EV_CC_SUBSCRIPTION_TERMINATED.
         * do not force SUB/NOT mgr to cleanup SCB immediately, because we may have to handle digest
         * challenges to terminating SUBSCRIBE sent.
         */
       (void)sub_int_subscribe_term(msg->sub_id, FALSE, msg->request_id, CC_SUBSCRIPTIONS_REMOTECC);

    }
    ev_data     = msg->u.notify_ind_data.eventData;
    msg->u.notify_ind_data.eventData = NULL;
    if (ev_data == NULL) {
        GSM_ERR_MSG(DEB_F_PREFIX"No body in the NOTIFY message\n",
                    DEB_F_PREFIX_ARGS(GSM, fname));
        /*
         * if (no content & subscription state is TERMINATED
         * then reset active_feature to NONE.
         */
        if (msg->u.notify_ind_data.subscription_state == SUBSCRIPTION_STATE_TERMINATED) {
            memset(&temp_msg, 0, sizeof(temp_msg));
            temp_msg.msg_id     = CC_MSG_FEATURE_ACK;
            temp_msg.src_id     = CC_SRC_GSM;
            temp_msg.call_id    = call_id;
            fim_process_event((void *)&temp_msg, FALSE);
        }
        return;
    }

    // other types of event data is not supported as of now.
    free_event_data(ev_data);
}

/*
 * return TRUE if GSM is considered idle,
 *   currently this means lsm is idle
 * FALSE otherwise.
 */
boolean
gsm_is_idle (void)
{
    if (lsm_is_phone_idle()) {
        return (TRUE);
    }
    return (FALSE);
}

/*
 *  Function: destroy_gsm_thread
 *  Description:  shutdown gsm and kill gsm thread
 *  Parameters:   none
 *  Returns: none
 */
void destroy_gsm_thread()
{
    static const char fname[] = "destroy_gsm_thread";
    DEF_DEBUG(DEB_F_PREFIX"Unloading GSM and destroying GSM thread\n",
        DEB_F_PREFIX_ARGS(SIP_CC_INIT, fname));
    gsm_shutdown();
    dp_shutdown();
    kpml_shutdown();
    (void) cprDestroyThread(gsm_thread);
}
