/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "cpr_types.h"
#include "cpr_stdlib.h"
#include "cpr_stdio.h"
#include "cpr_string.h"
#include "fim.h"
#include "lsm.h"
#include "sm.h"
#include "ccapi.h"
#include "phone_debug.h"
#include "text_strings.h"
#include "fsm.h"
#include "uiapi.h"
#include "debug.h"
#include "regmgrapi.h"
#include "platform_api.h"

extern fsmdef_dcb_t *fsmdef_dcbs;

#define FSMXFR_NULL_DIALSTRING ""
static fsmxfr_xcb_t *fsmxfr_xcbs;

typedef enum fsmxfr_states_t_ {
    FSMXFR_S_MIN = -1,
    FSMXFR_S_IDLE,
    FSMXFR_S_ACTIVE,
    FSMXFR_S_MAX
} fsmxfr_states_t;

static const char *const fsmxfr_state_names[] = {
    "IDLE",
    "ACTIVE"
};

static sm_rcs_t fsmxfr_ev_idle_setup(sm_event_t *event);
static sm_rcs_t fsmxfr_ev_idle_feature(sm_event_t *event);
static sm_rcs_t fsmxfr_ev_idle_dialstring(sm_event_t *event);
static sm_rcs_t fsmxfr_ev_active_connected_ack(sm_event_t *event);
static sm_rcs_t fsmxfr_ev_active_release(sm_event_t *event);
static sm_rcs_t fsmxfr_ev_active_release_complete(sm_event_t *event);
static sm_rcs_t fsmxfr_ev_active_feature(sm_event_t *event);
static sm_rcs_t fsmxfr_ev_active_feature_ack(sm_event_t *event);
static sm_rcs_t fsmxfr_ev_active_onhook(sm_event_t *event);
static sm_rcs_t fsmxfr_ev_active_dialstring(sm_event_t *event);
static sm_rcs_t fsmxfr_ev_active_proceeding(sm_event_t *event);

static sm_function_t fsmxfr_function_table[FSMXFR_S_MAX][CC_MSG_MAX] =
{
/* FSMXFR_S_IDLE ------------------------------------------------------------ */
    {
    /* FSMXFR_E_SETUP            */ fsmxfr_ev_idle_setup,
    /* FSMXFR_E_SETUP_ACK        */ NULL,
    /* FSMXFR_E_PROCEEDING       */ NULL,
    /* FSMXFR_E_ALERTING         */ NULL,
    /* FSMXFR_E_CONNECTED        */ NULL,
    /* FSMXFR_E_CONNECTED_ACK    */ NULL,
    /* FSMXFR_E_RELEASE          */ NULL,
    /* FSMXFR_E_RELEASE_COMPLETE */ NULL,
    /* FSMXFR_E_FEATURE          */ fsmxfr_ev_idle_feature,
    /* FSMXFR_E_FEATURE_ACK      */ NULL,
    /* FSMXFR_E_OFFHOOK          */ NULL,
    /* FSMXFR_E_ONHOOK           */ NULL,
    /* FSMXFR_E_LINE             */ NULL,
    /* FSMXFR_E_DIGIT_BEGIN      */ NULL,
    /* FSMXFR_E_DIGIT            */ NULL,
    /* FSMXFR_E_DIALSTRING       */ fsmxfr_ev_idle_dialstring,
    /* FSMXFR_E_MWI              */ NULL,
    /* FSMXFR_E_SESSION_AUDIT    */ NULL
    },

/* FSMXFR_S_ACTIVE ---------------------------------------------------- */
    {
    /* FSMXFR_E_SETUP            */ NULL,
    /* FSMXFR_E_SETUP_ACK        */ NULL,
    /* FSMXFR_E_PROCEEDING       */ fsmxfr_ev_active_proceeding,
    /* FSMXFR_E_ALERTING         */ NULL,
    /* FSMXFR_E_CONNECTED        */ NULL,
    /* FSMXFR_E_CONNECTED_ACK    */ fsmxfr_ev_active_connected_ack,
    /* FSMXFR_E_RELEASE          */ fsmxfr_ev_active_release,
    /* FSMXFR_E_RELEASE_COMPLETE */ fsmxfr_ev_active_release_complete,
    /* FSMXFR_E_FEATURE          */ fsmxfr_ev_active_feature,
    /* FSMXFR_E_FEATURE_ACK      */ fsmxfr_ev_active_feature_ack,
    /* FSMXFR_E_OFFHOOK          */ NULL,
    /* FSMXFR_E_ONHOOK           */ fsmxfr_ev_active_onhook,
    /* FSMXFR_E_LINE             */ NULL,
    /* FSMXFR_E_DIGIT_BEGIN      */ NULL,
    /* FSMXFR_E_DIGIT            */ NULL,
    /* FSMXFR_E_DIALSTRING       */ fsmxfr_ev_active_dialstring,
    /* FSMXFR_E_MWI              */ NULL,
    /* FSMXFR_E_SESSION_AUDIT    */ NULL
    }
};

static sm_table_t fsmxfr_sm_table;
sm_table_t *pfsmxfr_sm_table = &fsmxfr_sm_table;

const char *
fsmxfr_state_name (int state)
{
    if ((state <= FSMXFR_S_MIN) || (state >= FSMXFR_S_MAX)) {
        return (get_debug_string(GSM_UNDEFINED));
    }

    return (fsmxfr_state_names[state]);
}


static int
fsmxfr_get_new_xfr_id (void)
{
    static int xfr_id = 0;

    if (++xfr_id < 0) {
        xfr_id = 1;
    }

    return (xfr_id);
}

/**
 *
 * Sets/rest transfer complete flag in the DCB so it can be used in
 * UI to indicate if the call is ended because of xfer or endcall.
 *
 * @cns_call_id consult call id
 * @Xfr_call_id transfer call id
 *
 * @return  none
 *
 * @pre     (none)
 */
void fsmxfr_mark_dcb_for_xfr_complete(callid_t cns_call_id, callid_t xfr_call_id,
        boolean set_flag)
{
    fsm_fcb_t    *cns_fcb, *xfr_fcb;

    cns_fcb = fsm_get_fcb_by_call_id_and_type(cns_call_id,
                                    FSM_TYPE_DEF);
    xfr_fcb = fsm_get_fcb_by_call_id_and_type(xfr_call_id,
                                    FSM_TYPE_DEF);
    if (set_flag) {
        if (cns_fcb && cns_fcb->dcb) {
            FSM_SET_FLAGS(cns_fcb->dcb->flags, FSMDEF_F_XFER_COMPLETE);
        }
        if (xfr_fcb && xfr_fcb->dcb) {
            FSM_SET_FLAGS(xfr_fcb->dcb->flags, FSMDEF_F_XFER_COMPLETE);
        }
    } else {
        if (cns_fcb && cns_fcb->dcb) {
            FSM_RESET_FLAGS(cns_fcb->dcb->flags, FSMDEF_F_XFER_COMPLETE);
        }
        if (xfr_fcb && xfr_fcb->dcb) {
            FSM_RESET_FLAGS(xfr_fcb->dcb->flags, FSMDEF_F_XFER_COMPLETE);
        }
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


fsm_fcb_t *fsmxfr_get_active_xfer(void)
{
    fsm_fcb_t    *fcb;
    fsmxfr_xcb_t *xcb;

    FSM_FOR_ALL_CBS(xcb, fsmxfr_xcbs, FSMXFR_MAX_XCBS) {
        fcb = fsm_get_fcb_by_call_id_and_type(xcb->xfr_call_id,
                                               FSM_TYPE_XFR);
        if (fcb && fcb->state == FSMXFR_S_ACTIVE) {
            return(fcb);
        }
    }

    return(NULL);
}


static void
fsmxfr_init_xcb (fsmxfr_xcb_t *xcb)
{
    if (xcb != NULL) {
        xcb->xfr_id        = FSM_NO_ID;
        xcb->xfr_call_id   = CC_NO_CALL_ID;
        xcb->cns_call_id   = CC_NO_CALL_ID;
        xcb->xfr_line      = CC_NO_LINE;
        xcb->cns_line      = CC_NO_LINE;
        xcb->type          = FSMXFR_TYPE_NONE;
        xcb->method        = CC_XFER_METHOD_NONE;
        xcb->cnf_xfr       = FALSE;
        xcb->active        = FALSE;
        xcb->mode          = FSMXFR_MODE_TRANSFEROR;
        xcb->xfer_comp_req = FALSE;
        xcb->xfr_orig      = CC_SRC_MIN;

        if (xcb->xcb2 != NULL) {
            fsmxfr_init_xcb(xcb->xcb2);
            xcb->xcb2 = NULL;
        }

        if (xcb->dialstring != NULL) {
            cpr_free(xcb->dialstring);
            xcb->dialstring = NULL;
        }
        if (xcb->queued_dialstring != NULL) {
            cpr_free(xcb->queued_dialstring);
            xcb->queued_dialstring = NULL;
        }
        if (xcb->referred_by != NULL) {
            cpr_free(xcb->referred_by);
            xcb->referred_by = NULL;
        }
    }
}


static fsmxfr_xcb_t *
fsmxfr_get_xcb_by_xfr_id (int xfr_id)
{
    static const char fname[] = "fsmxfr_get_xcb_by_xfr_id";
    fsmxfr_xcb_t *xcb;
    fsmxfr_xcb_t *xcb_found = NULL;

    FSM_FOR_ALL_CBS(xcb, fsmxfr_xcbs, FSMXFR_MAX_XCBS) {
        if (xcb->xfr_id == xfr_id) {
            xcb_found = xcb;

            FSM_DEBUG_SM(get_debug_string(FSMXFR_DBG_PTR), xcb->xfr_id,
                         xcb->xfr_call_id, xcb->cns_call_id, fname, xcb);

            break;
        }
    }


    return (xcb_found);
}


/*
 *  Function: fsmxfr_get_new_xfr_context
 *
 *  Parameters:
 *      xfr_call_id: call_id for the call initiating the transfer
 *      type:        attended or unattended transfer
 *      method:      BYE/ALSO or REFER method of transfer
 *      local:       local or remote initiated transfer
 *
 *  Description: This function creates a new transfer context by:
 *               - getting a free xcb
 *               - creating new xfr_id and cns_call_id
 *
 *  Returns: xcb
 *
 */
static fsmxfr_xcb_t *
fsmxfr_get_new_xfr_context (callid_t xfr_call_id, line_t line, fsmxfr_types_t type,
                           cc_xfer_methods_t method, fsmxfr_modes_t mode)
{
    static const char fname[] = "fsmxfr_get_new_xfr_context";
    fsmxfr_xcb_t *xcb;

    xcb = fsmxfr_get_xcb_by_xfr_id(FSM_NO_ID);
    if (xcb != NULL) {
        xcb->xfr_id      = fsmxfr_get_new_xfr_id();
        xcb->xfr_call_id = xfr_call_id;
        xcb->cns_call_id = cc_get_new_call_id();
        xcb->xfr_line    = line;
        xcb->cns_line    = line;
        xcb->type        = type;
        xcb->method      = method;
        xcb->mode        = mode;

        FSM_DEBUG_SM(get_debug_string(FSMXFR_DBG_PTR), xcb->xfr_id,
                     xcb->xfr_call_id, xcb->cns_call_id, fname, xcb);
    }

    return (xcb);
}


fsmxfr_xcb_t *
fsmxfr_get_xcb_by_call_id (callid_t call_id)
{
    fsmxfr_xcb_t *xcb;
    fsmxfr_xcb_t *xcb_found = NULL;

    FSM_FOR_ALL_CBS(xcb, fsmxfr_xcbs, FSMXFR_MAX_XCBS) {
        if ((xcb->xfr_call_id == call_id) || (xcb->cns_call_id == call_id)) {
            xcb_found = xcb;
            break;
        }
    }

    return (xcb_found);
}

/**
 *
 * Cancel tranfer operation by sending cancel event to SIP stack.
 * This routine is used in roundtable phone.
 *
 * @param line, call_id, target_call_id, cause (implicit or explicit)
 *
 * @return  void
 *
 * @pre     (none)
 */
void
fsmxfr_feature_cancel (fsmxfr_xcb_t *xcb, line_t line, callid_t call_id,
                       callid_t target_call_id,
                       cc_rcc_skey_evt_type_e cause)
{
    static const char fname[] = "fsmxfr_feature_cancel";
    cc_feature_data_t data;
    fsm_fcb_t         *fcb_def;

    DEF_DEBUG(DEB_F_PREFIX"Sending cancel call_id = %d, t_id=%d, cause = %d",
            DEB_F_PREFIX_ARGS(GSM, fname), call_id, target_call_id, cause);

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


void
fsmxfr_update_xfr_context (fsmxfr_xcb_t *xcb, callid_t old_call_id,
                           callid_t new_call_id)
{
    static const char fname[] = "fsmxfr_update_xfr_context";
    FSM_DEBUG_SM(DEB_F_PREFIX"Entered.", DEB_F_PREFIX_ARGS(FSM, "fsmxfr_update_xfr_context"));

    if (xcb != NULL) {
        if (old_call_id == xcb->xfr_call_id) {
            xcb->xfr_call_id = new_call_id;
        } else if (old_call_id == xcb->cns_call_id) {
            xcb->cns_call_id = new_call_id;
        }

        FSM_DEBUG_SM(get_debug_string(FSMXFR_DBG_PTR), xcb->xfr_id,
                     xcb->xfr_call_id, xcb->cns_call_id, fname, xcb);
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
fsmxfr_get_other_line (fsmxfr_xcb_t *xcb, callid_t call_id)
{
    line_t other_line = CC_NO_LINE;

    if (xcb != NULL) {
        if (xcb->xfr_call_id == call_id) {
            other_line = xcb->cns_line;
        } else if (xcb->cns_call_id == call_id) {
            other_line = xcb->xfr_line;
        }
    }

    return (other_line);
}

callid_t
fsmxfr_get_other_call_id (fsmxfr_xcb_t *xcb, callid_t call_id)
{
    callid_t other_call_id = CC_NO_CALL_ID;

    if (xcb != NULL) {
        if (xcb->xfr_call_id == call_id) {
            other_call_id = xcb->cns_call_id;
        } else if (xcb->cns_call_id == call_id) {
            other_call_id = xcb->xfr_call_id;
        }
    }

    return (other_call_id);
}


/*
 *  Function: fsmxfr_remove_fcb
 *
 *  Parameters:
 *      xfr_id:  xfr_id for the transfer
 *      call_id: call_id that identifies the fcb to be removed
 *
 *  Description: This function will remove the fcb identified by the given
 *               call_id from the xcb. And the function will free the xcb
 *               if both fcbs have been removed.
 *
 *  Returns: none
 *
 *  Note: This is a helper function for fsmxfr_cleanup. It allows fsmxfr_cleanup
 *        to cleanup one fcb (one call involved in the transfer) independent
 *        of the other involved fcb.
 */
static void
fsmxfr_remove_fcb (fsm_fcb_t *fcb, callid_t call_id)
{
    fsmxfr_xcb_t *xcb = fcb->xcb;
    FSM_DEBUG_SM(DEB_F_PREFIX"Entered.", DEB_F_PREFIX_ARGS(FSM, "fsmxfr_remove_fcb"));

    if (xcb != NULL) {
        fsmxfr_update_xfr_context(xcb, call_id, CC_NO_CALL_ID);

        /*
         * Free the xcb if both fcb references have been removed.
         */
        if ((xcb->xfr_call_id == CC_NO_CALL_ID) &&
            (xcb->cns_call_id == CC_NO_CALL_ID)) {
            fsmxfr_init_xcb(xcb);
        }
    }
}


static void
fsmxfr_cnf_cleanup (fsmxfr_xcb_t *xcb)
{
    fsmdef_dcb_t     *xfr_dcb;
    fsmdef_dcb_t     *cns_dcb;
    cc_feature_data_t ftr_data;

    cns_dcb = fsm_get_dcb(xcb->cns_call_id);
    xfr_dcb = fsm_get_dcb(xcb->xfr_call_id);

    ftr_data.endcall.cause = CC_CAUSE_NORMAL;
    ftr_data.endcall.dialstring[0] = '\0';
    /*
     * This is a conference transfer hence we are cleaning
     * up both the lines
     */
    cc_int_feature(CC_SRC_GSM, CC_SRC_GSM,
                   cns_dcb->call_id, cns_dcb->line,
                   CC_FEATURE_END_CALL, &ftr_data);
    cc_int_feature(CC_SRC_GSM, CC_SRC_GSM,
                   xfr_dcb->call_id, xfr_dcb->line,
                   CC_FEATURE_END_CALL, &ftr_data);
}


static void
fsmxfr_cleanup (fsm_fcb_t *fcb, int fname, boolean both)
{
    fsm_fcb_t   *other_fcb = NULL;
    callid_t     call_id = fcb->call_id;
    callid_t     other_call_id = CC_NO_CALL_ID;
    line_t       other_line;


    FSM_DEBUG_SM(DEB_F_PREFIX"Entered.", DEB_F_PREFIX_ARGS(FSM, "fsmxfr_cleanup"));
    other_call_id = fsmxfr_get_other_call_id(fcb->xcb, call_id);
    other_line    = fsmxfr_get_other_line(fcb->xcb, call_id);

    if (other_call_id != CC_NO_CALL_ID) {
        other_fcb = fsm_get_fcb_by_call_id_and_type(other_call_id,
                                                    FSM_TYPE_XFR);
    }

    if (fcb->xcb && (fcb->xcb->cnf_xfr != TRUE) &&
        (call_id == fcb->xcb->xfr_call_id)) {

        if (other_call_id != CC_NO_CALL_ID) {
            /*
             * Not clearing consulation call, so change consultation call
             * attribute to display connected softkey set. Do not change
             * softkey set if it is a transfer o
             */
            cc_call_attribute(other_call_id, other_line, NORMAL_CALL);
        }
    }
    /*
     * Check if the user wanted to cleanup the whole xcb.
     * If so, then we will grab the other fcb first and call this function
     * again with this other fcb. The whole xcb will be freed after this block
     * of code because both call_ids will be -1, which tells
     * fsmxfr_remove_fcb to free the xcb.
     */
    if (both) {
        FSM_DEBUG_SM(DEB_F_PREFIX"clean both.", DEB_F_PREFIX_ARGS(FSM, "fsmxfr_cleanup"));

        if (other_call_id != CC_NO_CALL_ID) {
            if (other_fcb != NULL) {
                fsmxfr_cleanup(other_fcb, fname, FALSE);
            } else {
                /*
                 * If there is no other FCB, we need to clean up so that the
                 * XCB will be deleted.
                 */
                fsmxfr_update_xfr_context(fcb->xcb, other_call_id,
                                          CC_NO_CALL_ID);
            }
        }
    }

    /*
     * Remove the reference to this fcb from the xcb
     */
    fsmxfr_remove_fcb(fcb, fcb->call_id);

    /*
     * Move this fcb to the IDLE state
     */
    fsm_change_state(fcb, fname, FSMXFR_S_IDLE);

    /*
     * Reset the data for this fcb. The fcb is still included in a call
     * so set the call_id and dcb values accordingly.
     */
    fsm_init_fcb(fcb, fcb->call_id, fcb->dcb, FSM_TYPE_XFR);
}


void
fsmxfr_free_cb (fim_icb_t *icb, callid_t call_id)
{
    fsm_fcb_t *fcb = NULL;

    if (call_id != CC_NO_CALL_ID) {
        fcb = fsm_get_fcb_by_call_id_and_type(call_id, FSM_TYPE_XFR);

        if (fcb != NULL) {
            fsmxfr_cleanup(fcb, __LINE__, FALSE);
            fsm_init_fcb(fcb, CC_NO_CALL_ID, FSMDEF_NO_DCB, FSM_TYPE_NONE);
        }
    }
}


fsmxfr_types_t
fsmxfr_get_xfr_type (callid_t call_id)
{
    fsmxfr_xcb_t   *xcb;
    fsmxfr_types_t  type = FSMXFR_TYPE_BLND_XFR;

    xcb = fsmxfr_get_xcb_by_call_id(call_id);
    if (xcb != NULL) {
        type = xcb->type;
    }

    return (type);
}


cc_features_t
fsmxfr_type_to_feature (fsmxfr_types_t type)
{
    cc_features_t feature;

    if (type == FSMXFR_TYPE_XFR) {
        feature = CC_FEATURE_XFER;
    } else {
        feature = CC_FEATURE_BLIND_XFER;
    }

    return (feature);
}


/*******************************************************************
 * event functions
 */


static sm_rcs_t
fsmxfr_ev_idle_setup (sm_event_t *event)
{
    fsm_fcb_t    *fcb     = (fsm_fcb_t *) event->data;
    cc_setup_t   *msg     = (cc_setup_t *) event->msg;
    fsmxfr_xcb_t *xcb;

    /*
     * If this is the consultation call involved in a transfer,
     * then set the rest of the data required to make the transfer
     * happen. The data is the xcb in the fcb. The data is set now
     * because we did not have the fcb when the transfer was
     * initiated. The fcb is created when a new_call event is
     * received by the FIM, not when a transfer event is received.
     */

    /*
     * Ignore this event if this call is not involved in a transfer.
     */
    xcb = fsmxfr_get_xcb_by_call_id(msg->call_id);
    if (xcb == NULL) {
        return (SM_RC_DEF_CONT);
    }
    fcb->xcb = xcb;

    fsm_change_state(fcb, __LINE__, FSMXFR_S_ACTIVE);

    return (SM_RC_CONT);
}


static boolean
fsmxfr_copy_dialstring (char **saved_dialstring, char *dialstring)
{
    char         *tempstring;
    int           len;

    if (saved_dialstring == NULL) {
        return (FALSE);
    }

    /*
     * Make sure we have a valid dialstring.
     */
    if ((dialstring == NULL) || (dialstring[0] == '\0')) {
        return (FALSE);
    }

    /*
     * Copy the dialstring to the xcb. We will need it later when
     * SIP sends the RELEASE so that we can send out the
     * NEWCALL to start the call to the target.
     */
    len = (strlen(dialstring) + 1) * sizeof(char);
    tempstring = (char *) cpr_malloc(len);
    if (tempstring != NULL) {
        sstrncpy(tempstring, dialstring, len);

        *saved_dialstring = tempstring;
    }

    return (TRUE);
}

static void
fsmxfr_set_xfer_data (cc_causes_t cause, cc_xfer_methods_t method,
                      callid_t target_call_id, char *dialstring,
                      cc_feature_data_xfer_t *xfer)
{
    xfer->cause          = cause;
    xfer->method         = method;
    xfer->target_call_id = target_call_id;
    sstrncpy(xfer->dialstring, dialstring, CC_MAX_DIALSTRING_LEN);
}


static boolean
fsmxfr_remote_transfer (fsm_fcb_t *fcb, cc_features_t ftr_id,
                        callid_t call_id, line_t line, char *dialstring,
                        char *referred_by)
{
    fsmxfr_types_t    type;
    int               free_lines;
    cc_feature_data_t data;
    fsmxfr_xcb_t     *xcb;
    fsmxfr_xcb_t     *primary_xcb;
    callid_t          cns_call_id;
    line_t            newcall_line = 0;

    memset(&data, 0, sizeof(cc_feature_data_t));

    /*
     * Make sure we have a free line for the consultation
     * call if this is an attended transfer. We do not need this
     * check for an unattended transfer, because the stack will
     * send up a release for the call being transferred, which will
     * then trigger the fsmxfr to send out the newcall in the same
     * plane of the call being released.
     */
    if (ftr_id == CC_FEATURE_XFER) {
        /*
         * Make sure we have a free line to start the
         * consultation call.
         */
        free_lines = lsm_get_instances_available_cnt(line, TRUE);
        if (free_lines <= 0) {
            /*
             * No free lines - let the user know and end this
             * request.
             */
            fsm_display_no_free_lines();

            return (FALSE);
        }
    }

    newcall_line = lsm_get_newcall_line(line);
    if (newcall_line == NO_LINES_AVAILABLE) {
        /*
         * Error Pass Limit - let the user know and end this request.
         */
        lsm_ui_display_notify_str_index(STR_INDEX_ERROR_PASS_LIMIT);
         return (FALSE);
    }

    /*
     * Get a new xcb and new xfr id - This is the handle that will
     * identify the xfr.
     */
    type = ((ftr_id == CC_FEATURE_XFER) ?
            (FSMXFR_TYPE_XFR) : (FSMXFR_TYPE_BLND_XFR));

    primary_xcb = fsmxfr_get_xcb_by_call_id(call_id);

    xcb = fsmxfr_get_new_xfr_context(call_id, line, type, CC_XFER_METHOD_REFER,
                                     FSMXFR_MODE_TRANSFEREE);
    if (xcb == NULL) {
        return (FALSE);
    }

    if (primary_xcb) {
        fcb->xcb->xcb2 = xcb;
        fcb->xcb->active = TRUE;
    } else {
        fcb->xcb = xcb;
    }

    xcb->cns_line = newcall_line;
    cns_call_id = xcb->cns_call_id;
    fsmxfr_set_xfer_data(CC_CAUSE_OK, CC_XFER_METHOD_REFER, cns_call_id,
                         FSMXFR_NULL_DIALSTRING, &(data.xfer));

    cc_int_feature_ack(CC_SRC_GSM, CC_SRC_SIP, call_id,
                       line, ftr_id, &data, CC_CAUSE_NORMAL);

    if (ftr_id == CC_FEATURE_XFER) {
        /*
         * This call needs to go on hold so we can start the
         * consultation call. The call may already be on hold,
         * so the event will just be silently ignored. We set
         * the line number to 0xFF so that GSM will know this
         * came from a transfer and we only want to put the call
         * on local hold. We don't want to send an Invite Hold
         * to the SIP stack.
         */
        cc_int_feature(CC_SRC_GSM, CC_SRC_GSM, call_id,
                       0xFF, CC_FEATURE_HOLD, NULL);

        /*
         * Initiate the consultation call.
         */

        /*
         * Make sure we have a valid dialstring.
         */
        if ((dialstring == NULL) || (dialstring[0] == '\0')) {
            return (FALSE);
        }

        /*
         * memset is done because if redirects[0].number is
         * corrupted we might think that it is a blind
         * transfer
         */
        memset(data.newcall.redirect.redirects[0].number, 0,
               sizeof(CC_MAX_DIALSTRING_LEN));
        data.newcall.cause = CC_CAUSE_XFER_REMOTE;
        data.newcall.redirect.redirects[0].redirect_reason =
            CC_REDIRECT_REASON_DEFLECTION;
        sstrncpy(data.newcall.dialstring, dialstring, CC_MAX_DIALSTRING_LEN);

        cc_int_feature(CC_SRC_GSM, CC_SRC_GSM, cns_call_id, newcall_line,
                       CC_FEATURE_NEW_CALL, &data);

        FSM_DEBUG_SM(get_debug_string(FSMXFR_DBG_XFR_INITIATED),
                     xcb->xfr_id, call_id, cns_call_id, __LINE__);

        fsm_change_state(fcb, __LINE__, FSMXFR_S_ACTIVE);
    } else { /* CC_FEATURE_BLIND_XFER */
        /*
         * Make sure we have a valid dialstring.
         */
        if ((fsmxfr_copy_dialstring(&xcb->dialstring, dialstring) == TRUE) &&
            (fsmxfr_copy_dialstring(&xcb->referred_by, referred_by) == TRUE)) {

            fsm_change_state(fcb, __LINE__, FSMXFR_S_ACTIVE);
        } else {
            fsmxfr_cleanup(fcb, __LINE__, TRUE);
        }
    }

    return (TRUE);
}


static sm_rcs_t
fsmxfr_ev_idle_feature (sm_event_t *event)
{
    const char        *fname    = "fsmxfr_ev_idle_feature";
    fsm_fcb_t         *fcb     = (fsm_fcb_t *) event->data;
    cc_feature_t      *msg     = (cc_feature_t *) event->msg;
    callid_t           call_id = msg->call_id;
    line_t             line    = msg->line;
    cc_srcs_t          src_id  = msg->src_id;
    cc_features_t      ftr_id  = msg->feature_id;
    cc_feature_data_t *ftr_data = &(msg->data);
    fsmdef_dcb_t      *dcb     = fcb->dcb;
    fsm_fcb_t         *other_fcb;
    sm_rcs_t           sm_rc   = SM_RC_CONT;
    fsmxfr_types_t     type;
    int                free_lines;
    cc_feature_data_t  data;
    cc_causes_t        cause   = msg->data.xfer.cause;
    cc_xfer_methods_t  method;
    fsmxfr_xcb_t      *xcb;
    fsm_fcb_t         *fcb_def;
    fsm_fcb_t         *cns_fcb, *con_fcb, *sel_fcb;
    boolean            int_rc = FALSE;
    callid_t           cns_call_id;
    line_t             newcall_line = 0;

    memset(&data, 0, sizeof(data));
    fsm_sm_ftr(ftr_id, src_id);

    /*
     * Consume the XFER events and don't pass them along to the other FSMs.
     */
    if ((ftr_id == CC_FEATURE_BLIND_XFER) || (ftr_id == CC_FEATURE_XFER)) {
        sm_rc = SM_RC_END;
    }

    switch (src_id) {
    case CC_SRC_UI:
        switch (ftr_id) {
        case CC_FEATURE_DIRTRXFR:

            /* If there is a active xfer pending
             * then link this transfer to active transfer
             */
            other_fcb = fsmxfr_get_active_xfer();
            if (other_fcb) {
                if (other_fcb->xcb == NULL) {
                    GSM_DEBUG_ERROR(GSM_F_PREFIX"Cannot find the active xfer", fname);
                    return (SM_RC_END);
                }
                /* End existing consult call and then link another
                 * call with the trasfer
                 */
                cc_int_feature(CC_SRC_GSM, CC_SRC_GSM, other_fcb->xcb->cns_call_id,
                       other_fcb->xcb->cns_line, CC_FEATURE_END_CALL, NULL);
                other_fcb->xcb->cns_call_id = call_id;

                cc_int_feature(CC_SRC_GSM, CC_SRC_GSM, other_fcb->xcb->xfr_call_id,
                               other_fcb->xcb->xfr_line, CC_FEATURE_DIRTRXFR, NULL);

                return(SM_RC_END);
            }
            fsm_get_fcb_by_selected_or_connected_call_fcb(call_id, &con_fcb, &sel_fcb);

            /* If there is a call selected use that for direct transfer */
            if (sel_fcb) {
                other_fcb = sel_fcb;
            } else if (con_fcb) {
                other_fcb = con_fcb;
            } else {
                /* No connected call or selected call */
                return(SM_RC_CONT);
            }

            /* Make sure atleast one call has been selected, connected and this call
             * is not in the focus
             */
            if ((fsmutil_get_num_selected_calls() > 1) && (dcb->selected == FALSE)) {
                //return error
                return(SM_RC_CONT);
            }

            if (other_fcb->xcb == NULL || other_fcb->xcb->xfr_line != line) {
                //Not on same line display a message
                GSM_DEBUG_ERROR(GSM_F_PREFIX"Cannot find the active xfer", fname);
                return(SM_RC_CONT);
            }

            xcb = fsmxfr_get_new_xfr_context(call_id, line, FSMXFR_TYPE_XFR,
                                             CC_XFER_METHOD_REFER,
                                             FSMXFR_MODE_TRANSFEROR);
            if (xcb == NULL) {
                break;
            }

            xcb->xfr_orig = src_id;
            fcb->xcb = xcb;

            xcb->type = FSMXFR_TYPE_DIR_XFR;
            xcb->cns_call_id = other_fcb->dcb->call_id;

            other_fcb->xcb = xcb;
            /*
             * Emulating user hitting transfer key for second time
             * in attended transfer
             */
            cc_int_feature(CC_SRC_GSM, CC_SRC_GSM, dcb->call_id,
                               dcb->line, CC_FEATURE_DIRTRXFR, NULL);

            fsm_change_state(fcb, __LINE__, FSMXFR_S_ACTIVE);

            return(SM_RC_END);

        case CC_FEATURE_BLIND_XFER:
        case CC_FEATURE_XFER:

            /* Connect the existing call to active trasnfer state
             * machine. If the UI generates the event with target
             * call_id in the data then terminate the existing consulatative
             * call and link that to another call.
             */
            if ((cause != CC_CAUSE_XFER_CNF) &&
                (ftr_data && msg->data_valid) &&
                (ftr_data->xfer.target_call_id != CC_NO_CALL_ID) &&
                ((cns_fcb = fsm_get_fcb_by_call_id_and_type(ftr_data->xfer.target_call_id,
                                                    FSM_TYPE_XFR)) != NULL)) {
                /* In this case there is no active xcb but upper layer
                 * wants to complete a trasnfer with 2 different call_ids
                 */
                xcb = fsmxfr_get_new_xfr_context(call_id, line, FSMXFR_TYPE_XFR,
                                                 CC_XFER_METHOD_REFER,
                                                 FSMXFR_MODE_TRANSFEROR);
                if (xcb == NULL) {
                    return(SM_RC_END);
                }

                fcb->xcb = xcb;
                cns_fcb->xcb = xcb;
                xcb->type = FSMXFR_TYPE_DIR_XFR;

                xcb->cns_call_id = ftr_data->xfer.target_call_id;

                fcb_def = fsm_get_fcb_by_call_id_and_type(call_id, FSM_TYPE_DEF);

                /* Find line information for target call.
                 */
                if (fcb_def && fcb_def->dcb) {

                    xcb->cns_line = fcb_def->dcb->line;

                } else {

                    return(SM_RC_END);
                }

                fsm_change_state(cns_fcb, __LINE__, FSMXFR_S_ACTIVE);
                fsm_change_state(fcb, __LINE__, FSMXFR_S_ACTIVE);

                cc_int_feature(CC_SRC_GSM, CC_SRC_GSM, xcb->xfr_call_id,
                               line, CC_FEATURE_DIRTRXFR, NULL);

                return(SM_RC_END);
            }

            /*
             * This call is the transferor and we are initiating a local
             * transfer. So:
             * 1. Make sure we have a free line to open a new call plane to
             *    collect digits (and place call) for the transfer target,
             * 2. Create a new transfer context,
             * 3. Place this call on hold,
             * 4. Send a newcall feature back to the GSM so that the
             *    consultation call can be initiated.
             */

            /*
             * Check for any other active features which may block
             * the transfer.
             */

            /*
             * The call must be in the connected state to initiate a transfer.
             */
            fcb_def = fsm_get_fcb_by_call_id_and_type(call_id, FSM_TYPE_DEF);
            if ((fcb_def != NULL) &&
                (fcb_def->state != FSMDEF_S_CONNECTED) &&
                (cause != CC_CAUSE_XFER_CNF)) {
                break;
            }

            /*
             * If it's not a conference transfer make sure we have a free
             * line to start the consultation call.
             */
            if (cause != CC_CAUSE_XFER_CNF) {
                //CSCsz38962 don't use expline for local-initiated transfer
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
                     * Error Pass Limit - let the user know and end this request.
                     */
                    lsm_ui_display_notify_str_index(STR_INDEX_ERROR_PASS_LIMIT);
                    return (FALSE);
                }
            }

            /*
             * Get a new xcb and new xfr id - This is the handle that will
             * identify the xfr.
             */
            type = ((ftr_id == CC_FEATURE_XFER) ?
                    (FSMXFR_TYPE_XFR) : (FSMXFR_TYPE_BLND_XFR));

            xcb = fsmxfr_get_new_xfr_context(call_id, line, type,
                                             CC_XFER_METHOD_REFER,
                                             FSMXFR_MODE_TRANSFEROR);
            if (xcb == NULL) {
                break;
            }
            xcb->xfr_orig = src_id;
            fcb->xcb = xcb;
            /*
             * If not conference transfer initiate the consultation call.
             * For conference transfer we already have both calls setup.
             */
            if (cause != CC_CAUSE_XFER_CNF) {
                /*
                 * Set the consultative line to new line id.
                 */
                xcb->cns_line = newcall_line;
                /*
                 * Record the active feature if it is a blind xfer.
                 */
                if (ftr_id == CC_FEATURE_BLIND_XFER) {
                    dcb->active_feature = ftr_id;
                }

                /*
                 * This call needs to go on hold so we can start the
                 * consultation call. Indicate feature indication should
                 * be send and set the feature reason.
                 */
                data.hold.call_info.type = CC_FEAT_HOLD;
                data.hold.call_info.data.hold_resume_reason = CC_REASON_XFER;
                data.hold.msg_body.num_parts = 0;
                data.hold.call_info.data.call_info_feat_data.protect = TRUE;
                cc_int_feature(CC_SRC_GSM, CC_SRC_GSM, dcb->call_id, dcb->line,
                               CC_FEATURE_HOLD, &data);

                cns_call_id = xcb->cns_call_id;
                if (ftr_data->xfer.cause == CC_CAUSE_XFER_LOCAL_WITH_DIALSTRING) {
                    cc_int_dialstring(CC_SRC_GSM, CC_SRC_GSM, cns_call_id, newcall_line,
                                      ftr_data->xfer.dialstring, NULL, 0);
                } else {
                    data.newcall.cause = CC_CAUSE_XFER_LOCAL;
                    if (ftr_data->xfer.dialstring[0] != 0) {
                        data.newcall.cause = CC_CAUSE_XFER_BY_REMOTE;
                        sstrncpy(data.newcall.dialstring, ftr_data->xfer.dialstring,
                                 CC_MAX_DIALSTRING_LEN);
                    }

                    if (ftr_data->xfer.global_call_id[0] != 0) {
                        sstrncpy(data.newcall.global_call_id,
                                 ftr_data->xfer.global_call_id, CC_GCID_LEN);
                    }
                    data.newcall.prim_call_id = xcb->xfr_call_id;
                    data.newcall.hold_resume_reason = CC_REASON_XFER;

                    cc_int_feature(CC_SRC_GSM, CC_SRC_GSM, cns_call_id, newcall_line,
                                   CC_FEATURE_NEW_CALL, &data);
                }
                FSM_DEBUG_SM(get_debug_string(FSMXFR_DBG_XFR_INITIATED),
                             xcb->xfr_id, call_id, cns_call_id, __LINE__);
            } else {
                other_fcb =
                    fsm_get_fcb_by_call_id_and_type(msg->data.xfer.target_call_id,
                                                    FSM_TYPE_XFR);
                if (other_fcb == NULL) {
                    GSM_DEBUG_ERROR(GSM_F_PREFIX"Cannot find the active xfer", fname);
                    break;
                }

                other_fcb->xcb = xcb;
                /*
                 * The xfr_call_id is the call that is being replaced by
                 * the cns_call_id.
                 */
                fsmxfr_update_xfr_context(xcb, xcb->cns_call_id,
                                          msg->data.xfer.target_call_id);
                xcb->cnf_xfr = TRUE;
                fsm_change_state(other_fcb, __LINE__, FSMXFR_S_ACTIVE);
                /*
                 * Emulating user hitting transfer key for second time
                 * in attended transfer
                 */
                cc_int_feature(CC_SRC_UI, CC_SRC_GSM, dcb->call_id,
                               dcb->line, CC_FEATURE_XFER, NULL);
            }

            fsm_change_state(fcb, __LINE__, FSMXFR_S_ACTIVE);
            break;

        default:
            fsm_sm_ignore_ftr(fcb, __LINE__, ftr_id);
            sm_rc = SM_RC_DEF_CONT;

            break;
        }                       /* switch (ftr_id) */

        break;

    case CC_SRC_GSM:
        switch (ftr_id) {
        case CC_FEATURE_NEW_CALL:

            /*
             * If this is the consultation call involved in a transfer,
             * then set the rest of the data required to make the transfer
             * happen. The data is the xcb in the fcb. The data is set now
             * because we did not have the fcb when the transfer was
             * initiated. The fcb is created when a new_call event is
             * received by the FIM, not when a transfer event is received.
             *
             * Or this could be the call that originated the
             * transfer (the transferor) and
             * the person he was talking to (the transfer target) has
             * decided to transfer the transferor to another target.
             */

            /*
             * Ignore this event if this call is not involved in a transfer.
             */
            xcb = fsmxfr_get_xcb_by_call_id(call_id);
            if (xcb == NULL) {
                break;
            }
            fcb->xcb = xcb;

            fsm_change_state(fcb, __LINE__, FSMXFR_S_ACTIVE);

            break;

        default:
            fsm_sm_ignore_ftr(fcb, __LINE__, ftr_id);
            sm_rc = SM_RC_DEF_CONT;

            break;
        }                       /* switch (ftr_id) */

        break;

    case CC_SRC_SIP:
        switch (ftr_id) {
        case CC_FEATURE_BLIND_XFER:
        case CC_FEATURE_XFER:
            if (msg->data_valid == FALSE) {
                break;
            }

            method = msg->data.xfer.method;

            switch (method) {
            case CC_XFER_METHOD_BYE:
                /*
                 * This is a remote initiated transfer using the
                 * BYE/ALSO method.
                 *
                 * The transferor has sent us, the transferee, a BYE with
                 * the transfer target.
                 *
                 * 1. Create a new transfer context
                 * 2. Ack the feature request.
                 *
                 * We need to wait for the RELEASE from SIP and then we will
                 * send out the NEWCALL to initiate the call to the target.
                 */

                /*
                 * Transfer is valid only for a remote originated transfer.
                 */
                if (msg->data.xfer.cause != CC_CAUSE_XFER_REMOTE) {
                    break;
                }

                /*
                 * Check for any other active features which may block
                 * the transfer.
                 */

                /*
                 * Get a new xcb and new xfr id - This is the handle that will
                 * identify the xfr.
                 */
                type = ((ftr_id == CC_FEATURE_XFER) ?
                        (FSMXFR_TYPE_XFR) : (FSMXFR_TYPE_BLND_XFR));

                xcb = fsmxfr_get_new_xfr_context(call_id, line, type, method,
                                                 FSMXFR_MODE_TRANSFEREE);
                if (xcb == NULL) {
                    break;
                }
                xcb->xfr_orig = src_id;
                fcb->xcb = xcb;

                fsmxfr_set_xfer_data(CC_CAUSE_OK, method,
                                     xcb->cns_call_id,
                                     FSMXFR_NULL_DIALSTRING, &(data.xfer));

                cc_int_feature_ack(CC_SRC_GSM, CC_SRC_SIP, dcb->call_id,
                                   dcb->line, ftr_id, &data, CC_CAUSE_NORMAL);

                /*
                 * Make sure we have a valid dialstring.
                 */
                if (fsmxfr_copy_dialstring(&xcb->dialstring,
                                           msg->data.xfer.dialstring) == TRUE) {
                    fsm_change_state(fcb, __LINE__, FSMXFR_S_ACTIVE);
                } else {
                    fsmxfr_feature_cancel(xcb, xcb->xfr_line, xcb->xfr_call_id,
                            xcb->cns_call_id, CC_SK_EVT_TYPE_IMPLI);
                    fsmxfr_cleanup(fcb, __LINE__, TRUE);
                }

                break;

            case CC_XFER_METHOD_REFER:
                /*
                 * This event is because:
                 * 1. we are the target involved in a transfer or
                 * 2. it is a remote initiated transfer using the REFER method.
                 *
                 * Case 1:
                 * The transferee has attempted to setup a call with us,
                 * the target. The call has been setup successfully so the
                 * SIP stack is informing us that we are the target.
                 *
                 * 1. Ack the request,
                 * 2. Create a new transfer context.
                 *
                 * Case 2:
                 * The transferor has sent us, the transferee, a REFER with
                 * the transfer target. We will attempt to contact the target
                 * and then report the result to the transferor.
                 *
                 * 1. Verify that we have a free line to start the
                 *    consultation call,
                 * 2. Create a new transfer context,
                 * 3. Place this call on hold,
                 * 4. Send a newcall feature back to the GSM so that the
                 *    consultation call can be initiated.
                 */

                /*
                 * Check if this is case 1.
                 * We know this because the target_call_id can only be set
                 * for this case.
                 */
                if ((msg->data.xfer.cause == CC_CAUSE_XFER_REMOTE) &&
                    (msg->data.xfer.target_call_id != CC_NO_CALL_ID)) {
                    type = ((ftr_id == CC_FEATURE_XFER) ?
                            (FSMXFR_TYPE_XFR) : (FSMXFR_TYPE_BLND_XFR));

                    xcb = fsmxfr_get_new_xfr_context(call_id, line, type,
                                                     CC_XFER_METHOD_REFER,
                                                     FSMXFR_MODE_TARGET);
                    if (xcb == NULL) {
                        break;
                    }
                    xcb->xfr_orig = src_id;
                    fcb->xcb = xcb;

                    /*
                     * The xfr_call_id is the call that is being replaced by
                     * the cns_call_id (which the stack supplied).
                     */
                    fsmxfr_update_xfr_context(xcb, xcb->cns_call_id,
                                              msg->data.xfer.target_call_id);
                    cns_call_id = xcb->cns_call_id;
                    /*
                     * Set the correct xfer_data. The target_call_id must be
                     * CC_NO_CALL_ID so that the stack can tell that this is
                     * a case 1 transfer.
                     */
                    fsmxfr_set_xfer_data(CC_CAUSE_XFER_REMOTE,
                                         method,
                                         CC_NO_CALL_ID,
                                         FSMXFR_NULL_DIALSTRING, &(data.xfer));

                    cc_int_feature_ack(CC_SRC_GSM, CC_SRC_SIP, dcb->call_id,
                                       dcb->line, ftr_id, &data,
                                       CC_CAUSE_NORMAL);

                    fsm_change_state(fcb, __LINE__, FSMXFR_S_ACTIVE);

                    break;
                }

                /*
                 * I guess we are at case 2...
                 * Transfer should be done only if call's present state
                 * is either on hold or connected
                 */
                fcb_def =
                    fsm_get_fcb_by_call_id_and_type(call_id, FSM_TYPE_DEF);
                if ((fcb_def->state == FSMDEF_S_CONNECTED) ||
                    (fcb_def->state == FSMDEF_S_CONNECTED_MEDIA_PEND) ||
                    (fcb_def->state == FSMDEF_S_RESUME_PENDING) ||
                    (fcb_def->state == FSMDEF_S_HOLD_PENDING) ||
                    (fcb_def->state == FSMDEF_S_HOLDING)) {
                    int_rc =
                        fsmxfr_remote_transfer(fcb, ftr_id, call_id, dcb->line,
                                               msg->data.xfer.dialstring,
                                               msg->data.xfer.referred_by);
                }

                if (int_rc == FALSE) {
                    fsmxfr_set_xfer_data(CC_CAUSE_ERROR, CC_XFER_METHOD_REFER,
                                         CC_NO_CALL_ID,
                                         FSMXFR_NULL_DIALSTRING, &(data.xfer));

                    cc_int_feature_ack(CC_SRC_GSM, CC_SRC_SIP, call_id,
                                       line, ftr_id, &data, CC_CAUSE_ERROR);
                }
                break;

            default:
                break;
            }                   /* switch (xfr_data->method) */

            break;
        case CC_FEATURE_NOTIFY:
            if (ftr_data->notify.subscription != CC_SUBSCRIPTIONS_XFER) {
                /* This notify is not for XFER subscription */
                break;
            }
            data.notify.cause        = msg->data.notify.cause;
            data.notify.cause_code   = msg->data.notify.cause_code;
            data.notify.subscription = CC_SUBSCRIPTIONS_XFER;
            data.notify.method       = CC_XFER_METHOD_REFER;
            data.notify.blind_xferror_gsm_id =
                msg->data.notify.blind_xferror_gsm_id;
            data.notify.final        = TRUE;

            if (data.notify.blind_xferror_gsm_id == CC_NO_CALL_ID) {
                if (msg->data.notify.cause == CC_CAUSE_OK) {
                    data.endcall.cause = CC_CAUSE_OK;
                    sm_rc = SM_RC_END;
                    /*
                     * If dcb is NULL, we received a final NOTIFY after the
                     * the dcb was cleared. This can happen when we receive
                     * BYE before final NOTIFY on the result of the REFER
                     * sent for xfer. If dcb is NULL, just quietly ignore
                     * the NOTIFY event. Otherwise, kick off the release of
                     * the call.
                     */
                    if (dcb) {
                        cc_int_feature(CC_SRC_GSM, CC_SRC_GSM,
                                       dcb->call_id, dcb->line,
                                       CC_FEATURE_END_CALL, &data);
                    }
                }
            } else {
                cc_int_feature(CC_SRC_GSM, CC_SRC_SIP,
                               data.notify.blind_xferror_gsm_id,
                               line, CC_FEATURE_NOTIFY, &data);
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
fsmxfr_ev_idle_dialstring (sm_event_t *event)
{
    fsm_fcb_t         *fcb     = (fsm_fcb_t *) event->data;
    cc_feature_t      *msg     = (cc_feature_t *) event->msg;
    callid_t           call_id = msg->call_id;
    fsmxfr_xcb_t      *xcb;
    sm_rcs_t           sm_rc   = SM_RC_CONT;

    /*
     * Ignore this event if this call is not involved in a transfer.
     */
    xcb = fsmxfr_get_xcb_by_call_id(call_id);
    if (xcb == NULL) {
        return sm_rc;
    }
    fcb->xcb = xcb;

    fsm_change_state(fcb, __LINE__, FSMXFR_S_ACTIVE);
    return (fsmxfr_ev_active_dialstring(event));
}




static sm_rcs_t
fsmxfr_ev_active_proceeding (sm_event_t *event)
{
    cc_proceeding_t   *msg     = (cc_proceeding_t *) event->msg;
    callid_t           call_id = msg->call_id;
    line_t             line    = msg->line;
    cc_feature_data_t  data;
    fsmxfr_xcb_t      *xcb;
    fsm_fcb_t         *other_fcb;
    callid_t           other_call_id;
    line_t             other_line;

    /*
     * Ignore this event if this call is not involved in a transfer
     * or we are not the target of the transfer.
     */
    xcb = fsmxfr_get_xcb_by_call_id(call_id);
    if ((xcb == NULL) || (xcb->mode != FSMXFR_MODE_TARGET)) {
        return (SM_RC_CONT);
    }

    other_call_id = fsmxfr_get_other_call_id(xcb, call_id);
    other_line = fsmxfr_get_other_line(xcb, call_id);
    other_fcb = fsm_get_fcb_by_call_id_and_type(other_call_id, FSM_TYPE_DEF);

    /*
     * Release the transfer call.
     *
     * We need to release the transfer call (which is really
     * the consultation call from the transferor to us
     * the target), because we want the next call coming in
     * to replace this one.
     */
    data.endcall.cause = CC_CAUSE_REPLACE;
    cc_int_feature(CC_SRC_GSM, CC_SRC_GSM, other_call_id,
                   other_line, CC_FEATURE_END_CALL, &data);

    /*
     * Only answer the call if the call being replaced was connected
     * or is currently connected, otherwise just let this call be setup
     * normally so that it will ring.
     */

    if (other_fcb && (other_fcb->old_state == FSMDEF_S_CONNECTED ||
        other_fcb->old_state == FSMDEF_S_CONNECTED_MEDIA_PEND ||
        other_fcb->old_state == FSMDEF_S_RESUME_PENDING ||
        other_fcb->state == FSMDEF_S_CONNECTED ||
        other_fcb->state == FSMDEF_S_CONNECTED_MEDIA_PEND ||
        other_fcb->state == FSMDEF_S_RESUME_PENDING)) {
        cc_int_feature(CC_SRC_UI, CC_SRC_GSM, call_id,
                       line, CC_FEATURE_ANSWER, NULL);
    }

    return (SM_RC_CONT);
}


static sm_rcs_t
fsmxfr_ev_active_connected_ack (sm_event_t *event)
{
    fsm_fcb_t          *fcb = (fsm_fcb_t *) event->data;
    cc_connected_ack_t *msg = (cc_connected_ack_t *) event->msg;
    fsmxfr_xcb_t       *xcb;

    /*
     * If we are the target and this is the call from the transferree
     * to the target, then we need to cleanup the rest of the transfer.
     * We need to do this because the consultation call is already cleared
     * from the transfer but the transfer call was not.
     */

    /*
     * Ignore this event if this call is not involved in a transfer.
     */
    xcb = fsmxfr_get_xcb_by_call_id(msg->call_id);
    if ((xcb == NULL) || (xcb->mode != FSMXFR_MODE_TARGET)) {
        return (SM_RC_CONT);
    }

    /*
     * Remove this call from the transfer.
     */
    fsmxfr_feature_cancel(xcb, xcb->xfr_line, xcb->xfr_call_id,
            xcb->cns_call_id, CC_SK_EVT_TYPE_IMPLI);
    fsmxfr_cleanup(fcb, __LINE__, FALSE);

    return (SM_RC_CONT);
}


static sm_rcs_t
fsmxfr_ev_active_release (sm_event_t *event)
{
    static const char fname[] = "fsmxfr_ev_active_release";
    fsm_fcb_t        *fcb     = (fsm_fcb_t *) event->data;
    cc_release_t     *msg     = (cc_release_t *) event->msg;
    callid_t          call_id = msg->call_id;
    fsmdef_dcb_t     *dcb     = fcb->dcb;
    callid_t          new_call_id;
    callid_t          other_call_id;
    line_t            other_line;
    fsmxfr_xcb_t     *xcb     = fcb->xcb;
    cc_feature_data_t data;
    boolean           secondary = FALSE;
    cc_action_data_t  action_data;
    fsm_fcb_t        *other_fcb;
    FSM_DEBUG_SM(DEB_F_PREFIX"Entered.", DEB_F_PREFIX_ARGS(FSM, "fsmxfr_ev_active_release"));

    /*
     * Complete a transfer if we have a pending transfer.
     */
    memset(&data, 0, sizeof(cc_feature_data_t));

    /*
     * Check if this is a transfer of a transfer.
     */
    if (xcb == NULL) {
        GSM_DEBUG_ERROR(GSM_F_PREFIX"Cannot find a transfer call to cancel.", fname);
        return (SM_RC_CONT);
    }

    if ((xcb->active == TRUE) && (xcb->xcb2 != NULL)) {
        xcb = xcb->xcb2;
        secondary = TRUE;
    }

    if ((xcb->dialstring != NULL) && (xcb->dialstring[0] != '\0')) {
        /*
         * Grab the call_id for the call to the target.
         * This will either already be in the xcb or we will need to
         * get a new one. The call_id will be in the xcb if we are the
         * transferee.
         */
        if (xcb->active == TRUE) {
            new_call_id = cc_get_new_call_id();
            fsmxfr_update_xfr_context(xcb, call_id, new_call_id);
        } else {
            new_call_id = fsmxfr_get_other_call_id(xcb, call_id);
            if (secondary == TRUE) {
                fsmxfr_update_xfr_context(fcb->xcb, call_id, new_call_id);
            }
        }

        data.newcall.cause = CC_CAUSE_XFER_REMOTE;
        sstrncpy(data.newcall.dialstring, xcb->dialstring,
                 CC_MAX_DIALSTRING_LEN);

        cpr_free(xcb->dialstring);
        xcb->dialstring = NULL;
        memset(data.newcall.redirect.redirects[0].number, 0,
               sizeof(CC_MAX_DIALSTRING_LEN));
        if (xcb->referred_by != NULL) {
            sstrncpy(data.newcall.redirect.redirects[0].number,
                     xcb->referred_by, CC_MAX_DIALSTRING_LEN);

            cpr_free(xcb->referred_by);
            xcb->referred_by = NULL;
        }

        cc_int_feature(CC_SRC_GSM, CC_SRC_GSM, new_call_id,
                       dcb->line, CC_FEATURE_NEW_CALL, &data);

        FSM_DEBUG_SM(get_debug_string(FSMXFR_DBG_XFR_INITIATED), xcb->xfr_id,
                     xcb->xfr_call_id, xcb->cns_call_id, __LINE__);

        if (secondary == TRUE) {
            fsmxfr_init_xcb(xcb);
            fcb->xcb->active = FALSE;
            fcb->xcb->xcb2 = NULL;
            fsmxfr_feature_cancel(xcb, xcb->xfr_line, xcb->xfr_call_id,
                    xcb->cns_call_id, CC_SK_EVT_TYPE_IMPLI);
            fsmxfr_cleanup(fcb, __LINE__, FALSE);
        } else if (xcb->active == TRUE) {
            fsmxfr_feature_cancel(xcb, xcb->xfr_line, xcb->xfr_call_id,
                    xcb->cns_call_id, CC_SK_EVT_TYPE_IMPLI);
            fsmxfr_cleanup(fcb, __LINE__, FALSE);
            fcb->xcb->active = FALSE;
        } else {
            /*
             * Reset the xcb call_id that was used for the call to the target.
             * The value was just temporary and it needs to be reset so that
             * the cleanup function works properly.
             */
            fsmxfr_update_xfr_context(xcb, new_call_id, CC_NO_CALL_ID);
            fsmxfr_feature_cancel(xcb, xcb->xfr_line, xcb->xfr_call_id,
                    xcb->cns_call_id, CC_SK_EVT_TYPE_IMPLI);
            fsmxfr_cleanup(fcb, __LINE__, TRUE);
        }

        xcb->active = FALSE;
    } else {
        if (secondary == TRUE) {
            /*
             * This is the secondary transfer of a primary transfer.
             * We need to:
             * 1. update the primary xcb to point to this just transferred
             *    call,
             * 2. mark the primary xcb as inactive since we just completed the
             *    secondary transfer,
             * 3. point the newly transferred call to the primary xcb and
             *    update the UI to show that this is a local transfer (the
             *    UI was set as though the call was a remote transfer).
             * 4. blow away this secondary xcb and cleanup the fcb,
             */
            new_call_id = fsmxfr_get_other_call_id(xcb, call_id);
            fsmxfr_update_xfr_context(fcb->xcb, call_id, new_call_id);

            fcb->xcb->active = FALSE;

            other_fcb = fsm_get_fcb_by_call_id_and_type(new_call_id,
                                                        FSM_TYPE_XFR);
            if (other_fcb == NULL) {
                return (SM_RC_CONT);
            }
            other_fcb->xcb = fcb->xcb;

            fsmxfr_init_xcb(xcb);

            action_data.update_ui.action = CC_UPDATE_XFER_PRIMARY;
            (void)cc_call_action(other_fcb->dcb->call_id, dcb->line,
                                 CC_ACTION_UPDATE_UI, &action_data);

            fsmxfr_cleanup(fcb, __LINE__, FALSE);
        } else {
            /*
             * One of the parties in the transfer has decided to release
             * the call, so go ahead and cleanup this transfer.
             */
            other_call_id = fsmxfr_get_other_call_id(xcb, call_id);
            other_line = fsmxfr_get_other_line(xcb, call_id);

            other_fcb = fsm_get_fcb_by_call_id_and_type(other_call_id,
                                                        FSM_TYPE_XFR);
            if (xcb->cnf_xfr) {
                /*
                 * This is the transfer for bridging a transfer call so
                 * clear the second line also.
                 */
                xcb->cnf_xfr = FALSE;
                if (other_fcb == NULL) {
                    return (SM_RC_CONT);
                }
                cc_int_feature(CC_SRC_GSM, CC_SRC_GSM, other_call_id,
                               other_line, CC_FEATURE_END_CALL, NULL);
            }

            fsmxfr_feature_cancel(xcb, xcb->xfr_line, xcb->xfr_call_id, xcb->cns_call_id,
                        CC_SK_EVT_TYPE_IMPLI);
            fsmxfr_cleanup(fcb, __LINE__, TRUE);

        }
    }

    return (SM_RC_CONT);
}


static sm_rcs_t
fsmxfr_ev_active_release_complete (sm_event_t *event)
{
    fsmxfr_cleanup((fsm_fcb_t *) event->data, __LINE__, TRUE);

    return (SM_RC_CONT);
}

static char *fsmxfr_get_dialed_num (fsmdef_dcb_t *dcb)
{
    static const char fname[] = "fsmxfr_get_dialed_num";
     char             *tmp_called_number;
    /* Get the dialed number only if the call is outgoing type */

    tmp_called_number = lsm_get_gdialed_digits();

    DEF_DEBUG(DEB_F_PREFIX"called_dialed_num = %s",
                DEB_F_PREFIX_ARGS(GSM, fname), tmp_called_number);

    /* Get dialed number to put in the refer-to header. If there
     * is no dialed number then use RPID or from header value
     */
    if (tmp_called_number == NULL || (*tmp_called_number) == NUL) {

        if (dcb->caller_id.called_number[0] != NUL) {
            DEF_DEBUG(DEB_F_PREFIX"called_dcb_num = %s",
                DEB_F_PREFIX_ARGS(GSM, fname), (char *)dcb->caller_id.called_number);
            return((char *)dcb->caller_id.called_number);

        } else {
            DEF_DEBUG(DEB_F_PREFIX"calling_dcb_num = %s",
                DEB_F_PREFIX_ARGS(GSM, fname), (char *)dcb->caller_id.calling_number);
            return((char *)dcb->caller_id.calling_number);
        }
    }

    /*
     * if tmp_called_number is same as what we receive in RPID,
     * then get the called name from RPID if provided.
     */
    if (dcb->caller_id.called_number != NULL &&
        dcb->caller_id.called_number[0] != NUL) {
        /* if Cisco PLAR string is used, use the RPID value */
        if (strncmp(tmp_called_number, CC_CISCO_PLAR_STRING, sizeof(CC_CISCO_PLAR_STRING)) == 0) {
            tmp_called_number = (char *)dcb->caller_id.called_number;
        }
    }

    return(tmp_called_number);
}

static void
fsmxfr_initiate_xfr (sm_event_t *event)
{
    static const char fname[] = "fsmxfr_initiate_xfr";
    fsm_fcb_t        *fcb     = (fsm_fcb_t *) event->data;
    fsm_fcb_t        *cns_fcb = NULL;
    fsmdef_dcb_t     *dcb     = fcb->dcb;
    fsmdef_dcb_t     *xfr_dcb;
    fsmdef_dcb_t     *cns_dcb;
    cc_feature_data_t data;
    fsmxfr_xcb_t     *xcb     = fcb->xcb;
    char             *called_num = NULL;

    /*
     * Place the consultation call on hold.
     */
    if (xcb == NULL) {
        GSM_DEBUG_ERROR(GSM_F_PREFIX"Cannot find the active xfer", fname);
        return;
    }

    cns_dcb = fsm_get_dcb(xcb->cns_call_id);
    cns_fcb = fsm_get_fcb_by_call_id_and_type(xcb->cns_call_id,
                                              FSM_TYPE_DEF);
    xfr_dcb = fsm_get_dcb(xcb->xfr_call_id);

    /*
     * If the consultation call is not connected
     * treat it like a blind xfer.
     */
    if (cns_fcb != NULL) {
        /*
         * If the transfer key is pressed twice, before the sofkey gets
         * updated in response to first transfer key, the 2nd transfer key
         * press is treated as a transfer complete and we try to initate
         * the transfer to the connected call itself. To prevent this, check
         * the state of the call to see if we should
         * ignore the 2nd transfer key press.
         */
        if ((cns_fcb->state == FSMDEF_S_COLLECT_INFO) ||
            (cns_fcb->state == FSMDEF_S_OUTGOING_PROCEEDING) ||
            (cns_fcb->state == FSMDEF_S_KPML_COLLECT_INFO)) {
            FSM_DEBUG_SM(DEB_L_C_F_PREFIX"Ignore the xfer xid %d cid %d %d",
				DEB_L_C_F_PREFIX_ARGS(FSM, xcb->xfr_line, xcb->xfr_call_id, "fsmxfr_initiate_xfr"),
                         xcb->xfr_id, xcb->xfr_call_id, xcb->cns_call_id);
            return;
        }

        /*
         * Indicate that xfer completion has been requested
         */
        xcb->xfer_comp_req = TRUE;

        if (cns_fcb->state < FSMDEF_S_CONNECTED) {
            data.endcall.cause = CC_CAUSE_NO_USER_RESP;
            cc_int_feature(CC_SRC_GSM, CC_SRC_GSM, cns_dcb->call_id,
                           cns_dcb->line, CC_FEATURE_END_CALL, &data);
            /*
             * Instruct the stack to transfer the call.
             */
            called_num = fsmxfr_get_dialed_num(cns_dcb);
            if (called_num && called_num[0] != '\0') {

                fsmxfr_set_xfer_data(CC_CAUSE_XFER_LOCAL,
                                     xcb->method, cns_dcb->call_id,
                                     called_num,
                                     &(data.xfer));

                cc_int_feature(CC_SRC_GSM, CC_SRC_SIP, xfr_dcb->call_id,
                               xfr_dcb->line, CC_FEATURE_XFER, &data);
            } else {
                /*
                 * Can't transfer the call without a dialstring, so
                 * just cleanup the transfer.
                 */
                fsmxfr_feature_cancel(xcb, xcb->xfr_line, xcb->xfr_call_id,
                                    xcb->cns_call_id, CC_SK_EVT_TYPE_IMPLI);
                fsmxfr_cleanup(fcb, __LINE__, TRUE);
                if (xcb->cnf_xfr) {
                    /*
                     * If it is a conference transfer clear up the
                     * calls.
                     */
                    fsmxfr_cnf_cleanup(xcb);
                }
            }
        } else {
            /*
             * If the consulation call is already on hold and
             * the intial call isn't then place ourselves on
             * hold (user hit the rocker arm switch). Otherwise,
             * place the consulation call on hold. Make sure we
             * do not send feature indication by setting the
             * call info type to none.
             */
            data.hold.call_info.type = CC_FEAT_NONE;
            data.hold.msg_body.num_parts = 0;
            if (((cns_fcb->state == FSMDEF_S_HOLDING) ||
                (cns_fcb->state == FSMDEF_S_HOLD_PENDING)) &&
                ((fcb->state != FSMDEF_S_HOLDING) &&
                (fcb->state != FSMDEF_S_HOLD_PENDING))) {
                cc_int_feature(CC_SRC_GSM, CC_SRC_GSM, dcb->call_id, dcb->line,
                               CC_FEATURE_HOLD, &data);
            } else {
                /* just place on hold */
                cc_int_feature(CC_SRC_GSM, CC_SRC_GSM,
                               cns_dcb->call_id, cns_dcb->line,
                               CC_FEATURE_HOLD, &data);
            }
        }
    }
}

static sm_rcs_t
fsmxfr_ev_active_feature (sm_event_t *event)
{
    static const char fname[] = "fsmxfr_ev_active_feature";
    fsm_fcb_t        *fcb     = (fsm_fcb_t *) event->data;
    cc_feature_t     *msg     = (cc_feature_t *) event->msg;
    callid_t          call_id = msg->call_id;
    line_t            line    = msg->line;
    fsmdef_dcb_t     *dcb     = fcb->dcb;
    cc_srcs_t         src_id  = msg->src_id;
    cc_features_t     ftr_id  = msg->feature_id;
    cc_feature_data_t *feat_data   = &(msg->data);
    fsmdef_dcb_t     *xfr_dcb, *cns_dcb;
    cc_feature_data_t data;
    sm_rcs_t          sm_rc   = SM_RC_CONT;
    fsmxfr_xcb_t     *xcb     = fcb->xcb;
    boolean           int_rc;
    char              tmp_str[STATUS_LINE_MAX_LEN];
    char             *called_num = NULL;
    fsm_fcb_t        *cns_fcb = NULL;

    fsm_sm_ftr(ftr_id, src_id);

    if (xcb == NULL) {
        GSM_DEBUG_ERROR(GSM_F_PREFIX"Cannot find the active xfer", fname);
        return (SM_RC_CONT);
    }

    /*
     * Consume the XFER and NOTIFY events and don't pass them along
     * to the other FSMs.
     */
    if ((ftr_id == CC_FEATURE_BLIND_XFER) ||
        (ftr_id == CC_FEATURE_XFER) || (ftr_id == CC_FEATURE_NOTIFY)) {
        if (ftr_id == CC_FEATURE_NOTIFY) {
            if (msg->data_valid &&
                (msg->data.notify.subscription != CC_SUBSCRIPTIONS_XFER)) {
                /* The subscription is not XFER, let the event flow through */
                return (SM_RC_CONT);
            }
        }
        sm_rc = SM_RC_END;
    }

    switch (src_id) {
    case CC_SRC_UI:
        switch (ftr_id) {
        case CC_FEATURE_CANCEL:
            sm_rc = SM_RC_END;
            fsmxfr_feature_cancel(xcb, xcb->xfr_line, xcb->xfr_call_id, xcb->cns_call_id,
                            CC_SK_EVT_TYPE_EXPLI);
            fsmxfr_cleanup(fcb, __LINE__, TRUE);
            break;

        case CC_FEATURE_XFER:
            /* Connect the existing call to active trasnfer state
             * machine. If the UI generates the event with target
             * call_id in the data then terminate the existing consulatative
             * call and link that to another call.
            */
            DEF_DEBUG(DEB_F_PREFIX"ACTIVE XFER call_id = %d, cns_id = %d, t_id=%d",
                DEB_F_PREFIX_ARGS(GSM, fname), xcb->xfr_call_id,
                    feat_data->xfer.target_call_id, xcb->cns_call_id);

            if (feat_data && msg->data_valid &&
                (xcb->cns_call_id != feat_data->xfer.target_call_id)) {

                cns_fcb = fsm_get_fcb_by_call_id_and_type(xcb->cns_call_id,
                        FSM_TYPE_DEF);

                if (cns_fcb != NULL) {
                    DEF_DEBUG(DEB_F_PREFIX"INVOKE ACTIVE XFER call_id = %d, t_id=%d",
                        DEB_F_PREFIX_ARGS(GSM, fname), xcb->xfr_call_id,
                        feat_data->xfer.target_call_id);
                    cc_int_feature(CC_SRC_GSM, CC_SRC_GSM, xcb->xfr_call_id,
                               xcb->xfr_line, CC_FEATURE_DIRTRXFR, NULL);
                }

                return(SM_RC_END);
            }
            switch (xcb->method) {
            case CC_XFER_METHOD_BYE:
            case CC_XFER_METHOD_REFER:
                /*
                 * This is the second transfer event for a local
                 * attended transfer with consultation.
                 *
                 * The user is attempting to complete the transfer, so
                 * 1. place the consultation call on hold,
                 * 2. instruct the stack to transfer the call.
                 */
                fsmxfr_initiate_xfr(event);
                lsm_set_hold_ringback_status(xcb->cns_call_id, FALSE);
                break;

            default:
                fsm_sm_ignore_ftr(fcb, __LINE__, ftr_id);
                break;
            }                   /* switch (ftr_id) { */
            break;
        case CC_FEATURE_RESUME:
            break;

        case CC_FEATURE_END_CALL:
            if (xcb->mode == FSMXFR_MODE_TRANSFEREE) {
                xfr_dcb = fsm_get_dcb(xcb->xfr_call_id);
                if (call_id == xcb->cns_call_id) {
                    /*
                     * Transferee ended call before transfer was completed.
                     * Notify the transfer call of the status of the
                     * transfer.
                     */
                    data.notify.cause        = CC_CAUSE_ERROR;
                    data.notify.subscription = CC_SUBSCRIPTIONS_XFER;
                    data.notify.method       = CC_XFER_METHOD_REFER;
                    data.notify.final        = TRUE;
                    cc_int_feature(CC_SRC_GSM, CC_SRC_SIP, xfr_dcb->call_id,
                                   xfr_dcb->line, CC_FEATURE_NOTIFY, &data);
                }
            }
            lsm_set_hold_ringback_status(xcb->cns_call_id, TRUE);
            fsmxfr_feature_cancel(xcb, xcb->xfr_line, xcb->xfr_call_id, xcb->cns_call_id,
                                CC_SK_EVT_TYPE_IMPLI);
            fsmxfr_cleanup(fcb, __LINE__, TRUE);
            break;

        case CC_FEATURE_HOLD:
            if ((msg->data_valid) &&
                (feat_data->hold.call_info.data.hold_resume_reason == CC_REASON_SWAP ||
                feat_data->hold.call_info.data.hold_resume_reason == CC_REASON_XFER ||
                feat_data->hold.call_info.data.hold_resume_reason == CC_REASON_INTERNAL))
            {
                feat_data->hold.call_info.data.call_info_feat_data.protect = TRUE;
            } else {
                DEF_DEBUG(DEB_F_PREFIX"Invoke hold call_id = %d t_call_id=%d",
                        DEB_F_PREFIX_ARGS(GSM, fname), xcb->xfr_call_id, xcb->cns_call_id);
                //Actual hold to this call, so break the feature layer.
                ui_terminate_feature(xcb->xfr_line, xcb->xfr_call_id, xcb->cns_call_id);
                fsmxfr_feature_cancel(xcb, xcb->xfr_line, xcb->xfr_call_id,
                            xcb->cns_call_id, CC_SK_EVT_TYPE_IMPLI);
                fsmxfr_cleanup(fcb, __LINE__, TRUE);
            }
            fsm_sm_ignore_ftr(fcb, __LINE__, ftr_id);
            break;

        default:
            fsm_sm_ignore_ftr(fcb, __LINE__, ftr_id);
            break;
        }                       /* switch (ftr_id) */

        break;

    case CC_SRC_GSM:
        switch (ftr_id) {
        case CC_FEATURE_DIRTRXFR:
            xfr_dcb = fsm_get_dcb(xcb->xfr_call_id);
            called_num = fsmxfr_get_dialed_num(xfr_dcb);

            if (called_num && called_num[0] != '\0') {
                fsmxfr_set_xfer_data(CC_CAUSE_XFER_LOCAL,
                        xcb->method, xcb->cns_call_id,
                        called_num,
                        &(data.xfer));

                data.xfer.method = CC_XFER_METHOD_DIRXFR;

                cc_int_feature(CC_SRC_GSM, CC_SRC_SIP,
                        call_id, line,
                        CC_FEATURE_XFER, &data);
            } else {
                /*
                * Can't transfer the call without a dialstring, so
                * just cleanup the transfer.
                */
                fsmxfr_feature_cancel(xcb, xcb->xfr_line, xcb->xfr_call_id,
                                    xcb->cns_call_id, CC_SK_EVT_TYPE_IMPLI);
                fsmxfr_cleanup(fcb, __LINE__, TRUE);
            }
            return(SM_RC_END);
        case CC_FEATURE_END_CALL:
            /*
             * Only cleanup the whole xfer if we know that we don't have
             * an outstanding blind transfer and if we are not the target.
             * We need the xcb to hang around because other users (lsm,...)
             * may still want to do something special and the xcb is the only
             * way they know that the call is involved in a transfer.
             */
            if (xcb->type == FSMXFR_TYPE_BLND_XFR) {
                fsmxfr_cleanup(fcb, __LINE__, FALSE);
            } else if ((xcb->type == FSMXFR_TYPE_XFR) &&
                       (msg->data.endcall.cause == CC_CAUSE_NO_USER_RESP)) {

                DEF_DEBUG(DEB_F_PREFIX"Xfer type =%d",
                        DEB_F_PREFIX_ARGS(GSM, fname), xcb->type);

                if ((platGetPhraseText(STR_INDEX_TRANSFERRING,
                                             (char *) tmp_str,
                                             STATUS_LINE_MAX_LEN - 1)) == CPR_SUCCESS) {
                    lsm_ui_display_status(tmp_str, xcb->xfr_line, xcb->xfr_call_id);
                }

                if (xcb->xfer_comp_req == FALSE) {
                    fsmxfr_feature_cancel(xcb, xcb->xfr_line, xcb->xfr_call_id,
                                    xcb->cns_call_id, CC_SK_EVT_TYPE_IMPLI);
                } else {
                    // Mark it as transfer complete.
                    fsmxfr_mark_dcb_for_xfr_complete(xcb->cns_call_id,
                                    xcb->xfr_call_id, TRUE);
                }
                fsmxfr_cleanup(fcb, __LINE__, FALSE);
            } else if (xcb->mode == FSMXFR_MODE_TARGET) {
                break;
            } else {
                /* Early attended transfer generates internal END_CALL event
                 * do not send cancel in that case
                 */
                if (xcb->xfer_comp_req == FALSE) {
                    fsmxfr_feature_cancel(xcb, xcb->xfr_line, xcb->xfr_call_id,
                                    xcb->cns_call_id, CC_SK_EVT_TYPE_IMPLI);
                }
                fsmxfr_cleanup(fcb, __LINE__, TRUE);
            }
            break;

        case CC_FEATURE_HOLD:
            ui_set_local_hold(dcb->line, dcb->call_id);
            if(msg->data_valid) {
                feat_data->hold.call_info.data.call_info_feat_data.protect = TRUE;
            }
            fsm_sm_ignore_ftr(fcb, __LINE__, ftr_id);
            break;

        default:
            fsm_sm_ignore_ftr(fcb, __LINE__, ftr_id);
            break;
        }                       /* switch (ftr_id) */
        break;

    case CC_SRC_SIP:
        switch (ftr_id) {
        case CC_FEATURE_CALL_PRESERVATION:
            DEF_DEBUG(DEB_F_PREFIX"Preservation call_id = %d t_call_id=%d",
                        DEB_F_PREFIX_ARGS(GSM, fname), xcb->xfr_call_id, xcb->cns_call_id);
            ui_terminate_feature(xcb->xfr_line, xcb->xfr_call_id, xcb->cns_call_id);
            fsmxfr_feature_cancel(xcb, xcb->xfr_line, xcb->xfr_call_id,
                            xcb->cns_call_id, CC_SK_EVT_TYPE_IMPLI);
            fsmxfr_cleanup(fcb, __LINE__, TRUE);
            break;

        case CC_FEATURE_BLIND_XFER:
        case CC_FEATURE_XFER:
            if (msg->data_valid == FALSE) {
                break;
            }

            /*
             * Transfer is valid only for a remote originated transfer.
             */
            if (msg->data.xfer.cause != CC_CAUSE_XFER_REMOTE) {
                break;
            }

            /*
             * This call is already involved in a transfer as the transferor,
             * but one of the parties, the transferee or target has decided to
             * transfer that leg also. So, now we have a transfer of a transfer.
             */
            switch (msg->data.xfer.method) {
            case CC_XFER_METHOD_BYE:
                /*
                 * Check for any other active features which may block
                 * the transfer.
                 */

                fsmxfr_set_xfer_data(CC_CAUSE_OK, CC_XFER_METHOD_BYE,
                                     CC_NO_CALL_ID,
                                     FSMXFR_NULL_DIALSTRING, &(data.xfer));

                cc_int_feature_ack(CC_SRC_GSM, CC_SRC_SIP, dcb->call_id,
                                   dcb->line, ftr_id, &data, CC_CAUSE_NORMAL);

                /*
                 * Make sure we have a valid dialstring.
                 */
                if (fsmxfr_copy_dialstring(&xcb->dialstring,
                                           msg->data.xfer.dialstring) == FALSE) {
                    fsmxfr_feature_cancel(xcb, xcb->xfr_line, xcb->xfr_call_id, xcb->cns_call_id,
                                CC_SK_EVT_TYPE_IMPLI);
                    fsmxfr_cleanup(fcb, __LINE__, TRUE);
                    break;
                }

                /*
                 * Mark the active flag in the xcb. This flag is used later
                 * when the RELEASE comes from SIP, so that the GSM
                 * knows that this call is still involved in a transfer.
                 */
                xcb->active = TRUE;

                break;

            case CC_XFER_METHOD_REFER:
                int_rc = fsmxfr_remote_transfer(fcb, ftr_id, call_id, dcb->line,
                                                msg->data.xfer.dialstring,
                                                msg->data.xfer.referred_by);

                if (int_rc == FALSE) {
                    fsmxfr_set_xfer_data(CC_CAUSE_ERROR, CC_XFER_METHOD_REFER,
                                         CC_NO_CALL_ID,
                                         FSMXFR_NULL_DIALSTRING, &(data.xfer));

                    cc_int_feature_ack(CC_SRC_GSM, CC_SRC_SIP, call_id,
                                       dcb->line, ftr_id, &data,
                                       CC_CAUSE_ERROR);
                }
                break;

            default:
                break;
            }                   /* switch (msg->data.xfer.method) */

            break;

        case CC_FEATURE_NOTIFY:
            /*
             * This could be:
             * 1. for an unattended transfer.
             *    The transferee is notifying us, the transferor, of the status
             *    of the transfer, ie. was the transferee able to connect to
             *    the target?
             *
             * 2. Or this is an attended transfer, and this is the call from
             *    the transferee to the target. The stack will NOTIFY the GSM
             *    of the status of the call after it receives a message
             *    from the network indicating success of failure.
             */
            if (msg->data_valid == FALSE) {
                break;
            }

            /*
             * Ack the request.
             */
            cc_int_feature_ack(CC_SRC_GSM, CC_SRC_SIP, dcb->call_id,
                               dcb->line, CC_FEATURE_NOTIFY, NULL,
                               CC_CAUSE_NORMAL);

            switch (xcb->type) {
            case FSMXFR_TYPE_BLND_XFR:
                switch (msg->data.notify.method) {
                case CC_XFER_METHOD_BYE:
                    /*
                     * This notification is really from the SIP stack.
                     * The network will not send a NOTIFY for a BYE/Also
                     * transfer, so the SIP stack just sends one up when
                     * it uses that method.
                     */

                    /*
                     * Release the transfer call.
                     */
                    data.endcall.cause = CC_CAUSE_OK;
                    cc_int_feature(CC_SRC_GSM, CC_SRC_GSM, dcb->call_id,
                                   dcb->line, CC_FEATURE_END_CALL, &data);
                    break;

                case CC_XFER_METHOD_REFER:
                    if (msg->data.notify.cause == CC_CAUSE_OK) {
                        /*
                         * Release the transfer call.
                         */
                        /* Set the dcb flag to indicate transfer is complete, so that
                         * it won't display endcall in this case
                         */
                        fsmxfr_mark_dcb_for_xfr_complete(xcb->cns_call_id,
                                    xcb->xfr_call_id, TRUE);

                        data.endcall.cause = CC_CAUSE_OK;
                        cc_int_feature(CC_SRC_GSM, CC_SRC_GSM, dcb->call_id,
                                       dcb->line, CC_FEATURE_END_CALL, &data);
                    } else {
                        fsmxfr_cleanup(fcb, __LINE__, TRUE);
                        cc_int_feature(CC_SRC_GSM, CC_SRC_GSM, dcb->call_id,
                                       dcb->line, CC_FEATURE_RESUME, NULL);
                    }
                    break;

                default:
                    break;
                }               /* switch (msg->data.notify.method) { */

                break;
            case FSMXFR_TYPE_DIR_XFR:
                /*
                 * Clear the transfer call if the transfer was OK,
                 * else just cleanup the transfer. The consultation
                 * call will be released by the target.
                */
                xfr_dcb = fsm_get_dcb(xcb->xfr_call_id);
                if (msg->data.notify.cause == CC_CAUSE_OK) {
                        data.endcall.cause = CC_CAUSE_OK;

                    fsmxfr_mark_dcb_for_xfr_complete(xcb->cns_call_id,
                                    xcb->xfr_call_id, TRUE);
                    cc_int_feature(CC_SRC_GSM, CC_SRC_GSM,
                        xfr_dcb->call_id,
                        xfr_dcb->line, CC_FEATURE_END_CALL,
                        &data);
                } else {
                    lsm_ui_display_status(platform_get_phrase_index_str(TRANSFER_FAILED),
                            dcb->line, xcb->xfr_call_id);
                    lsm_ui_display_status(platform_get_phrase_index_str(TRANSFER_FAILED),
                            dcb->line, xcb->cns_call_id);
                }
                if (xcb->cnf_xfr) {
                    /*
                     * If it is a conference transfer clear up the
                     * calls.
                     */
                    fsmxfr_cnf_cleanup(xcb);
                }
                break;

            case FSMXFR_TYPE_XFR:
                switch (msg->data.notify.method) {
                case CC_XFER_METHOD_BYE:
                    /*
                     * Release the consultation call.
                     */
                    cc_int_feature(CC_SRC_GSM, CC_SRC_GSM, xcb->cns_call_id,
                                   dcb->line, CC_FEATURE_END_CALL, NULL);

                    /*
                     * Release the call being transferred.
                     */
                    cc_int_feature(CC_SRC_GSM, CC_SRC_GSM, xcb->xfr_call_id,
                                   dcb->line, CC_FEATURE_END_CALL, NULL);
                    break;

                case CC_XFER_METHOD_REFER:
                    /*
                     * This notification is from either the target (which is
                     * the consultation call) or the transferee (which is the
                     * transfer call).
                     *
                     * So, do the following based on each case:
                     *
                     * 1. consultation call: this is the transferee receiving
                     *    notification of the status of the call from the
                     *    transferee to the target.
                     *    - notify the transfer call.
                     *    - the transfer call will be released by the transferor
                     *      after receiving the above notification and the
                     *      consultation call will remain.
                     *
                     * 2. transfer call: this is the transferor receiving
                     *    notification of the status of the call from the
                     *    transferee to the target.
                     *    - release the transfer call.
                     *    - the consultation call will be released by the target.
                     */
                    xfr_dcb = fsm_get_dcb(xcb->xfr_call_id);
                    if (call_id == xcb->cns_call_id) {
                        /*
                         * Notify the transfer call of the status of the
                         * transfer.
                         */
                        data.notify.cause        = msg->data.notify.cause;
                        data.notify.cause_code   = msg->data.notify.cause_code;
                        data.notify.subscription = CC_SUBSCRIPTIONS_XFER;
                        data.notify.method       = CC_XFER_METHOD_REFER;
                        data.notify.blind_xferror_gsm_id =
                            msg->data.notify.blind_xferror_gsm_id;
                        data.notify.final        = TRUE;
                        if (data.notify.blind_xferror_gsm_id == CC_NO_CALL_ID) {
                            cc_int_feature(CC_SRC_GSM, CC_SRC_SIP,
                                           xfr_dcb->call_id, xfr_dcb->line,
                                           CC_FEATURE_NOTIFY, &data);
                        } else {
                            cc_int_feature(CC_SRC_GSM, CC_SRC_SIP,
                                           data.notify.blind_xferror_gsm_id,
                                           msg->line, CC_FEATURE_NOTIFY, &data);
                        }
                    } else {
                        /*
                         * Clear the transfer call if the transfer was OK,
                         * else just cleanup the transfer. The consultation
                         * call will be released by the target.
                         */
                        if (xcb == NULL) {
                            GSM_DEBUG_ERROR(GSM_F_PREFIX"Cannot find the active xfer", fname);
                            break;
                        }

                        if (msg->data.notify.cause == CC_CAUSE_OK) {
                            data.endcall.cause = CC_CAUSE_OK;

                            fsmxfr_mark_dcb_for_xfr_complete(xcb->cns_call_id,
                                        xcb->xfr_call_id, TRUE);
                            cc_int_feature(CC_SRC_GSM, CC_SRC_GSM,
                                           xfr_dcb->call_id,
                                           xfr_dcb->line, CC_FEATURE_END_CALL,
                                           &data);
                        } else {
                            lsm_ui_display_status(platform_get_phrase_index_str(TRANSFER_FAILED),
                                                  dcb->line, xcb->xfr_call_id);
                            fsmxfr_mark_dcb_for_xfr_complete(xcb->cns_call_id,
                                        xcb->xfr_call_id, FALSE);
                            fsmxfr_feature_cancel(xcb, xcb->xfr_line, xcb->xfr_call_id,
                                                      xcb->cns_call_id, CC_SK_EVT_TYPE_IMPLI);
                            /*
                             * Resume the consultation call.
                             * if xcb->cns_call_id == 0, then cns call is already ended.
                             */
                            if (xcb->cns_call_id != CC_NO_CALL_ID) {
                               	lsm_ui_display_status(platform_get_phrase_index_str(TRANSFER_FAILED),
                                                      dcb->line, xcb->cns_call_id);
                                cns_dcb = fsm_get_dcb (xcb->cns_call_id);
                                cc_int_feature(CC_SRC_GSM, CC_SRC_GSM,
                                               xcb->cns_call_id, cns_dcb->line,
                                               CC_FEATURE_RESUME, NULL);
                            }

                            if (xcb->cnf_xfr) {
                                /*
                                 * If it is a conference transfer clear up the
                                 * calls.
                                 */
                                fsmxfr_cnf_cleanup(xcb);
                            }
                        }
                        fsmxfr_cleanup(fcb, __LINE__, TRUE);
                    }           /* if (call_id == xcb->cns_call_id)  */
                    break;

                default:
                    break;
                }               /* switch (msg->data.notify.method) { */

                break;

            default:
                break;
            }                   /* switch (xcb->type) { */
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

static void
fsmxfr_requeue_blind_xfer_dialstring (fsmdef_dcb_t *dcb, fsmxfr_xcb_t *xcb)
{
    /*
     * This is the result of the feature hold so we
     * can clear the active feature.
     */
    dcb->active_feature = CC_FEATURE_NONE;

    /*
     * If there is a dialstring on the xcb, it is because the
     * dialstring event for the consultative call has already
     * been received. We delayed handling the dialstring until
     * the result of the hold request has been received. We
     * are now ready to process the dial string so requeue it
     * for processing
     */
    if (xcb->queued_dialstring && xcb->queued_dialstring[0] != '\0') {
        cc_dialstring(CC_SRC_UI, xcb->cns_call_id, dcb->line,
                      xcb->queued_dialstring);
        cpr_free(xcb->queued_dialstring);
        xcb->queued_dialstring = NULL;
    }
}

static sm_rcs_t
fsmxfr_ev_active_feature_ack (sm_event_t *event)
{
    static const char fname[] = "fsmxfr_ev_active_feature_ack";
    fsm_fcb_t        *fcb     = (fsm_fcb_t *) event->data;
    cc_feature_ack_t *msg     = (cc_feature_ack_t *) event->msg;
    cc_srcs_t         src_id  = msg->src_id;
    cc_features_t     ftr_id  = msg->feature_id;
    fsmdef_dcb_t     *dcb     = fcb->dcb;
    cc_feature_data_t data;
    sm_rcs_t          sm_rc   = SM_RC_CONT;
    fsmxfr_xcb_t     *xcb     = fcb->xcb;
    fsmdef_dcb_t     *xfr_dcb = NULL;
    char             *called_num = NULL;

    fsm_sm_ftr(ftr_id, src_id);

    /*
     * Consume the XFER events and don't pass them along to the other FSMs.
     */
    if ((ftr_id == CC_FEATURE_BLIND_XFER) || (ftr_id == CC_FEATURE_XFER)) {
        sm_rc = SM_RC_END;
    }

    if (xcb == NULL) {
        GSM_DEBUG_ERROR(GSM_F_PREFIX"Cannot find the active xfer", fname);
        return (sm_rc);
    }

    switch (src_id) {
    case CC_SRC_SIP:
    case CC_SRC_GSM:
        switch (ftr_id) {
        case CC_FEATURE_BLIND_XFER:
            switch (xcb->type) {
            case FSMXFR_TYPE_BLND_XFR:
                switch (msg->data.xfer.method) {
                case CC_XFER_METHOD_REFER:
                    /*
                     * Clear the call if the transfer was OK, else just cleanup
                     * the transfer.
                     */
                    if (msg->cause == CC_CAUSE_OK) {
                        // This does not indicate that transfer was successful. So wait for the NOTIFYs.
                        //data.endcall.cause = CC_CAUSE_OK;

                        //cc_int_feature(CC_SRC_GSM, CC_SRC_GSM, dcb->call_id,
                                       //dcb->line, CC_FEATURE_END_CALL, &data);
                    } else {
                        fsmxfr_feature_cancel(xcb, xcb->xfr_line, xcb->xfr_call_id,
                            xcb->cns_call_id, CC_SK_EVT_TYPE_IMPLI);
                        fsmxfr_cleanup(fcb, __LINE__, TRUE);
                    }
                    break;

                default:
                    fsm_sm_ignore_ftr(fcb, __LINE__, ftr_id);
                    break;
                }               /* switch (msg->data.xfer.method) { */

                break;

            default:
                break;
            }                   /* switch (xcb->type) { */
            break;

        case CC_FEATURE_HOLD:
            if (msg->cause == CC_CAUSE_REQUEST_PENDING) {
                /*
                 * HOLD request is pending. Let this event drop through
                 * to the default sm for handling.
                 */
                fsm_sm_ignore_ftr(fcb, __LINE__, ftr_id);
                break;
            }

            /*
             * If this is the hold response during a blind transfer,
             * check to see if a dialstring has been queued for processing.
             */
            if (xcb->type == FSMXFR_TYPE_BLND_XFR &&
                xcb->xfr_call_id == fcb->call_id) {
                fsmxfr_requeue_blind_xfer_dialstring(dcb, xcb);
                break;
            }

            /*
             * If xfer soft key has not been pressed a second time then
             * don't transfer the call.
             */
            if (!xcb->xfer_comp_req) {
                break;
            }


            /* If there is any error reported from SIP stack then clear the
             * transfer data. One such case is where digest authentication
             * fails due to maximum retry (of 2). SIP stack generates a valid
             * ACK event by setting cause code to ERROR
             */
            if (msg->cause == CC_CAUSE_ERROR) {

                lsm_ui_display_status(platform_get_phrase_index_str(TRANSFER_FAILED),
                                      xcb->xfr_line, xcb->xfr_call_id);
                lsm_ui_display_status(platform_get_phrase_index_str(TRANSFER_FAILED),
                                      xcb->cns_line, xcb->cns_call_id);
                fsmxfr_feature_cancel(xcb, xcb->xfr_line, xcb->xfr_call_id, xcb->cns_call_id,
                                CC_SK_EVT_TYPE_IMPLI);
                fsmxfr_cleanup(fcb, __LINE__, TRUE);

                break;
            }

            /*
             * Instruct the stack to transfer the call.
             */
            if (xcb->type == FSMXFR_TYPE_XFR) {
                /*
                 * Check to see which side of the transfer
                 * the request is coming from. if it is from
                 * the XFR side, make sure the CNS side is
                 * indeed in the Held state.
                 */
                if (xcb->cns_call_id == fcb->call_id) {
                    xfr_dcb = fsm_get_dcb(xcb->xfr_call_id);

                    called_num = fsmxfr_get_dialed_num(fcb->dcb);

                    if (called_num && called_num[0] != '\0') {
                        fsmxfr_set_xfer_data(CC_CAUSE_XFER_LOCAL,
                                             xcb->method, fcb->dcb->call_id,
                                             called_num,
                                             &(data.xfer));
                        cc_int_feature(CC_SRC_GSM, CC_SRC_SIP, xfr_dcb->call_id,
                                       xfr_dcb->line, CC_FEATURE_XFER, &data);
                    } else {
                        /*
                         * Can't transfer the call without a dialstring, so
                         * just cleanup the transfer.
                         */
                        fsmxfr_feature_cancel(xcb, xcb->xfr_line, xcb->xfr_call_id,
                                                xcb->cns_call_id, CC_SK_EVT_TYPE_IMPLI);
                        fsmxfr_cleanup(fcb, __LINE__, TRUE);
                    }
                } else {
                    /*
                     * This is hold response on the xfer call leg.
                     *
                     * Check to see if dialstring has been queued for
                     * processing.  If so, it is requeued to GSM and
                     * we delay until the consultative call is setup.
                     */
                    //if (fsmxfr_requeue_blind_xfer_dialstring(dcb, xcb)) {
                    //    break;
                    //}

                    /*
                     * Get the dcb of the consultative call.
                     */
                    xfr_dcb = fsm_get_dcb(xcb->cns_call_id);

                    /*
                     * We must wait for the hold request on the consultative
                     * call to complete before completing the xfer. The state
                     * of the consultative call must be FSMDEF_S_HOLDING.
                     * FSMDEF_S_HOLDING is not a completed hold request which
                     * is why it is not checked for here.
                     */
                    if (xfr_dcb->fcb->state != FSMDEF_S_HOLDING) {
                        break;
                    } else {

                        called_num = fsmxfr_get_dialed_num(xfr_dcb);

                        if (called_num && called_num[0] != '\0') {
                            fsmxfr_set_xfer_data(CC_CAUSE_XFER_LOCAL,
                                                 xcb->method, xfr_dcb->call_id,
                                                 called_num,
                                                 &(data.xfer));
                            cc_int_feature(CC_SRC_GSM, CC_SRC_SIP,
                                           fcb->dcb->call_id, fcb->dcb->line,
                                           CC_FEATURE_XFER, &data);
                        } else {
                            /*
                             * Can't transfer the call without a dialstring, so
                             * just cleanup the transfer.
                             */
                            fsmxfr_feature_cancel(xcb, xcb->xfr_line, xcb->xfr_call_id,
                                                    xcb->cns_call_id, CC_SK_EVT_TYPE_IMPLI);
                            fsmxfr_cleanup(fcb, __LINE__, TRUE);
                        }
                    }
                }
            }
            break;

        case CC_FEATURE_XFER:

            if (msg->cause != CC_CAUSE_OK) {
                lsm_ui_display_status(platform_get_phrase_index_str(TRANSFER_FAILED),
                                      xcb->xfr_line, xcb->xfr_call_id);
                lsm_ui_display_status(platform_get_phrase_index_str(TRANSFER_FAILED),
                                      xcb->cns_line, xcb->cns_call_id);
                fsmxfr_feature_cancel(xcb, xcb->xfr_line, xcb->xfr_call_id,
                                    xcb->cns_call_id, CC_SK_EVT_TYPE_IMPLI);
                fsmxfr_cleanup(fcb, __LINE__, TRUE);
            } else {
                fsm_sm_ignore_ftr(fcb, __LINE__, ftr_id);
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


static sm_rcs_t
fsmxfr_ev_active_onhook (sm_event_t *event)
{
    static const char fname[] = "fsmxfr_ev_active_onhook";
    fsm_fcb_t        *fcb     = (fsm_fcb_t *) event->data;
    cc_onhook_t      *msg     = (cc_onhook_t *) event->msg;
    callid_t          call_id = msg->call_id;
    callid_t          other_call_id;
    fsmxfr_xcb_t     *xcb     = fcb->xcb;
    fsm_fcb_t        *other_fcb;
    fsmdef_dcb_t     *xfr_dcb;
    cc_feature_data_t data;
    fsm_fcb_t        *cns_fcb, *xfr_fcb;
    int               onhook_xfer = 0;

    if (xcb == NULL) {
        GSM_DEBUG_ERROR(GSM_F_PREFIX"Cannot find the active xfer", fname);
        return (SM_RC_CONT);
    }

    cns_fcb = fsm_get_fcb_by_call_id_and_type(xcb->cns_call_id, FSM_TYPE_DEF);
    xfr_fcb = fsm_get_fcb_by_call_id_and_type(xcb->xfr_call_id, FSM_TYPE_DEF);

    if (xcb->cnf_xfr) {
        /*
         * This is the conference transfer so clear the
         * second line also.
         */
        xcb->cnf_xfr = FALSE;
        other_call_id = fsmxfr_get_other_call_id(xcb, call_id);
        other_fcb = fsm_get_fcb_by_call_id_and_type(other_call_id,
                                                    FSM_TYPE_XFR);
        cc_int_feature(CC_SRC_GSM, CC_SRC_GSM, other_call_id,
                       other_fcb ? other_fcb->dcb->line:CC_NO_LINE, CC_FEATURE_END_CALL, NULL);
        fsmxfr_cleanup(fcb, __LINE__, TRUE);
        return (SM_RC_CONT);
    }

    if (xcb->mode == FSMXFR_MODE_TRANSFEREE) {
        xfr_dcb = fsm_get_dcb(xcb->xfr_call_id);
        if (call_id == xcb->cns_call_id) {
            /*
             * Transferee ended call before transfer was completed.
             * Notify the transfer call of the status of the
             * transfer.
             *
             * Note: This must be changed when fix is put in for configurable
             *       onhook xfer (CSCsb86757) so that 200 NOTIFY is sent when
             *       xfer is completed. fsmxfr_initiate_xfr will take care
             *       of this for us so just need to make sure the NOTIFY is
             *       sent to SIP stack only when xfer is abandoned due to
             *       onhook.
             */
            data.notify.cause        = CC_CAUSE_ERROR;
            data.notify.subscription = CC_SUBSCRIPTIONS_XFER;
            data.notify.method       = CC_XFER_METHOD_REFER;
            data.notify.final        = TRUE;
            cc_int_feature(CC_SRC_GSM, CC_SRC_SIP, xfr_dcb->call_id,
                           xfr_dcb->line, CC_FEATURE_NOTIFY, &data);
            if (cns_fcb && cns_fcb->state != FSMDEF_S_HOLDING &&
                cns_fcb->state != FSMDEF_S_HOLD_PENDING) {
                fsmxfr_feature_cancel(xcb, xfr_dcb->line,
                                    xcb->xfr_call_id, xcb->cns_call_id, CC_SK_EVT_TYPE_IMPLI);

                fsmxfr_cleanup(fcb, __LINE__, TRUE);
            }
            /*
             * fix bug CSCtb23681.
            */
            if( xfr_dcb->fcb->state == FSMDEF_S_HOLDING )
                cc_int_feature(CC_SRC_GSM, CC_SRC_GSM, xfr_dcb->call_id,xfr_dcb->line, CC_FEATURE_END_CALL, NULL);
            return (SM_RC_CONT);
        }
    }

    if (msg->softkey) {
        /*
         * Softkey set to TRUE indicates endcall softkey was pressed.
         * This causes the call with focus to release.
         */
        if ((call_id == xcb->cns_call_id) &&
            (cns_fcb->state == FSMDEF_S_HOLDING ||
             cns_fcb->state == FSMDEF_S_HOLD_PENDING)) {
            /* ignore the onhook event for the held consultation call */
        } if (msg->active_list == CC_REASON_ACTIVECALL_LIST) {
            /* Active call list has been requested also
             * existing consult call is canceled
             * But transfer state machine will remain
             * intact as feature layer is still running.*/
            xcb->cns_call_id = CC_NO_CALL_ID;
            xcb->cns_line = CC_NO_LINE;

        }else {
            fsmxfr_feature_cancel(xcb, xcb->xfr_line,
                                    xcb->xfr_call_id, xcb->cns_call_id, CC_SK_EVT_TYPE_IMPLI);

            fsmxfr_cleanup(fcb, __LINE__, TRUE);
        }
        return (SM_RC_CONT);
    } else {
        /*
         * Softkey set to FALSE indicates handset, speaker button, or
         * headset went onhook.  This causes the transfer to complete
         * if onhook xfer is enabled by config.
         */
        config_get_value(CFGID_XFR_ONHOOK_ENABLED, &onhook_xfer,
                         sizeof(onhook_xfer));
        if (onhook_xfer && ((cns_fcb->state == FSMDEF_S_OUTGOING_ALERTING)||
            (cns_fcb->state == FSMDEF_S_CONNECTED))) {
            fsmxfr_initiate_xfr(event);
            return (SM_RC_END);
        } else if (onhook_xfer && xfr_fcb &&
                ((xfr_fcb->state == FSMDEF_S_OUTGOING_ALERTING)||
                (xfr_fcb->state == FSMDEF_S_CONNECTED))) {
            fsmxfr_initiate_xfr(event);
            return (SM_RC_END);
        } else {
            fsmxfr_feature_cancel(xcb, xcb->xfr_line,
                                    xcb->xfr_call_id, xcb->cns_call_id, CC_SK_EVT_TYPE_IMPLI);

            fsmxfr_cleanup(fcb, __LINE__, TRUE);
            return (SM_RC_CONT);
        }
    }
}


/*
 * This event can only happen if the user initiated a blind transfer.
 */
static sm_rcs_t
fsmxfr_ev_active_dialstring (sm_event_t *event)
{
    static const char fname[] = "fsmxfr_ev_active_dialstring";
    fsm_fcb_t        *fcb     = (fsm_fcb_t *) event->data;
    cc_dialstring_t  *msg     = (cc_dialstring_t *) event->msg;
    callid_t          call_id = msg->call_id;
    line_t             line    = msg->line;
    fsmdef_dcb_t     *xfr_dcb;
    fsmxfr_xcb_t     *xcb     = fcb->xcb;
    cc_feature_data_t data;
    char             *dialstring;

    /*
     * Make sure we have a valid dialstring.
     */
    dialstring = msg->dialstring;
    if ((dialstring == NULL) || (dialstring[0] == '\0')) {
        FSM_DEBUG_SM(DEB_L_C_F_PREFIX"dialstring= %c",
			DEB_L_C_F_PREFIX_ARGS(FSM, msg->line, call_id, fname), '\0');
        return (SM_RC_END);
    }

    FSM_DEBUG_SM(DEB_L_C_F_PREFIX"dialstring= %s",
		DEB_L_C_F_PREFIX_ARGS(FSM, msg->line, call_id, fname), dialstring);

    /*
     * If this is a blind xfer and we have received the dialstring
     * before the original call has received a response to the hold
     * request, defer processing the dialstring event. active_feature
     * will be set to CC_FEATURE_BLIND_XFER if we are still waiting
     * for the hold response. The dial string is saved on the xcb
     * until needed.
     */
    if (xcb == NULL) {
        return (SM_RC_END);
    }

    xfr_dcb = fsm_get_dcb(xcb->xfr_call_id);
    if (xfr_dcb == NULL) {
        return (SM_RC_END);
    }

    if (xfr_dcb->active_feature == CC_FEATURE_BLIND_XFER) {
        if (!fsmxfr_copy_dialstring(&xcb->queued_dialstring, dialstring)) {
            GSM_DEBUG_ERROR(GSM_L_C_F_PREFIX"unable to copy dialstring",
                            msg->line, call_id, fname);
        }
        return (SM_RC_END);
    }

    if (xcb->type == FSMXFR_TYPE_BLND_XFR) {
        lsm_set_hold_ringback_status(xcb->cns_call_id, FALSE);
    }
    /*
     * Make sure that this event came from the consultation call and that
     * this is a blind transfer. We ignore the event if this is a transfer
     * with consultation, because we want to talk to the target. For an
     * unattended transfer this event is the final event needed to complete
     * the transfer.
     */
    if ((xcb->cns_call_id != call_id) || (xcb->type != FSMXFR_TYPE_BLND_XFR)) {
        return (SM_RC_CONT);
    }

    switch (xcb->method) {
    case CC_XFER_METHOD_BYE:
    case CC_XFER_METHOD_REFER:
        /*
         * This is an unattended transfer so:
         * 1. clear the consultation call,
         * 2. instruct the stack to initiate the transfer.
         */

        /*
         * Release this call because it was only used to collect digits.
         */
        data.endcall.cause = CC_CAUSE_NORMAL;
        cc_int_feature(CC_SRC_GSM, CC_SRC_GSM, call_id,
                       line, CC_FEATURE_END_CALL, &data);


        /*
         * Send an event to the transferer call leg so it can send the called
         * number to the transferee.
         */
        fsmxfr_set_xfer_data(CC_CAUSE_XFER_LOCAL, xcb->method, CC_NO_CALL_ID,
                             dialstring, &(data.xfer));

        cc_int_feature(CC_SRC_GSM, CC_SRC_SIP, xcb->xfr_call_id,
                       line, fsmxfr_type_to_feature(xcb->type), &data);

        FSM_DEBUG_SM(get_debug_string(FSMXFR_DBG_XFR_INITIATED),
                     xcb->xfr_id, xcb->xfr_call_id, xcb->cns_call_id, __LINE__);

        break;

    default:
        break;
    }                           /* switch (xcb->method) */

    return (SM_RC_END);
}


cc_int32_t
fsmxfr_show_cmd (cc_int32_t argc, const char *argv[])
{
    fsmxfr_xcb_t   *xcb;
    int             i = 0;

    /*
     * Check if need help.
     */
    if ((argc == 2) && (argv[1][0] == '?')) {
        debugif_printf("show fsmxfr\n");
        return (0);
    }

    debugif_printf("\n------------------------ FSMXFR xcbs -------------------------");
    debugif_printf("\ni   xfr_id  xcb         type  method  xfr_call_id  cns_call_id");
    debugif_printf("\n--------------------------------------------------------------\n");

    FSM_FOR_ALL_CBS(xcb, fsmxfr_xcbs, FSMXFR_MAX_XCBS) {
        debugif_printf("%-2d  %-6d  0x%8p  %-4d  %-6d  %-11d  %-11d\n",
                       i++, xcb->xfr_id, xcb, xcb->type, xcb->method,
                       xcb->xfr_call_id, xcb->cns_call_id);
    }

    return (0);
}


void
fsmxfr_init (void)
{
    fsmxfr_xcb_t *xcb;


    /*
     * Initialize the xcbs.
     */
    fsmxfr_xcbs = (fsmxfr_xcb_t *)
        cpr_calloc(FSMXFR_MAX_XCBS, sizeof(fsmxfr_xcb_t));

    FSM_FOR_ALL_CBS(xcb, fsmxfr_xcbs, FSMXFR_MAX_XCBS) {
        fsmxfr_init_xcb(xcb);
    }

    /*
     * Initialize the state/event table.
     */
    fsmxfr_sm_table.min_state = FSMXFR_S_MIN;
    fsmxfr_sm_table.max_state = FSMXFR_S_MAX;
    fsmxfr_sm_table.min_event = CC_MSG_MIN;
    fsmxfr_sm_table.max_event = CC_MSG_MAX;
    fsmxfr_sm_table.table     = (&(fsmxfr_function_table[0][0]));
}

cc_transfer_mode_e
cc_is_xfr_call (callid_t call_id)
{
    static const char fname[] = "cc_is_xfr_call";
    int mode;

    if (call_id == CC_NO_CALL_ID) {
        return CC_XFR_MODE_NONE;
    }
    mode = fsmutil_is_xfr_leg(call_id, fsmxfr_xcbs, FSMXFR_MAX_XCBS);

    switch (mode) {
    case FSMXFR_MODE_TRANSFEROR:
        FSM_DEBUG_SM(DEB_F_PREFIX"xfer mode is transferor for call id = %d", DEB_F_PREFIX_ARGS(FSM, fname), call_id);
        return CC_XFR_MODE_TRANSFEROR;
    case FSMXFR_MODE_TRANSFEREE:
        FSM_DEBUG_SM(DEB_F_PREFIX"xfer mode is transferee for call id = %d", DEB_F_PREFIX_ARGS(FSM, fname), call_id);
        return CC_XFR_MODE_TRANSFEREE;
    case FSMXFR_MODE_TARGET:
        FSM_DEBUG_SM(DEB_F_PREFIX"xfer mode is target for call id = %d", DEB_F_PREFIX_ARGS(FSM, fname), call_id);
        return CC_XFR_MODE_TARGET;
    default:
        FSM_DEBUG_SM(DEB_F_PREFIX"invalid xfer mode %d for call id = %d", DEB_F_PREFIX_ARGS(FSM, fname), mode, call_id);
        return CC_XFR_MODE_NONE;
    }
}

void
fsmxfr_shutdown (void)
{
    cpr_free(fsmxfr_xcbs);
    fsmxfr_xcbs = NULL;
}

int
fsmutil_is_xfr_consult_call (callid_t call_id)
{
    return fsmutil_is_xfr_consult_leg(call_id, fsmxfr_xcbs, FSMXFR_MAX_XCBS);
}

callid_t
fsmxfr_get_consult_call_id (callid_t call_id)
{
    fsmxfr_xcb_t *xcb;

    xcb = fsmxfr_get_xcb_by_call_id(call_id);

    if (xcb && call_id == xcb->xfr_call_id) {
        return (fsmxfr_get_other_call_id(xcb, call_id));
    } else {
        return (CC_NO_CALL_ID);
    }
}

callid_t
fsmxfr_get_primary_call_id (callid_t call_id)
{
    fsmxfr_xcb_t *xcb;

    xcb = fsmxfr_get_xcb_by_call_id(call_id);

    if (xcb && (xcb->cns_call_id == call_id)) {
        return (fsmxfr_get_other_call_id(xcb, call_id));
    } else {
        return (CC_NO_CALL_ID);
    }
}
