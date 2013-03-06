/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "cpr_types.h"
#include "cpr_stdlib.h"
#include "cpr_stdio.h"
#include "phntask.h"
#include "fsm.h"
#include "fim.h"
#include "lsm.h"
#include "sm.h"
#include "gsm.h"
#include "ccapi.h"
#include "phone_debug.h"
#include "debug.h"
#include "text_strings.h"
#include "sip_interface_regmgr.h"
#include "resource_manager.h"
#include "singly_link_list.h"
#include "platform_api.h"

#define CAC_FAILURE_TIMEOUT 5
cc_int32_t g_cacDebug = 0;

/* CAC key */
typedef struct {
    callid_t  call_id;
} cac_key_t;


typedef enum {
    FSM_CAC_IDLE = 0,
    FSM_CAC_REQ_PENDING = 1,
    FSM_CAC_REQ_RESP = 2
} fsm_cac_state_e;

/* CAC structure to hold the data
 */
typedef struct cac_data_t {
    void                *msg_ptr;
    callid_t            call_id;
    void                *cac_fail_timer;
    fsm_cac_state_e     cac_state;
    uint32_t            sessions;
} cac_data_t;

static sll_handle_t s_cac_list = NULL;



/*
 *      Function responsible for searching the list waiting for
 *  bandwidth allocation.
 *
 *  @param cac_data_t *key_p - pointer to the key.
 *  @param cac_data_t *cac_data - cac data.
 *
 *  @return void
 *
 */
static sll_match_e
fsm_cac_match_call_id (cac_data_t *key_p, cac_data_t *cac_data)
{
    if (cac_data->call_id == key_p->call_id) {

        return SLL_MATCH_FOUND;
    }

    return SLL_MATCH_NOT_FOUND;

}

/*
 *      Function responsible to create new data information
 *  for cac.
 *
 *  @param none.
 *
 *  @return cac_data_t *
 *
 */
static cac_data_t *
fsm_get_new_cac_data (void)
{
    static const char *fname="fsm_get_new_cac_data";
    cac_data_t *cac_mem;

    cac_mem = (cac_data_t *) cpr_malloc(sizeof(cac_data_t));

    if (cac_mem == NULL) {
        CAC_ERROR(CAC_F_PREFIX"No memory for CAC data.\n",
                DEB_F_PREFIX_ARGS("CAC", fname));
        return (NULL);
    }

    memset(cac_mem, 0, sizeof(cac_data_t));
    return (cac_mem);
}

/**
 *
 * Release all cac related data. This includes timer, cac_data
 * and message buffers
 *
 * @param cac_data       cac data structure
 *
 * @return  none.
 *
 * @pre     (cac_data not_eq NULL)
 */

static void
fsm_clear_cac_data (cac_data_t *cac_data)
{

    if (cac_data->cac_fail_timer) {
        (void) cprCancelTimer(cac_data->cac_fail_timer);

        (void) cprDestroyTimer(cac_data->cac_fail_timer);
    }

    (void) sll_remove(s_cac_list, cac_data);

    fim_free_event(cac_data->msg_ptr);

    /* Release buffer too */
    cpr_free(cac_data->msg_ptr);

    cpr_free(cac_data);

}

/**
 *
 * Notifies the SIP stack and UI that CAC has failed.
 *
 * @param cac_data       cac data structure
 *
 * @return  none.
 *
 * @pre     (cac_data not_eq NULL)
 */

static void fsm_cac_notify_failure (cac_data_t *cac_data)
{
    const char fname[] = "fsm_cac_notify_failure";
    cc_setup_t     *msg = (cc_setup_t *) cac_data->msg_ptr;
    cc_msgs_t       msg_id   = msg->msg_id;
    callid_t        call_id  = msg->call_id;
    line_t          line     = msg->line;
    int             event_id = msg_id;
    cc_srcs_t       src_id  = msg->src_id;

    /* Notify UI about the failure */
    lsm_ui_display_notify_str_index(STR_INDEX_NO_BAND_WIDTH);

    /* Send response from network side regarding the failure */
    if (event_id == CC_MSG_SETUP &&
            src_id == CC_SRC_SIP) {
        DEF_DEBUG(DEB_F_PREFIX"Send CAC failure to SIP %d.\n",
                    DEB_F_PREFIX_ARGS("CAC", fname), cac_data->call_id);
        cc_int_release(CC_SRC_GSM, CC_SRC_SIP, call_id, line,
                       CC_CAUSE_CONGESTION, NULL, NULL);
    } else {
        /* If the cac failed, GSM is not spinning yet, so just send the
         * information to UI in this case. Other case, where GSM receives event
         * will send the information from GSM.
         * If the UI is not cleaned up, session infomation is not cleared.
         */
        ui_call_state(evOnHook, line, call_id, CC_CAUSE_CONGESTION);
    }

}

/**
 *
 * Initialize the cac timer. This timer is responsible for cleanup if the
 * cac response is not received from lower layer.
 *
 * @param cac_data       cac data structure
 *        timeout        specify the time out in sec
 *
 * @return  true if the timer is created scuccessfully.
 *          false if the timer is not created.
 *
 * @pre     (cac_data not_eq NULL)
 */

static boolean
fsm_init_cac_failure_timer(cac_data_t *cac_data, uint32_t timeout)
{
    const char fname[] = "fsm_init_cac_failure_timer";

    CAC_DEBUG(DEB_F_PREFIX"cac_data call_id=%x\n",
              DEB_F_PREFIX_ARGS("CAC", fname),
              cac_data->call_id);

    cac_data->cac_fail_timer =
        cprCreateTimer("CAC failure timer", GSM_CAC_FAILURE_TIMER, TIMER_EXPIRATION,
                       gsm_msgq);

    if (cac_data->cac_fail_timer == NULL) {
        CAC_ERROR(CAC_F_PREFIX"CAC Timer allocation failed.\n",
                                    DEB_F_PREFIX_ARGS("CAC", fname));
        return(FALSE);
    }

    (void) cprStartTimer(cac_data->cac_fail_timer, timeout * 1000,
                         (void *)(long)cac_data->call_id);

    return(TRUE);
}

/**
 *
 * Serches through cac link list and returns cac_data
 * based on call_id. This search is a singly link list search.
 *
 * @param call_id       call_id of the call
 *
 * @return  cac_data if found in the list
 *          NULL     if there is no cac_data
 *
 * @pre     (call_id not_eq CC_NO_CALL_ID)
 */

static cac_data_t *
fsm_cac_get_data_by_call_id (callid_t call_id)
{
    const char fname[] = "fsm_cac_get_data_by_call_id";
    cac_data_t *cac_data;

    cac_data = (cac_data_t *) sll_next(s_cac_list, NULL);

    while (cac_data != NULL) {

        if (cac_data->call_id == call_id) {
            CAC_DEBUG(DEB_F_PREFIX"cac_data found call_id=%x\n",
              DEB_F_PREFIX_ARGS("CAC", fname),
              cac_data->call_id);
            return(cac_data);
        }

        cac_data = (cac_data_t *) sll_next(s_cac_list, cac_data);

    }

    CAC_DEBUG(DEB_F_PREFIX"cac_data NOT found.\n",
        DEB_F_PREFIX_ARGS("CAC", fname));
    return(NULL);
}

/**
 *
 * Initialize cac module by enabling debugs and creating a cac list.
 *
 * @param void
 *
 * @return  void
 *
 * @pre     (NULL)
 */
void fsm_cac_init (void)
{
    const char fname[] = "fsm_cac_init";


    /* allocate and initialize cac list */
    s_cac_list = sll_create((sll_match_e(*)(void *, void *))
                            fsm_cac_match_call_id);

    if (s_cac_list == NULL) {
        CAC_ERROR(CAC_F_PREFIX"CAC list creation failed.\n",
                                    DEB_F_PREFIX_ARGS("CAC", fname));

    }
}

/**
 *
 * clears all the entries in the cac list
 *
 * @param void
 *
 * @return  void
 *
 * @pre     (NULL)
 */
void fsm_cac_clear_list (void)
{
    const char fname[] = "fsm_cac_clear_list";
    cac_data_t *cac_data;
    cac_data_t *prev_cac_data;

    DEF_DEBUG(DEB_F_PREFIX"Clear all pending CAC dat.\n",
                DEB_F_PREFIX_ARGS("CAC", fname));

    cac_data = (cac_data_t *) sll_next(s_cac_list, NULL);

    while (cac_data != NULL) {

        prev_cac_data = cac_data;
        cac_data = (cac_data_t *) sll_next(s_cac_list, cac_data);

        fsm_cac_notify_failure(prev_cac_data);
        fsm_clear_cac_data(prev_cac_data);
    }

}

/**
 *
 * Shutdown cac module, clears all the pending cac requests
 *
 * @param void
 *
 * @return  void
 *
 * @pre     (NULL)
 */
void fsm_cac_shutdown (void)
{

    fsm_cac_clear_list();

    sll_destroy(s_cac_list);

    s_cac_list = NULL;
}

/**
 *
 * Check if there are pending CAC requests
 *
 * @param none
 *
 * @return  cac_data returns first pending request.
 *
 * @pre     (NULL)
 */
static cac_data_t *
fsm_cac_check_if_pending_req (void)
{
    cac_data_t *cac_data;

    cac_data = (cac_data_t *) sll_next(s_cac_list, NULL);

    while (cac_data != NULL) {

        if (cac_data->cac_state == FSM_CAC_REQ_PENDING ||
                cac_data->cac_state == FSM_CAC_IDLE) {

            return(cac_data);
        }

        cac_data = (cac_data_t *) sll_next(s_cac_list, cac_data);

    }

    return(NULL);
}

/**
 *
 * Check if there are pending CAC requests
 *
 * @param none
 *
 * @return  cac_data returns first pending request.
 *
 * @pre     (NULL)
 */
static cc_causes_t
fsm_cac_process_bw_allocation (cac_data_t *cac_data)
{
    const char fname[] = "fsm_cac_process_bw_allocation";

    if (lsm_allocate_call_bandwidth(cac_data->call_id, cac_data->sessions) ==
            CC_CAUSE_CONGESTION) {

        DEF_DEBUG(DEB_F_PREFIX"CAC Allocation failed.\n",
                DEB_F_PREFIX_ARGS("CAC", fname));

        fsm_cac_notify_failure(cac_data);

        fsm_clear_cac_data(cac_data);

        return(CC_CAUSE_CONGESTION);
    }

    cac_data->cac_state = FSM_CAC_REQ_PENDING;

    return(CC_CAUSE_OK);
}

/**
 *
 * Check if there are pending CAC requests
 *
 * @param call_id request a cac for this call_id
 *       sessions  number of sessions in the request
 *        msg    ccapi msg, that is held to process
 *              till cac response is received.
 *
 * @return  CC_CAUSE_BW_OK if the bandwidth is received.
 *          CC_CAUSE_Ok Call returned successfully, not sure about BW yet
 *          CC_CAUSE_ERROR: Call returned with failure.
 *
 * @pre     (NULL)
 */
cc_causes_t
fsm_cac_call_bandwidth_req (callid_t call_id, uint32_t sessions,
                            void *msg)
{
    const char fname[] = "fsm_cac_call_bandwidth_req";
    cac_data_t *cac_data, *cac_pend_data;

    /* If wlan not connected return OK */
    cac_data = fsm_get_new_cac_data();

    if (cac_data == NULL) {

        return(CC_CAUSE_CONGESTION);
    }

    cac_data->msg_ptr = msg;
    cac_data->call_id = call_id;
    cac_data->cac_state = FSM_CAC_IDLE;
    cac_data->sessions = sessions;

    fsm_init_cac_failure_timer(cac_data, CAC_FAILURE_TIMEOUT);

    /* Make sure there is no pending requests before submitting
     * another one
     */
    if ((cac_pend_data = fsm_cac_check_if_pending_req()) == NULL) {

        /*
        * Make sure sufficient bandwidth available to make a outgoing call. This
        * should be done before allocating other resources.
        */
        DEF_DEBUG(DEB_F_PREFIX"CAC request for %d sessions %d.\n",
                DEB_F_PREFIX_ARGS("CAC", fname), call_id, sessions);

        if (fsm_cac_process_bw_allocation(cac_data) == CC_CAUSE_CONGESTION) {

            return(CC_CAUSE_CONGESTION);
        }

        cac_data->cac_state = FSM_CAC_REQ_PENDING;

    } else if (cac_pend_data->cac_state == FSM_CAC_IDLE) {

        if (fsm_cac_process_bw_allocation(cac_pend_data) ==
                    CC_CAUSE_CONGESTION) {

            /* Clear all remaining data */
            fsm_cac_clear_list();

            return(CC_CAUSE_CONGESTION);
        }

    }

    (void) sll_append(s_cac_list, cac_data);

    return(CC_CAUSE_OK);

}

/**
 *
 * This is called by gsm to cleanup the cac data. If there are any
 * pending CAC requests and far end cancels the call, the pending
 * request has to be canceled.
 *
 * @param call_id - call_id of the request
 *
 * @return  none.
 *
 * @pre     (NULL)
 */
void fsm_cac_call_release_cleanup (callid_t call_id)
{
    cac_data_t *cac_data;

    cac_data = fsm_cac_get_data_by_call_id(call_id);

    if (cac_data) {

        sll_remove(s_cac_list, cac_data);

        fsm_clear_cac_data(cac_data);
    }

}


/**
 *
 * Called when the bandwidth response with available bw is received. This
 * also process held ccapi messages through fim event chain
 *
 * @param none
 *
 * @return  CC_CAUSE_NO_RESOURCE No bandwidth
 *          CC_CAUSE_OK if ok
 *
 *
 * @pre     (NULL)
 */

cc_causes_t
fsm_cac_process_bw_avail_resp (void)
{
    const char      fname[] = "fsm_cac_process_bw_avail_resp";
    cac_data_t      *cac_data = NULL;
    cac_data_t      *next_cac_data = NULL;


    cac_data = (cac_data_t *) sll_next(s_cac_list, NULL);

    if (cac_data != NULL) {

        switch (cac_data->cac_state) {
        default:
        case FSM_CAC_IDLE:
            DEF_DEBUG(DEB_F_PREFIX"No Pending CAC request.\n",
                DEB_F_PREFIX_ARGS("CAC", fname));
            /*
            * Make sure sufficient bandwidth available to make a outgoing call. This
            * should be done before allocating other resources.
            */
            if (fsm_cac_process_bw_allocation(cac_data) == CC_CAUSE_CONGESTION) {

                sll_remove(s_cac_list, cac_data);

                return(CC_CAUSE_NO_RESOURCE);
            }


            break;
        case FSM_CAC_REQ_PENDING:

            next_cac_data = (cac_data_t *) sll_next(s_cac_list, cac_data);
            sll_remove(s_cac_list, cac_data);

            /* Request for the next bandwidth */
            DEF_DEBUG(DEB_F_PREFIX"Process pending responses %d.\n",
                DEB_F_PREFIX_ARGS("CAC", fname), cac_data->call_id);

            /* Let GSM process completed request */
            fim_process_event(cac_data->msg_ptr, TRUE);

            fsm_clear_cac_data(cac_data);

            if (next_cac_data != NULL) {
                /*
                 * Make sure sufficient bandwidth available to make a outgoing call. This
                 * should be done before allocating other resources.
                 */
                DEF_DEBUG(DEB_F_PREFIX"Requesting next allocation %d.\n",
                    DEB_F_PREFIX_ARGS("CAC", fname), next_cac_data->call_id);

                if (fsm_cac_process_bw_allocation(next_cac_data) ==
                                CC_CAUSE_CONGESTION) {

                    /* If the next data was in idle state and the request fialed
                     * then clean up the remaining list
                     */
                    if (next_cac_data->cac_state == FSM_CAC_IDLE) {
                        /* Clear all remaining data */
                        fsm_cac_clear_list();
                    } else {

                        sll_remove(s_cac_list, next_cac_data);
                    }

                    return(CC_CAUSE_NO_RESOURCE);
                }

            }

            break;
        }

    }

    return(CC_CAUSE_NO_RESOURCE);

}

/**
 *
 * Called when the bandwidth response with failed bw is received. This
 * also process held ccapi messages through fim event chain
 *
 * @param none
 *
 * @return  CC_CAUSE_NO_RESOURCE No bandwidth
 *          CC_CAUSE_OK if ok
 *
 *
 * @pre     (NULL)
 */
cc_causes_t
fsm_cac_process_bw_failed_resp (void)
{
    const char      fname[] = "fsm_cac_process_bw_avail_resp";
    cac_data_t      *cac_data = NULL;
    cac_data_t      *next_cac_data = NULL;


    cac_data = (cac_data_t *) sll_next(s_cac_list, NULL);

    if (cac_data != NULL) {

        switch (cac_data->cac_state) {
        default:
        case FSM_CAC_IDLE:
            DEF_DEBUG(DEB_F_PREFIX"No Pending request.\n",
                DEB_F_PREFIX_ARGS("CAC", fname));
            /*
            * Make sure sufficient bandwidth available to make a outgoing call. This
            * should be done before allocating other resources.
            */
            if (fsm_cac_process_bw_allocation(cac_data) == CC_CAUSE_CONGESTION) {

                sll_remove(s_cac_list, cac_data);

                return(CC_CAUSE_NO_RESOURCE);
            }

            break;
        case FSM_CAC_REQ_PENDING:

            next_cac_data = (cac_data_t *) sll_next(s_cac_list, cac_data);

            sll_remove(s_cac_list, cac_data);

            /* Request for the next bandwidth */
            DEF_DEBUG(DEB_F_PREFIX"Process pending responses even after failure.\n",
                DEB_F_PREFIX_ARGS("CAC", fname));

            /* Let GSM process completed request */
            fsm_cac_notify_failure(cac_data);

            fsm_clear_cac_data(cac_data);

            if (next_cac_data != NULL) {

                /*
                 * Make sure sufficient bandwidth available to make a outgoing call. This
                 * should be done before allocating other resources.
                 */
                if (fsm_cac_process_bw_allocation(next_cac_data) == CC_CAUSE_CONGESTION) {

                    /* If the next data was in idle state and the request fialed
                     * then clean up the remaining list
                     */
                    if (next_cac_data->cac_state == FSM_CAC_IDLE) {
                        /* Clear all remaining data */
                        fsm_cac_clear_list();
                    } else {

                        sll_remove(s_cac_list, next_cac_data);
                    }

                    return(CC_CAUSE_NO_RESOURCE);
                }

            }

            break;
        }

    }

    return(CC_CAUSE_NO_RESOURCE);
}

/**
 *
 * Process time-out event. This cause cac data to send failure notifications.
 *
 * @param   void *tmr_data - timer data
 *
 * @return  none
 *
 *
 * @pre     (NULL)
 */
void
fsm_cac_process_bw_fail_timer (void *tmr_data)
{
    const char      fname[] = "fsm_cac_process_bw_fail_timer";

    DEF_DEBUG(DEB_F_PREFIX"CAC request timedout %d.\n",
                    DEB_F_PREFIX_ARGS("CAC", fname), (callid_t)(long)tmr_data);

    /* Time-out causes same set of processing as bw failure
     */
    fsm_cac_process_bw_failed_resp();

}

