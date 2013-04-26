/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "cpr_types.h"
#include "cpr_stdlib.h"
#include "cpr_stdio.h"
#include "cpr_string.h"
#include "fsm.h"
#include "fim.h"
#include "lsm.h"
#include "sm.h"
#include "ccapi.h"
#include "phone_debug.h"
#include "text_strings.h"
#include "debug.h"
#include "config.h"
#include "uiapi.h"
#include "phntask.h"
#include "regmgrapi.h"
#include "subapi.h"
#include "rcc_int_types.h"

static fsmcnf_ccb_t *fsmb2bcnf_ccbs;

typedef enum {
    FSMB2BCNF_S_MIN = -1,
    FSMB2BCNF_S_IDLE,
    FSMB2BCNF_S_ACTIVE,
    FSMB2BCNF_S_MAX
} fsmb2bcnf_states_t;

static const char *fsmb2bcnf_state_names[] = {
    "IDLE",
    "ACTIVE"
};


static sm_rcs_t fsmb2bcnf_ev_idle_feature(sm_event_t *event);
static sm_rcs_t fsmb2bcnf_ev_active_release(sm_event_t *event);
static sm_rcs_t fsmb2bcnf_ev_active_release_complete(sm_event_t *event);
static sm_rcs_t fsmb2bcnf_ev_active_feature(sm_event_t *event);
static sm_rcs_t fsmb2bcnf_ev_active_feature_ack(sm_event_t *event);
static sm_rcs_t fsmb2bcnf_ev_active_onhook(sm_event_t *event);

static sm_function_t fsmb2bcnf_function_table[FSMB2BCNF_S_MAX][CC_MSG_MAX] =
{
/* FSMB2BCNF_S_IDLE ------------------------------------------------------------ */
    {
    /* FSMB2BCNF_E_SETUP            */ NULL,
    /* FSMB2BCNF_E_SETUP_ACK        */ NULL,
    /* FSMB2BCNF_E_PROCEEDING       */ NULL,
    /* FSMB2BCNF_E_ALERTING         */ NULL,
    /* FSMB2BCNF_E_CONNECTED        */ NULL,
    /* FSMB2BCNF_E_CONNECTED_ACK    */ NULL,
    /* FSMB2BCNF_E_RELEASE          */ NULL,
    /* FSMB2BCNF_E_RELEASE_COMPLETE */ NULL,
    /* FSMB2BCNF_E_FEATURE          */ fsmb2bcnf_ev_idle_feature,
    /* FSMB2BCNF_E_FEATURE_ACK      */ NULL,
    /* FSMB2BCNF_E_OFFHOOK          */ NULL,
    /* FSMB2BCNF_E_ONHOOK           */ NULL,
    /* FSMB2BCNF_E_LINE             */ NULL,
    /* FSMB2BCNF_E_DIGIT_BEGIN      */ NULL,
    /* FSMB2BCNF_E_DIGIT            */ NULL,
    /* FSMB2BCNF_E_DIALSTRING       */ NULL,
    /* FSMB2BCNF_E_MWI              */ NULL,
    /* FSMB2BCNF_E_SESSION_AUDIT    */ NULL
    },

/* FSMB2BCNF_S_ACTIVE --------------------------------------------------- */
    {
    /* FSMB2BCNF_E_SETUP            */ NULL,
    /* FSMB2BCNF_E_SETUP_ACK        */ NULL,
    /* FSMB2BCNF_E_PROCEEDING       */ NULL,
    /* FSMB2BCNF_E_ALERTING         */ fsmb2bcnf_ev_active_feature,
    /* FSMB2BCNF_E_CONNECTED        */ NULL,
    /* FSMB2BCNF_E_CONNECTED_ACK    */ NULL,
    /* FSMB2BCNF_E_RELEASE          */ fsmb2bcnf_ev_active_release,
    /* FSMB2BCNF_E_RELEASE_COMPLETE */ fsmb2bcnf_ev_active_release_complete,
    /* FSMB2BCNF_E_FEATURE          */ fsmb2bcnf_ev_active_feature,
    /* FSMB2BCNF_E_FEATURE_ACK      */ fsmb2bcnf_ev_active_feature_ack,
    /* FSMB2BCNF_E_OFFHOOK          */ NULL,
    /* FSMB2BCNF_E_ONHOOK           */ fsmb2bcnf_ev_active_onhook,
    /* FSMB2BCNF_E_LINE             */ NULL,
    /* FSMB2BCNF_E_DIGIT_BEGIN      */ NULL,
    /* FSMB2BCNF_E_DIGIT            */ NULL,
    /* FSMB2BCNF_E_DIALSTRING       */ NULL,
    /* FSMB2BCNF_E_MWI              */ NULL,
    /* FSMB2BCNF_E_SESSION_AUDIT    */ NULL
    }
};

static sm_table_t g_fsmb2bcnf_sm_table;
sm_table_t *pfsmb2bcnf_sm_table = &g_fsmb2bcnf_sm_table;

const char *
fsmb2bcnf_state_name (int state)
{
    if ((state <= FSMB2BCNF_S_MIN) || (state >= FSMB2BCNF_S_MAX)) {
        return (get_debug_string(GSM_UNDEFINED));
    }

    return (fsmb2bcnf_state_names[state]);
}


static int
fsmb2bcnf_get_new_b2bcnf_id (void)
{
    static int b2bcnf_id = FSM_NO_ID;

    if (++b2bcnf_id < FSM_NO_ID) {
        b2bcnf_id = 1;
    }

    return (b2bcnf_id);
}


static void
fsmb2bcnf_init_ccb (fsmcnf_ccb_t *ccb)
{
    if (ccb != NULL) {
        ccb->cnf_id      = FSM_NO_ID;
        ccb->cnf_call_id = CC_NO_CALL_ID;
        ccb->cns_call_id = CC_NO_CALL_ID;
        ccb->cnf_line    = CC_NO_LINE;
        ccb->cns_line    = CC_NO_LINE;
        ccb->bridged     = FALSE;
        ccb->active      = FALSE;
        ccb->cnf_ftr_ack = FALSE;
        ccb->cnf_orig    = CC_SRC_MIN;
    }
}

/**
 *
 * Get active trasnfer state machine information (in active state).
 *
 * @param none
 *
 * @return  fsm_fcb_t if there is a active trasnfer pending
 *          else NULL
 *
 * @pre     (none)
 */

fsm_fcb_t *fsmb2bcnf_get_active_cnf(void)
{
    fsm_fcb_t    *fcb;
    fsmcnf_ccb_t *b2bccb;

    FSM_FOR_ALL_CBS(b2bccb, fsmb2bcnf_ccbs, FSMCNF_MAX_CCBS) {
        fcb = fsm_get_fcb_by_call_id_and_type(b2bccb->cnf_call_id,
                                               FSM_TYPE_B2BCNF);
        if (fcb && fcb->state == FSMB2BCNF_S_ACTIVE) {
            return(fcb);
        }
    }

    return(NULL);
}

static fsmcnf_ccb_t *
fsmb2bcnf_get_ccb_by_b2bcnf_id (int b2bcnf_id)
{
    fsmcnf_ccb_t   *ccb;
    fsmcnf_ccb_t   *ccb_found = NULL;

    FSM_FOR_ALL_CBS(ccb, fsmb2bcnf_ccbs, FSMCNF_MAX_CCBS) {
        if (ccb->cnf_id == b2bcnf_id) {
            ccb_found = ccb;

            break;
        }
    }

    return (ccb_found);
}


/*
 *  Function: fsmb2bcnf_get_new_b2bcnf_context
 *
 *  Parameters:
 *      b2bcnf_call_id: call_id for the call initiating the conference
 *
 *  Description: This function creates a new conference context by:
 *               - getting a free ccb
 *               - creating new b2bcnf_id and cns_call_id
 *
 *  Returns: ccb
 *
 */
static fsmcnf_ccb_t *
fsmb2bcnf_get_new_b2bcnf_context (callid_t b2bcnf_call_id, line_t line)
{
    const char fname[] = "fsmb2bcnf_get_new_b2bcnf_context";
    fsmcnf_ccb_t *ccb;

    ccb = fsmb2bcnf_get_ccb_by_b2bcnf_id(FSM_NO_ID);
    if (ccb != NULL) {
        ccb->cnf_id      = fsmb2bcnf_get_new_b2bcnf_id();
        ccb->cnf_call_id = b2bcnf_call_id;
        ccb->cnf_line    = line;
        ccb->cns_line    = line;
        ccb->cns_call_id = cc_get_new_call_id();

        FSM_DEBUG_SM(get_debug_string(FSMB2BCNF_DBG_PTR), ccb->cnf_id,
                     ccb->cnf_call_id, ccb->cns_call_id, fname, ccb);
    } else {

        GSM_DEBUG_ERROR(GSM_F_PREFIX"Failed to get new b2bccb.", fname);
    }

    return (ccb);
}


static fsmcnf_ccb_t *
fsmb2bcnf_get_ccb_by_call_id (callid_t call_id)
{
    fsmcnf_ccb_t *ccb;
    fsmcnf_ccb_t *ccb_found = NULL;

    FSM_FOR_ALL_CBS(ccb, fsmb2bcnf_ccbs, FSMCNF_MAX_CCBS) {
        if ((ccb->cnf_call_id == call_id) || (ccb->cns_call_id == call_id)) {
            ccb_found = ccb;

            break;
        }
    }

    return (ccb_found);
}


static void
fsmb2bcnf_update_b2bcnf_context (fsmcnf_ccb_t *ccb, callid_t old_call_id,
                                 callid_t new_call_id)
{
    const char fname[] = "fsmb2bcnf_update_b2bcnf_context";

    if (ccb != NULL) {
        if (old_call_id == ccb->cnf_call_id) {
            ccb->cnf_call_id = new_call_id;
        } else if (old_call_id == ccb->cns_call_id) {
            ccb->cns_call_id = new_call_id;
        }

        FSM_DEBUG_SM(get_debug_string(FSMB2BCNF_DBG_PTR), ccb->cnf_id,
                     ccb->cnf_call_id, ccb->cns_call_id, fname, ccb);
    }
}

/*
 *      Function to get line number of other call associated in
 *      transfer.
 *
 *  @param xcb and call_id.
 *
 *  @return void
 *
 */
line_t
fsmb2bcnf_get_other_line (fsmcnf_ccb_t *ccb, callid_t call_id)
{
    line_t other_line = CC_NO_LINE;

    if (ccb != NULL) {
        if (ccb->cnf_call_id == call_id) {
            other_line = ccb->cns_line;
        } else if (ccb->cns_call_id == call_id) {
            other_line = ccb->cnf_line;
        }
    }

    return (other_line);
}

static callid_t
fsmb2bcnf_get_other_call_id (fsmcnf_ccb_t *ccb, callid_t call_id)
{
    callid_t other_call_id = CC_NO_CALL_ID;

    if (ccb != NULL) {
        if (ccb->cnf_call_id == call_id) {
            other_call_id = ccb->cns_call_id;
        } else if (ccb->cns_call_id == call_id) {
            other_call_id = ccb->cnf_call_id;
        }
    }

    return (other_call_id);
}

/*
 *  Function: fsmb2bcnf_remove_fcb
 *
 *  Parameters:
 *      b2bcnf_id:  b2bcnf_id for the conference
 *      call_id: call_id that identifies the fcb to be removed
 *
 *  Description: This function will remove the fcb identified by the given
 *               call_id from the ccb. And the function will free the ccb
 *               if both fcbs have been removed.
 *
 *  Returns: none
 *
 *  Note: This is a helper function for fsmb2bcnf_cleanup. It allows fsmb2bcnf_cleanup
 *        to cleanup one fcb (one call involved in the conference) independent
 *        of the other involved fcb.
 */
static void
fsmb2bcnf_remove_fcb (fsm_fcb_t *fcb, callid_t call_id)
{
    fsmcnf_ccb_t *ccb = fcb->b2bccb;

    if (ccb != NULL) {
        fsmb2bcnf_update_b2bcnf_context(ccb, call_id, CC_NO_CALL_ID);

        /*
         * Free the ccb if both fcb references have been removed.
         */
        if ((ccb->cnf_call_id == CC_NO_CALL_ID) &&
            (ccb->cns_call_id == CC_NO_CALL_ID)) {
            fsmb2bcnf_init_ccb(ccb);
        }
    }
}


static void
fsmb2bcnf_cleanup (fsm_fcb_t *fcb, int fname, boolean both)
{
    fsm_fcb_t      *other_fcb = NULL;
    callid_t        call_id       = fcb->call_id;
    callid_t        other_call_id = CC_NO_CALL_ID;
    line_t          other_line;

    other_call_id = fsmb2bcnf_get_other_call_id(fcb->b2bccb, call_id);
    other_line    = fsmb2bcnf_get_other_line(fcb->b2bccb, call_id);

    if (other_call_id != CC_NO_CALL_ID) {
        other_fcb = fsm_get_fcb_by_call_id_and_type(other_call_id,
                                                    FSM_TYPE_B2BCNF);
    }

    if (fcb->b2bccb && (call_id == fcb->b2bccb->cnf_call_id)) {

        if (other_call_id != CC_NO_CALL_ID) {
            /*
             * Not clearing consulation call, so change consultation
             * call attribute to display connected softkey set.
             * Do not change softkey set if it is a transfer o
             */
            cc_call_attribute(other_call_id, other_line, NORMAL_CALL);
        }

    }
    /*
     * Check if the user wanted to cleanup the whole ccb.
     * If so, then we will grab the other fcb first and call this function
     * again with this other fcb. The whole ccb will be freed after this block
     * of code because both call_ids will be -1, which tells
     * fsmb2bcnf_remove_fcb to free the ccb.
     */
    if (both) {
        if (other_call_id != CC_NO_CALL_ID) {
            if (other_fcb != NULL) {
                fsmb2bcnf_cleanup(other_fcb, fname, FALSE);
            }
        }
    }
    /*
     * Remove the reference to this fcb from the ccb.
     */
    fsmb2bcnf_remove_fcb(fcb, fcb->call_id);

    /*
     * Move this fcb to the IDLE state
    */
    fsm_change_state(fcb, fname, FSMB2BCNF_S_IDLE);

    /*
     * Reset the data for this fcb. The fcb is still included in a call
     * so set the call_id and dcb values accordingly.
     */
    fsm_init_fcb(fcb, fcb->call_id, fcb->dcb, FSM_TYPE_B2BCNF);
}


void
fsmb2bcnf_free_cb (fim_icb_t *icb, callid_t call_id)
{
    fsm_fcb_t *fcb = NULL;

    if (call_id != CC_NO_CALL_ID) {
        fcb = fsm_get_fcb_by_call_id_and_type(call_id, FSM_TYPE_B2BCNF);

        if (fcb != NULL) {
            fsmb2bcnf_cleanup(fcb, __LINE__, FALSE);
            fsm_init_fcb(fcb, CC_NO_CALL_ID, FSMDEF_NO_DCB, FSM_TYPE_NONE);
        }
    }
}


/*
 * fsmb2bcnf_check_if_ok_to_setup_conf
 *
 * Description:
 *    Checks if the requested call is ok to setup conference.
 *
 * Parameters:
 *    call_id
 *
 * Returns: TRUE - if the call is ok to setup conference
 *          FALSE - other cases
 */
boolean
fsmb2bcnf_check_if_ok_to_setup_conf (callid_t call_id)
{
    fsmdef_dcb_t *dcb;

    if (call_id == CC_NO_CALL_ID) {
        return (FALSE);
    }

    dcb = fsm_get_dcb(call_id);

    if(dcb && dcb->policy == CC_POLICY_CHAPERONE
		    && dcb->is_conf_call == TRUE){
	return (FALSE);
    }

    return (TRUE);
}


/*
 * fsmb2bcnf_b2bcnf_invoke
 *
 * Description:
 *    This function implements the conference feature invocation.
 *    If the feature is already invoked and waiting for the
 *    feature ack back from the SIP stack then no action taken.
 *    Otherwise, the conf feature is invoked and state is SET.
 *
 * Parameters:
 *    fsmdef_dcb_t *dcb - dcb associated with this call
 *
 * Returns: None
 */
static void
fsmb2bcnf_cnf_invoke (callid_t call_id, callid_t target_call_id,
                         line_t line, fsmcnf_ccb_t *ccb)
{
    sipspi_msg_t subscribe_msg;
    ccsip_event_data_t *evt_data;

    /*
     * post SIPSPI_EV_CC_SUBSCRIBE to SIP stack
     */
    evt_data = (ccsip_event_data_t *)
        cpr_malloc(sizeof(ccsip_event_data_t));
    if (evt_data == NULL) {
        return;
    }
    memset(evt_data, 0, sizeof(ccsip_event_data_t));
    evt_data->type = EVENT_DATA_REMOTECC_REQUEST;
    evt_data->u.remotecc_data.line = 0;
    evt_data->u.remotecc_data.rcc_request_type = RCC_SOFTKEY_EVT;
    evt_data->u.remotecc_data.rcc_int.rcc_softkey_event_msg.softkeyevent = RCC_SOFTKEY_CONFERENCE;
    evt_data->u.remotecc_data.consult_gsm_id = target_call_id;
    evt_data->u.remotecc_data.gsm_id = call_id;

    memset(&subscribe_msg, 0, sizeof(sipspi_msg_t));
    subscribe_msg.msg.subscribe.eventPackage = CC_SUBSCRIPTIONS_REMOTECC;
    subscribe_msg.msg.subscribe.sub_id = CCSIP_SUBS_INVALID_SUB_ID;
    subscribe_msg.msg.subscribe.auto_resubscribe = TRUE;
    subscribe_msg.msg.subscribe.request_id = (long)ccb;
    subscribe_msg.msg.subscribe.duration = 60;
    subscribe_msg.msg.subscribe.subsNotCallbackTask = CC_SRC_GSM;
    subscribe_msg.msg.subscribe.subsResCallbackMsgID = SUB_MSG_B2BCNF_SUBSCRIBE_RESP;
    subscribe_msg.msg.subscribe.subsNotIndCallbackMsgID = SUB_MSG_B2BCNF_NOTIFY;
    subscribe_msg.msg.subscribe.subsTermCallbackMsgID = SUB_MSG_B2BCNF_TERMINATE;
    subscribe_msg.msg.subscribe.norefersub = FALSE;
    subscribe_msg.msg.subscribe.eventData = evt_data;
    subscribe_msg.msg.subscribe.dn_line = line;

    (void)sub_int_subscribe(&subscribe_msg);
}

/**
 *
 * Cancel b2b conference feature by sending cancel event to SIP stack.
 * This routine is used in roundtable phone.
 *
 * @param line, call_id, target_call_id, cause (implicit or explicit)
 *
 * @return  void
 *
 * @pre     (none)
 */
void
fsmb2bcnf_feature_cancel (fsmcnf_ccb_t *ccb, line_t line, callid_t call_id,
                          callid_t target_call_id,
                          cc_rcc_skey_evt_type_e cause)
{
    cc_feature_data_t data;
    fsm_fcb_t         *fcb_def;

    fcb_def = fsm_get_fcb_by_call_id_and_type(call_id, FSM_TYPE_DEF);

    if ((cause == CC_SK_EVT_TYPE_EXPLI) &&
        (fcb_def != NULL) && ((fcb_def->dcb->selected == FALSE) &&
            ((fcb_def->state == FSMDEF_S_OUTGOING_ALERTING) ||
            ((fcb_def->state == FSMDEF_S_CONNECTED) &&
            (fcb_def->dcb->spoof_ringout_requested == TRUE) &&
            (fcb_def->dcb->spoof_ringout_applied == TRUE))))) {

        cc_int_feature(CC_SRC_GSM, CC_SRC_GSM, call_id,
                           line, CC_FEATURE_END_CALL, NULL);
    }

    fcb_def = fsm_get_fcb_by_call_id_and_type(target_call_id, FSM_TYPE_DEF);

    if ((cause == CC_SK_EVT_TYPE_EXPLI) &&
        (fcb_def != NULL) && ((fcb_def->dcb->selected == FALSE) &&
            ((fcb_def->state == FSMDEF_S_OUTGOING_ALERTING) ||
            ((fcb_def->state == FSMDEF_S_CONNECTED) &&
            (fcb_def->dcb->spoof_ringout_requested == TRUE) &&
            (fcb_def->dcb->spoof_ringout_applied == TRUE))))) {

        cc_int_feature(CC_SRC_GSM, CC_SRC_GSM, target_call_id,
                           line, CC_FEATURE_END_CALL, NULL);
    }

    data.cancel.target_call_id = target_call_id;
    data.cancel.call_id = call_id;
    data.cancel.cause = cause;

    cc_int_feature(CC_SRC_GSM, CC_SRC_SIP, call_id,
                           line, CC_FEATURE_CANCEL, &data);
}

/*******************************************************************
 * event functions
 */


static sm_rcs_t
fsmb2bcnf_ev_idle_feature (sm_event_t *event)
{
    const char        *fname    = "fsmb2bcnf_ev_idle_feature";
    fsm_fcb_t         *fcb      = (fsm_fcb_t *) event->data;
    cc_feature_t      *msg      = (cc_feature_t *) event->msg;
    callid_t           call_id  = msg->call_id;
    line_t             line     = msg->line;
    cc_srcs_t          src_id   = msg->src_id;
    cc_features_t      ftr_id   = msg->feature_id;
    cc_feature_data_t *ftr_data = &(msg->data);
    fsmdef_dcb_t      *dcb      = fcb->dcb;
    callid_t           cns_call_id;
    sm_rcs_t           sm_rc    = SM_RC_CONT;
    fsmcnf_ccb_t      *ccb;
    int                free_lines;
    cc_feature_data_t  data;
    fsm_fcb_t         *other_fcb, *cns_fcb;
    fsm_fcb_t         *fcb_def;
    callid_t           other_call_id;
    line_t             newcall_line = 0;

    fsm_sm_ftr(ftr_id, src_id);

    switch (src_id) {
    case CC_SRC_RCC:
    case CC_SRC_UI:
        switch (ftr_id) {
        case CC_FEATURE_B2BCONF:
            /* Connect the existing call to active trasnfer state
             * machine. If the UI generates the event with target
             * call_id in the data then terminate the existing consulatative
             * call and link that to another call.
            */
            if (ftr_data && msg->data_valid &&
                (ftr_data->b2bconf.target_call_id != CC_NO_CALL_ID)
                && (cns_fcb = fsm_get_fcb_by_call_id_and_type(ftr_data->b2bconf.target_call_id,
                            FSM_TYPE_B2BCNF)) != NULL) {
                /*
                 * Get a new ccb and new b2bcnf id - This is the handle that will
                 * identify the b2bcnf.
                 */
                ccb = fsmb2bcnf_get_new_b2bcnf_context(call_id, line);

                if (ccb==NULL || ccb->cnf_id == FSM_NO_ID) {
                    return(SM_RC_END);
                }

                /* Conference origination id, required later. This indicates conf is
                 * because of UI or because of CTI
                 */
                ccb->cnf_orig = src_id;

                ccb->cns_call_id = ftr_data->b2bconf.target_call_id;
                fcb->b2bccb = ccb;
                cns_fcb->b2bccb = ccb;

                /* Find line information for target call.
                 */
                fcb_def = fsm_get_fcb_by_call_id_and_type(call_id, FSM_TYPE_DEF);
                if (fcb_def != NULL && fcb_def->dcb) {

                    ccb->cns_line = fcb_def->dcb->line;

                } else {

                    return(SM_RC_END);
                }

                fsm_change_state(fcb, __LINE__, FSMB2BCNF_S_ACTIVE);

                fsm_change_state(cns_fcb, __LINE__, FSMB2BCNF_S_ACTIVE);

                cc_int_feature(CC_SRC_GSM, CC_SRC_GSM, ccb->cns_call_id,
                                   ccb->cns_line, CC_FEATURE_B2BCONF, NULL);
                return(SM_RC_END);

            }


            /*
             * This call is the conference and we are initiating a local
             * conference. So:
             * 1. Make sure we have a free line to open a new call plane to
             *    collect digits (and place call) for the consultation call,
             * 2. Create a new conference context,
             * 3. Place this call on hold,
             * 4. Send a newcall feature back to the GSM so that the
             *    consultation call can be initiated.
             */

            /*
             * Check for any other active features which may block
             * the conference.
             */

            /*
             * The call must be in the connected state to initiate a conference
             */
            fcb_def = fsm_get_fcb_by_call_id_and_type(call_id, FSM_TYPE_DEF);
            if ((fcb_def != NULL) && (fcb_def->state != FSMDEF_S_CONNECTED)) {
                break;
            }

            /*
             * Make sure we have a free line to start the consultation call.
             */
            //CSCsz38962 don't use expline for b2bcnf call
            //free_lines = lsm_get_instances_available_cnt(line, TRUE);
            free_lines = lsm_get_instances_available_cnt(line, FALSE);
            if (free_lines <= 0) {
                /*
                 * No free lines - let the user know and end this request.
                 */
                fsm_display_no_free_lines();

                break;
            }

            newcall_line = lsm_get_newcall_line(line);
            if (newcall_line == NO_LINES_AVAILABLE) {
                /*
                 * Error Pass Limit- let the user know and end this request.
                 */
                lsm_ui_display_notify_str_index(STR_INDEX_ERROR_PASS_LIMIT);

                break;
            }

            /*
             * Get a new ccb and new b2bcnf id - This is the handle that will
             * identify the b2bcnf.
             */
            ccb = fsmb2bcnf_get_new_b2bcnf_context(call_id, line);

            if (ccb==NULL || ccb->cnf_id == FSM_NO_ID) {
                break;
            }

            ccb->cnf_orig = src_id;
            fcb->b2bccb = ccb;
            ccb->cns_line = newcall_line;

            /*
             * This call needs to go on hold so we can start the consultation
             * call. Indicate feature indication should be send by setting
             * call info type to hold and feature reason to conference.
             */
            memset(&data, 0, sizeof(data));
            data.hold.call_info.type = CC_FEAT_HOLD;
            data.hold.call_info.data.hold_resume_reason = CC_REASON_CONF;
            data.hold.msg_body.num_parts = 0;
            data.hold.call_info.data.call_info_feat_data.protect = TRUE;

            cc_int_feature(CC_SRC_GSM, CC_SRC_GSM, dcb->call_id, dcb->line,
                           CC_FEATURE_HOLD, &data);

            /*
             * Initiate the consultation call.
             */
            data.newcall.cause = CC_CAUSE_CONF;
            cns_call_id = ccb->cns_call_id;
            sstrncpy(data.newcall.global_call_id,
                     ftr_data->b2bconf.global_call_id, CC_GCID_LEN);
            data.newcall.prim_call_id = ccb->cnf_call_id;
            data.newcall.hold_resume_reason = CC_REASON_CONF;

            cc_int_feature(CC_SRC_GSM, CC_SRC_GSM, cns_call_id, newcall_line,
                           CC_FEATURE_NEW_CALL, &data);

            FSM_DEBUG_SM(get_debug_string(FSMB2BCNF_DBG_CNF_INITIATED),
                         ccb->cnf_id, call_id, cns_call_id, __LINE__);

            fsm_change_state(fcb, __LINE__, FSMB2BCNF_S_ACTIVE);

            sm_rc = SM_RC_END;
            break;

        default:
            fsm_sm_ignore_ftr(fcb, __LINE__, ftr_id);
            sm_rc = SM_RC_DEF_CONT;
            break;
        }                       /* switch (ftr_id) */
        break;

    case CC_SRC_GSM:
        switch (ftr_id) {
        case CC_FEATURE_NOTIFY:

            /* Since this message is specifically for conference, msg is
             * consumed here and not forwarded
             */
            if ((msg->data.notify.subscription == CC_SUBSCRIPTIONS_REMOTECC)
                && (msg->data.notify.data.rcc.feature == CC_FEATURE_B2BCONF)) {
                sm_rc = SM_RC_END;
            }
            break;
        case CC_FEATURE_NEW_CALL:
            /*
             * If this is the consultation call involved in a conference,
             * then set the rest of the data required to make the conference
             * happen. The data is the b2bcnf_id in the fcb. The data is set now
             * because we did not have the fcb when the conference was
             * initiated. The fcb is created when a new_call event is
             * received by the FIM, not when a conference event is received.
             *
             * Or this could be the call that originated the conference and
             * the person he was talking to (the conference target) has
             * decided to conference the trasnferor to another party.
             */

            /*
             * Ignore this event if this call is not involved in a conference.
             */
            ccb = fsmb2bcnf_get_ccb_by_call_id(call_id);
            if (ccb == NULL) {
                break;
            }
            fcb->b2bccb = ccb;

            /*
             * Determine what state this b2bcnf should be in (b2bcnfing or b2bcnfed).
             * If the cnfrn key has only been hit once, then this call will
             * be in the cnfing state. If it has been hit the second time,
             * then this call should go to the cnfed state. The latter
             * case only happens when the calls are conferenced and one
             * of the remote ends has decided to transfer one leg of the cnf.
             * And it just so happens that the other call involved in the cnf
             * should match this call, so we can just use it's state to
             * assign the state to this call.
             */
            other_call_id = fsmb2bcnf_get_other_call_id(ccb, call_id);
            other_fcb = fsm_get_fcb_by_call_id_and_type(other_call_id,
                                                        FSM_TYPE_B2BCNF);

            if (other_fcb == NULL) {
                GSM_DEBUG_ERROR(GSM_F_PREFIX"FCP not found", fname);
            } else {
               fsm_change_state(fcb, __LINE__, other_fcb->state);
            }
            break;

        default:
            fsm_sm_ignore_ftr(fcb, __LINE__, ftr_id);
            sm_rc = SM_RC_DEF_CONT;
            break;
        }                       /* switch (ftr_id) */
        break;

    default:
        fsm_sm_ignore_src(fcb, __LINE__, src_id);
        sm_rc = SM_RC_DEF_CONT;
        break;
    }                           /* switch (src_id) */

    return (sm_rc);
}


static sm_rcs_t
fsmb2bcnf_ev_active_release (sm_event_t *event)
{

    fsm_fcb_t        *fcb     = (fsm_fcb_t *) event->data;
    fsmcnf_ccb_t     *ccb     = fcb->b2bccb;

    /* For round table phone wait for NOTIFY response, so do not
     * clear the conf state machine
     */
    if (ccb->active == FALSE) {
        fsmb2bcnf_feature_cancel(ccb, ccb->cnf_line, ccb->cnf_call_id,
                                        ccb->cns_call_id, CC_SK_EVT_TYPE_IMPLI);
        fsmb2bcnf_cleanup((fsm_fcb_t *) event->data, __LINE__, TRUE);
    }

    return (SM_RC_CONT);
}

static sm_rcs_t
fsmb2bcnf_ev_active_release_complete (sm_event_t *event)
{
    fsm_fcb_t        *fcb     = (fsm_fcb_t *) event->data;
    fsmcnf_ccb_t     *ccb     = fcb->b2bccb;

    if (ccb->active == FALSE) {


        fsmb2bcnf_cleanup((fsm_fcb_t *) event->data, __LINE__, TRUE);
    }

    return (SM_RC_CONT);
}

static sm_rcs_t
fsmb2bcnf_ev_active_feature (sm_event_t *event)
{
    static const char fname[] = "fsmb2bcnf_ev_active_feature";
    fsm_fcb_t        *fcb     = (fsm_fcb_t *) event->data;
    cc_feature_t     *msg     = (cc_feature_t *) event->msg;
    callid_t          call_id = msg->call_id;
    fsmdef_dcb_t     *dcb     = fcb->dcb;
    fsmcnf_ccb_t     *ccb     = fcb->b2bccb;
    cc_srcs_t         src_id  = msg->src_id;
    cc_features_t     ftr_id  = msg->feature_id;
    cc_feature_data_t *feat_data   = &(msg->data);
    sm_rcs_t          sm_rc   = SM_RC_CONT;
    callid_t          other_call_id;
    fsmdef_dcb_t     *other_dcb;
    fsm_fcb_t        *other_fcb;
    cc_action_data_t  action_data;
    fsm_fcb_t        *cnf_fcb = NULL;

    fsm_sm_ftr(ftr_id, src_id);

    memset(&action_data, 0, sizeof(cc_action_data_t));

    switch (src_id) {
    case CC_SRC_UI:
    case CC_SRC_RCC:
    case CC_SRC_GSM:
        switch (ftr_id) {
        case CC_FEATURE_CANCEL:
            sm_rc = SM_RC_END;
            fsmb2bcnf_feature_cancel(ccb, ccb->cnf_line, ccb->cnf_call_id,
                                ccb->cns_call_id,
                                CC_SK_EVT_TYPE_EXPLI);
            fsmb2bcnf_cleanup(fcb, __LINE__, TRUE);
            break;

        case CC_FEATURE_HOLD:
            /* Do not send out protect parameter for CTI generated conference
             * and send protect=true for swap or hold call
            */
            if ((msg->data_valid) &&
                (feat_data->hold.call_info.data.hold_resume_reason == CC_REASON_SWAP ||
                feat_data->hold.call_info.data.hold_resume_reason == CC_REASON_CONF ||
                feat_data->hold.call_info.data.hold_resume_reason == CC_REASON_INTERNAL))
            {
                feat_data->hold.call_info.data.call_info_feat_data.protect = TRUE;
            } else if ((msg->data_valid) &&
                (feat_data->hold.call_info.data.hold_resume_reason == CC_REASON_RCC)) {
                //Do nothing remote-cc will terminate the feature layer.

            } else {
                DEF_DEBUG(DEB_F_PREFIX"Invoke hold call_id = %d t_call_id=%d",
                        DEB_F_PREFIX_ARGS(GSM, fname), ccb->cnf_call_id, ccb->cns_call_id);
                //Actual hold to this call, so break the feature layer.
                ui_terminate_feature(ccb->cnf_line, ccb->cnf_call_id, ccb->cns_call_id);
                fsmb2bcnf_feature_cancel(ccb, ccb->cnf_line, ccb->cnf_call_id,
                        ccb->cns_call_id, CC_SK_EVT_TYPE_IMPLI);
                fsmb2bcnf_cleanup(fcb, __LINE__, TRUE);
            }
            fsm_sm_ignore_ftr(fcb, __LINE__, ftr_id);
            break;

        case CC_FEATURE_B2BCONF:
            /* Connect the existing call to active trasnfer state
             * machine. If the UI generates the event with target
             * call_id in the data then terminate the existing consulatative
             * call and link that to another call.
            */
            DEF_DEBUG(DEB_F_PREFIX"ACTIVE CNF call_id = %d, t_id = %d, cns_id=%d",
                DEB_F_PREFIX_ARGS(GSM, fname), ccb->cnf_call_id,
                    feat_data->b2bconf.target_call_id, ccb->cns_call_id);

            if (feat_data && msg->data_valid &&
                (feat_data->b2bconf.target_call_id != CC_NO_CALL_ID)) {
                /* End existing consult call and then link another
                * call with the trasfer. This is the case where User can
                * select active call instead of consultative call
                */
                cnf_fcb = fsm_get_fcb_by_call_id_and_type(ccb->cns_call_id,
                                                    FSM_TYPE_DEF);

                /* If the call_id is different then active call has been picked
                 */

                if (ccb->cns_call_id != feat_data->b2bconf.target_call_id) {

                    cnf_fcb = fsm_get_fcb_by_call_id_and_type(ccb->cns_call_id,
                            FSM_TYPE_B2BCNF);

                    if (cnf_fcb != NULL) {
                        DEF_DEBUG(DEB_F_PREFIX"INVOKE ACTIVE CNF call_id = %d, t_id=%d",
                            DEB_F_PREFIX_ARGS(GSM, fname), ccb->cnf_call_id,
                        feat_data->b2bconf.target_call_id);

                        cnf_fcb->b2bccb = ccb;
                        fsm_change_state(cnf_fcb, __LINE__, FSMB2BCNF_S_ACTIVE);


                        cc_int_feature(CC_SRC_GSM, CC_SRC_GSM, ccb->cns_call_id,
                                   ccb->cnf_line, CC_FEATURE_B2BCONF, NULL);
                    }

                    return(SM_RC_END);
                }
            }
            /*
             * This is the second conference event for a local
             * attended conference with consultation.
             *
             * The user is attempting to complete the conference, so
             * resume the other leg of the conference call.
             */
            ccb->active = TRUE;

            if (dcb) {
                dcb->active_feature = CC_FEATURE_B2BCONF;
            }

            other_call_id = fsmb2bcnf_get_other_call_id(ccb, call_id);
            other_dcb = fsm_get_dcb(other_call_id);

            //Update confinvoked value through JPlatUi method.
            ui_update_conf_invoked(other_dcb->line, other_call_id, TRUE);

            fsmb2bcnf_cnf_invoke(fsmb2bcnf_get_other_call_id(ccb, call_id),
                call_id, other_dcb->line, ccb);

            other_fcb = fsm_get_fcb_by_call_id_and_type(other_call_id,
                                                        FSM_TYPE_B2BCNF);

            fsm_change_state(other_fcb, __LINE__, FSMB2BCNF_S_ACTIVE);

            fsm_change_state(fcb, __LINE__, FSMB2BCNF_S_ACTIVE);

            sm_rc = SM_RC_END;

            break;

        case CC_FEATURE_END_CALL:
            /* Release ccbs related to conference and allow event to pass through
             * fsmdef
             */
            fsmb2bcnf_feature_cancel(ccb, ccb->cnf_line, ccb->cnf_call_id,
                        ccb->cns_call_id, CC_SK_EVT_TYPE_IMPLI);
            fsmb2bcnf_cleanup(fcb, __LINE__, TRUE);

            break;


        case CC_FEATURE_RESUME:
            /* to achieve SCCP phone behaviour, if the 1st call is resumed
             * then conference should be terminated.
             */
            if (ccb->cnf_orig == CC_SRC_RCC) {
                if (ccb->cnf_call_id == call_id) {
                    fsmb2bcnf_feature_cancel(ccb, ccb->cnf_line, ccb->cnf_call_id,
                        ccb->cns_call_id, CC_SK_EVT_TYPE_IMPLI);
                    fsmb2bcnf_cleanup(fcb, __LINE__, TRUE);
                    cc_call_state(dcb->call_id, dcb->line, CC_STATE_CONNECTED,
                                  ((cc_state_data_t *) (&(dcb->caller_id))));
                }
            }
            break;

        case CC_FEATURE_NOTIFY:

            if ((msg->data.notify.subscription == CC_SUBSCRIPTIONS_REMOTECC)
                && (msg->data.notify.data.rcc.feature == CC_FEATURE_B2BCONF)) {

                if (msg->data.notify.cause_code != RCC_SUCCESS) {
                    fsmb2bcnf_feature_cancel(fcb->b2bccb, fcb->b2bccb->cnf_line,
                                    fcb->b2bccb->cnf_call_id,
                                    fcb->b2bccb->cns_call_id, CC_SK_EVT_TYPE_IMPLI);
                }
                /*
                 * Conference key press has been accepted, so now terminate the
                 * conference data structures. This call is now same as any other
                 * regular call. All the future events are handled by fsmdef.
                 */
                fsmb2bcnf_cleanup(fcb, __LINE__, TRUE);
                sm_rc = SM_RC_END;
            }
            break;

        default:
            fsm_sm_ignore_ftr(fcb, __LINE__, ftr_id);
            break;
        }                       /* switch (ftr_id) */
        break;

    case CC_SRC_SIP:
        switch (ftr_id) {
        case CC_FEATURE_CALL_PRESERVATION:
             DEF_DEBUG(DEB_F_PREFIX"Invoke hold call_id = %d t_call_id=%d",
                            DEB_F_PREFIX_ARGS(GSM, fname), ccb->cnf_call_id, ccb->cns_call_id);
             //Actual hold to this call, so break the feature layer.
             ui_terminate_feature(ccb->cnf_line, ccb->cnf_call_id, ccb->cns_call_id);
             fsmb2bcnf_feature_cancel(ccb, ccb->cnf_line, ccb->cnf_call_id,
                        ccb->cns_call_id, CC_SK_EVT_TYPE_IMPLI);
             fsmb2bcnf_cleanup(fcb, __LINE__, TRUE);
             break;

        default:
            fsm_sm_ignore_ftr(fcb, __LINE__, ftr_id);
            break;
        }                       /* switch (ftr_id) */
        break;

    default:
        fsm_sm_ignore_src(fcb, __LINE__, src_id);
        break;
    }                           /* switch (src_id) */

    return (sm_rc);
}

static sm_rcs_t
fsmb2bcnf_ev_active_feature_ack (sm_event_t *event)
{
    fsm_fcb_t        *fcb     = (fsm_fcb_t *) event->data;
    cc_feature_ack_t *msg     = (cc_feature_ack_t *) event->msg;
    callid_t          call_id = msg->call_id;
    cc_srcs_t         src_id  = msg->src_id;
    cc_features_t     ftr_id  = msg->feature_id;
    sm_rcs_t          sm_rc   = SM_RC_CONT;
    cc_action_data_t  data;
    callid_t          other_call_id;
    callid_t          other_ui_id;

    fsm_sm_ftr(ftr_id, src_id);

    memset(&data, 0, sizeof(cc_action_data_t));

    switch (src_id) {
    case CC_SRC_GSM:
    case CC_SRC_SIP:
        switch (ftr_id) {
        case CC_FEATURE_B2BCONF:
            /* Handle 2nd conference key press completion NOTIFY */
            if (msg->cause == CC_CAUSE_ERROR) {
                other_call_id =
                    fsmb2bcnf_get_other_call_id(fcb->b2bccb, call_id);

                other_ui_id = lsm_get_ui_id(other_call_id);
                ui_set_call_status(platform_get_phrase_index_str(CONF_CANNOT_COMPLETE),
                                   msg->line, other_ui_id);
                fsmb2bcnf_feature_cancel(fcb->b2bccb, fcb->b2bccb->cnf_line, fcb->b2bccb->cnf_call_id,
                                    fcb->b2bccb->cns_call_id, CC_SK_EVT_TYPE_IMPLI);
                fsmb2bcnf_cleanup(fcb, __LINE__, TRUE);
                break;
            }

            sm_rc = SM_RC_END;
            break;

        case CC_FEATURE_NOTIFY:

            if ((msg->data.notify.subscription == CC_SUBSCRIPTIONS_REMOTECC) &&
                (msg->data.notify.data.rcc.feature == CC_FEATURE_B2BCONF)) {

                /*
                 * Conference key press has been accepted, so now terminate the
                 * conference data structures. This call is now same as any other
                 * regular call. All the future events are handled by fsmdef.
                 */
                fsmb2bcnf_cleanup(fcb, __LINE__, TRUE);
                sm_rc = SM_RC_END;
            }

            break;

        default:
            fsm_sm_ignore_ftr(fcb, __LINE__, ftr_id);
            break;
        }                       /* switch (ftr_id) */
        break;

    default:
        fsm_sm_ignore_src(fcb, __LINE__, src_id);
        break;
    }                           /* switch (src_id) */

    return (sm_rc);
}

void
fsmb2bcnf_get_sub_call_id_from_ccb(fsmcnf_ccb_t *ccb, callid_t *cnf_call_id,
                callid_t *cns_call_id)
{
    static const char fname[] = "fsmb2bcnf_get_sub_call_id_from_ccb";

    DEF_DEBUG(DEB_F_PREFIX"call_id = %d t_call_id=%d",
                        DEB_F_PREFIX_ARGS(GSM, fname), ccb->cnf_call_id, ccb->cns_call_id);

    *cnf_call_id = ccb->cnf_call_id;
    *cns_call_id = ccb->cns_call_id;
}

static sm_rcs_t
fsmb2bcnf_ev_active_onhook (sm_event_t *event)
{
    fsm_fcb_t        *fcb     = (fsm_fcb_t *) event->data;
    fsmcnf_ccb_t     *ccb     = fcb->b2bccb;
    cc_onhook_t      *msg     = (cc_onhook_t *) event->msg;

    /* For RT phone active call list can be invoked during
     * conf and that genertes onhook event for existing
     * consult call. Conf state machine is not terminated
     */
    if (msg->active_list == CC_REASON_ACTIVECALL_LIST) {

        ccb->cns_line = CC_NO_LINE;
        ccb->cns_call_id = CC_NO_CALL_ID;

    } else {
        fsmb2bcnf_feature_cancel(ccb, ccb->cnf_line, ccb->cnf_call_id,
                                    ccb->cns_call_id, CC_SK_EVT_TYPE_IMPLI);
        fsmb2bcnf_cleanup((fsm_fcb_t *) event->data, __LINE__, TRUE);
    }

    return (SM_RC_CONT);
}

cc_int32_t
fsmb2bcnf_show_cmd (cc_int32_t argc, const char *argv[])
{
    fsmcnf_ccb_t *ccb;
    int           i = 0;

    /*
     * check if need help
     */
    if ((argc == 2) && (argv[1][0] == '?')) {
        debugif_printf("show fsmb2bcnf\n");
        return (0);
    }

    debugif_printf("\n-------------------------- FSMB2BCNF ccbs --------------------------");
    debugif_printf("\ni   b2bcnf_id  ccb         cnf_call_id  cns_call_id  active  bridged");
    debugif_printf("\n--------------------------------------------------------------------"
         "\n");

    FSM_FOR_ALL_CBS(ccb, fsmb2bcnf_ccbs, FSMCNF_MAX_CCBS) {
        debugif_printf("%-2d  %-6d  %p  %-11d  %-11d  %-6d  %-7d\n",
                       i++, ccb->cnf_id, ccb, ccb->cnf_call_id,
                       ccb->cns_call_id, ccb->active, ccb->bridged);
    }

    return (0);
}


void
fsmb2bcnf_init (void)
{
    fsmcnf_ccb_t *ccb;
    static const char *fname = "fsmb2bcnf_init";


    /*
     * Initialize the ccbs.
     */
    fsmb2bcnf_ccbs = (fsmcnf_ccb_t *)
        cpr_malloc(sizeof(fsmcnf_ccb_t) * FSMCNF_MAX_CCBS);

    if (fsmb2bcnf_ccbs == NULL) {
        GSM_DEBUG_ERROR(GSM_F_PREFIX"Failed to allocate memory \
                forb2bcnf ccbs.\n", fname);
        return;
    }

    FSM_FOR_ALL_CBS(ccb, fsmb2bcnf_ccbs, FSMCNF_MAX_CCBS) {
        fsmb2bcnf_init_ccb(ccb);
    }

    /*
     * Initialize the state/event table.
     */
    g_fsmb2bcnf_sm_table.min_state = FSMB2BCNF_S_MIN;
    g_fsmb2bcnf_sm_table.max_state = FSMB2BCNF_S_MAX;
    g_fsmb2bcnf_sm_table.min_event = CC_MSG_MIN;
    g_fsmb2bcnf_sm_table.max_event = CC_MSG_MAX;
    g_fsmb2bcnf_sm_table.table     = (&(fsmb2bcnf_function_table[0][0]));
}


callid_t
fsmb2bcnf_get_primary_call_id (callid_t call_id)
{
    fsmcnf_ccb_t *ccb;

    ccb = fsmb2bcnf_get_ccb_by_call_id(call_id);

    if (ccb && (ccb->cns_call_id == call_id)) {
        return (fsmb2bcnf_get_other_call_id(ccb, call_id));
    } else {
        return (CC_NO_CALL_ID);
    }
}

callid_t
fsmb2bcnf_get_consult_call_id (callid_t call_id)
{
    fsmcnf_ccb_t *ccb;

    ccb = fsmb2bcnf_get_ccb_by_call_id(call_id);

    if (ccb && ccb->cnf_call_id == call_id) {
        return (fsmb2bcnf_get_other_call_id(ccb, call_id));
    } else {
        return (CC_NO_CALL_ID);
    }
}

int
fsmutil_is_b2bcnf_consult_call (callid_t call_id)
{
    return fsmutil_is_cnf_consult_leg(call_id, fsmb2bcnf_ccbs, FSMCNF_MAX_CCBS);
}

boolean
fsmb2bcnf_is_rcc_orig_b2bcnf (callid_t call_id)
{
    fsmcnf_ccb_t *ccb;

    ccb = fsmb2bcnf_get_ccb_by_call_id(call_id);
    if (ccb && ccb->cnf_orig == CC_SRC_RCC) {
        return TRUE;
    }

    return FALSE;
}

void
fsmb2bcnf_shutdown (void)
{
    cpr_free(fsmb2bcnf_ccbs);
    fsmb2bcnf_ccbs = NULL;
}
