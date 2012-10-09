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
#include "cpr_stdlib.h"
#include "cpr_stdio.h"
#include "fsm.h"
#include "fim.h"
#include "lsm.h"
#include "sm.h"
#include "gsm.h" /* for GSM_ERR_MSG */
#include "ccapi.h"
#include "phone_debug.h"
#include "debug.h"
#include "text_strings.h"
#include "sip_interface_regmgr.h"
#include "resource_manager.h"
#include "platform_api.h"

#define FSM_MAX_FCBS (LSM_MAX_CALLS * (FSM_TYPE_MAX - 1))
#define FSM_S_IDLE   0

extern sm_table_t *pfsmcnf_sm_table;
extern sm_table_t *pfsmb2bcnf_sm_table;
extern sm_table_t *pfsmxfr_sm_table;
extern sm_table_t *pfsmdef_sm_table;

static fsm_fcb_t *fsm_fcbs;
static fsmdef_dcb_t fsm_dcb;
extern uint16_t g_numofselected_calls;
extern void dcsm_update_gsm_state(fsm_fcb_t *fcb, callid_t call_id, int state);


static const char *fsm_type_names[] = {
    "HEAD",
    "CNF",
    "B2BCNF",
    "XFR",
    "DEF"
};

static resource_manager_t *ci_map_p[MAX_REG_LINES + 1];
static resource_manager_t *shown_calls_ci_map_p[MAX_REG_LINES + 1];

const char *
fsm_type_name (fsm_types_t type)
{
    if ((type <= FSM_TYPE_MIN) || (type >= FSM_TYPE_MAX)) {
        return (get_debug_string(GSM_UNDEFINED));
    }

    return (fsm_type_names[type]);
}


const char *
fsm_state_name (fsm_types_t type, int id)
{
    switch (type) {
    case FSM_TYPE_DEF:
        return (fsmdef_state_name(id));

    case FSM_TYPE_XFR:
        return (fsmxfr_state_name(id));

    case FSM_TYPE_CNF:
        return (fsmcnf_state_name(id));

    case FSM_TYPE_B2BCNF:
        return (fsmb2bcnf_state_name(id));

    case FSM_TYPE_NONE:
        return ("IDLE");

    default:
        return (get_debug_string(GSM_UNDEFINED));
    }
}


void
fsm_sm_ftr (cc_features_t ftr_id, cc_srcs_t src_id)
{
    FSM_DEBUG_SM(get_debug_string(FSM_DBG_SM_FTR_ENTRY),
                 cc_feature_name(ftr_id), cc_src_name(src_id));
}


void
fsm_sm_ignore_ftr (fsm_fcb_t *fcb, int fname, cc_features_t ftr_id)
{
    FSM_DEBUG_SM(get_debug_string(FSM_DBG_IGNORE_FTR),
                 fsm_type_name(fcb->fsm_type), fcb->call_id, fname,
                 cc_feature_name(ftr_id));
}


void
fsm_sm_ignore_src (fsm_fcb_t *fcb, int fname, cc_srcs_t src_id)
{
    FSM_DEBUG_SM(get_debug_string(FSM_DBG_IGNORE_SRC),
                 fsm_type_name(fcb->fsm_type), fcb->call_id, fname,
                 cc_src_name(src_id));
}


void
fsm_init_fcb (fsm_fcb_t *fcb, callid_t call_id, fsmdef_dcb_t *dcb,
              fsm_types_t type)
{
    fcb->call_id = call_id;

    fcb->state     = FSM_S_IDLE;
    fcb->old_state = FSM_S_IDLE;

    fcb->fsm_type = type;

    fcb->dcb = dcb;

    fcb->xcb = NULL;

    fcb->ccb = NULL;

    fcb->b2bccb = NULL;
}


/*
 *  ROUTINE:     fsm_get_fcb_by_call_id_and_type
 *
 *  DESCRIPTION: return the fcb referenced by the given call_id and type
 *
 *  PARAMETERS:  fsm_id
 *
 *  RETURNS:     fcb
 *      !NULL: fcb found
 *      NULL:  fcb not found
 *
 *  NOTES:
 */
fsm_fcb_t *
fsm_get_fcb_by_call_id_and_type (callid_t call_id, fsm_types_t type)
{
    static const char fname[] = "fsm_get_fcb_by_call_id_and_type";
    fsm_fcb_t      *fcb;
    fsm_fcb_t      *fcb_found = NULL;

    FSM_FOR_ALL_CBS(fcb, fsm_fcbs, FSM_MAX_FCBS) {
        if ((fcb->call_id == call_id) && (fcb->fsm_type == type)) {
            fcb_found = fcb;
            break;
        }
    }

    FSM_DEBUG_SM(get_debug_string(GSM_DBG_PTR), "FSM", call_id,
                 fname, "fcb", fcb_found);

    return (fcb_found);
}

/*
 *  DESCRIPTION: return the fcb referenced by the given call_id and type
 *
 *  PARAMETERS:  fsm_id
 *
 *  @return void
 *      !NULL: fcb found
 *      NULL:  fcb not found
 *
 */
void 
fsm_get_fcb_by_selected_or_connected_call_fcb (callid_t call_id, fsm_fcb_t **con_fcb_found,
                                               fsm_fcb_t **sel_fcb_found)
{
    static const char fname[] = "fsm_get_fcb_by_selected_or_connected_call_fcb";
    fsm_fcb_t      *fcb;

    *con_fcb_found = NULL;
    *sel_fcb_found = NULL;

    FSM_FOR_ALL_CBS(fcb, fsm_fcbs, FSM_MAX_FCBS) {

        if (fcb->call_id == call_id) {
            /* Do not count current call_id */
            continue;
        }
        if (fcb->fsm_type == FSM_TYPE_DEF && 
            (fcb->state == FSMDEF_S_CONNECTED ||
             fcb->state == FSMDEF_S_CONNECTED_MEDIA_PEND ||
             fcb->state == FSMDEF_S_OUTGOING_ALERTING)) {
            *con_fcb_found = fcb;
        } else if (fcb->fsm_type == FSM_TYPE_DEF && fcb->dcb->selected) {
            *sel_fcb_found = fcb;
            break;
        }
    }

    FSM_DEBUG_SM(get_debug_string(GSM_DBG_PTR), "FSM", call_id,
                 fname, "fcb", con_fcb_found);

}

/*
 *  ROUTINE:     fsm_get_fcb_by_call_id
 *
 *  DESCRIPTION: return the fcb referenced by the given call_id
 *
 *  PARAMETERS:  fsm_id
 *
 *  RETURNS:     fcb
 *      !NULL: fcb found
 *      NULL:  fcb not found
 *
 *  NOTES:
 */
fsm_fcb_t *
fsm_get_fcb_by_call_id (callid_t call_id)
{
    static const char fname[] = "fsm_get_fcb_by_call_id";
    fsm_fcb_t      *fcb;
    fsm_fcb_t      *fcb_found = NULL;

    FSM_FOR_ALL_CBS(fcb, fsm_fcbs, FSM_MAX_FCBS) {
        if (fcb->call_id == call_id) {
            fcb_found = fcb;
            break;
        }
    }

    FSM_DEBUG_SM(get_debug_string(GSM_DBG_PTR), "FSM", call_id,
                 fname, "fcb", fcb_found);

    return (fcb_found);
}


/*
 *  ROUTINE:     fsm_get_new_fcb
 *
 *  DESCRIPTION: return a new fcb initialized with the given data
 *
 *  PARAMETERS:
 *      call_id:    call_id
 *      type:       feature type
 *
 *  RETURNS:
 *      fcb: the new fcb
 */
fsm_fcb_t *
fsm_get_new_fcb (callid_t call_id, fsm_types_t fsm_type)
{
    static const char fname[] = "fsm_get_new_fcb";
    fsm_fcb_t *fcb;

    /*
     * Get free fcb by using CC_NO_CALL_ID as the call_id because a free fcb
     * will have a call_id of CC_NO_CALL_ID.
     */
    fcb = fsm_get_fcb_by_call_id(CC_NO_CALL_ID);
    if (fcb != NULL) {
        fsm_init_fcb(fcb, call_id, FSMDEF_NO_DCB, fsm_type);
    }

    FSM_DEBUG_SM(get_debug_string(GSM_DBG_PTR), "FSM", call_id,
                 fname, "fcb", fcb);

    return (fcb);
}


fsmdef_dcb_t *
fsm_get_dcb (callid_t call_id)
{
    fsmdef_dcb_t *dcb;

    dcb = fsmdef_get_dcb_by_call_id(call_id);

    /*
     * Return the fsm default dcb if a dcb was not found.
     */
    if (dcb == NULL) {
        dcb = &fsm_dcb;
    }

    return (dcb);
}


void
fsm_get_fcb (fim_icb_t *icb, callid_t call_id)
{
    icb->cb = fsm_get_new_fcb(call_id, icb->scb->type);
}


void
fsm_init_scb (fim_icb_t *icb, callid_t call_id)
{
    icb->scb->get_cb = &fsm_get_fcb;

    switch (icb->scb->type) {

    case FSM_TYPE_B2BCNF:
        icb->scb->sm = pfsmb2bcnf_sm_table;
        icb->scb->free_cb = fsmb2bcnf_free_cb;

        break;

    case FSM_TYPE_CNF:
        icb->scb->sm = pfsmcnf_sm_table;
        icb->scb->free_cb = fsmcnf_free_cb;

        break;
    case FSM_TYPE_XFR:
        icb->scb->sm = pfsmxfr_sm_table;
        icb->scb->free_cb = fsmxfr_free_cb;

        break;

    case FSM_TYPE_DEF:
        icb->scb->sm = pfsmdef_sm_table;
        icb->scb->free_cb = fsmdef_free_cb;

        break;

    case FSM_TYPE_HEAD:
    default:
        icb->scb->get_cb  = NULL;
        icb->scb->free_cb = NULL;
        icb->scb->sm      = NULL;
    }

}


void
fsm_change_state (fsm_fcb_t *fcb, int fname, int new_state)
{

    DEF_DEBUG(DEB_L_C_F_PREFIX"%s: %s -> %s\n",
                 DEB_L_C_F_PREFIX_ARGS(FSM, ((fcb->dcb == NULL)? CC_NO_LINE: fcb->dcb->line),
                 fcb->call_id, "fsm_change_state"),
                 fsm_type_name(fcb->fsm_type), 
                 fsm_state_name(fcb->fsm_type, fcb->state),
                 fsm_state_name(fcb->fsm_type, new_state));

    fcb->old_state = fcb->state;
    fcb->state = new_state;
    NOTIFY_STATE_CHANGE(fcb, fcb->call_id, new_state);

}


void
fsm_release (fsm_fcb_t *fcb, int fname, cc_causes_t cause)
{
    fsm_change_state(fcb, fname, FSM_S_IDLE);

    /* Cleanup any pending cac request for that call */
    fsm_cac_call_release_cleanup(fcb->call_id);

    fsm_init_fcb(fcb, CC_NO_CALL_ID, FSMDEF_NO_DCB, FSM_TYPE_NONE);
}


cc_int32_t
fsm_show_cmd (cc_int32_t argc, const char *argv[])
{
    fsm_fcb_t      *fcb;
    int             i = 0;
    void           *cb = NULL;

    /*
     * check if need help
     */
    if ((argc == 2) && (argv[1][0] == '?')) {
        debugif_printf("show fsm\n");
        return 0;
    }

    /*
     * Print the fcbs
     */
    debugif_printf("\n----------------------------- FSM fcbs -------------------------------");
    debugif_printf("\ni    call_id  fcb         type       state      dcb         cb        ");
    debugif_printf("\n----------------------------------------------------------------------\n");

    FSM_FOR_ALL_CBS(fcb, fsm_fcbs, FSM_MAX_FCBS) {
        switch (fcb->fsm_type) {
        case FSM_TYPE_CNF:
            cb = fcb->ccb;
            break;

        case FSM_TYPE_B2BCNF:
            cb = fcb->ccb;
            break;

        case FSM_TYPE_XFR:
            cb = fcb->xcb;
            break;

        case FSM_TYPE_DEF:
            cb = fcb->dcb;
            break;

        default:
            cb = NULL;
        }

        debugif_printf("%-3d  %-7d  0x%8p  %-9s  %-9s  0x%8p  0x%8p\n",
                       i++, fcb->call_id, fcb, fsm_type_name(fcb->fsm_type),
                       fsm_state_name(fcb->fsm_type, fcb->state),
                       fcb->dcb, cb);
    }

    return (0);
}


void
fsm_init (void)
{
    fsm_fcb_t *fcb;

    fsmdef_init_dcb(&fsm_dcb, 0, FSMDEF_CALL_TYPE_NONE, NULL, LSM_NO_LINE,
                    NULL);

    fsmdef_init();
    fsmb2bcnf_init();
    fsmcnf_init();
    fsmxfr_init();

    fsm_cac_init();

    /*
     * Initialize the fcbs.
     */
    fsm_fcbs = (fsm_fcb_t *) cpr_calloc(FSM_MAX_FCBS, sizeof(fsm_fcb_t));
    if (fsm_fcbs == NULL) {
        GSM_ERR_MSG(GSM_F_PREFIX"Failed to allcoate FSM FCBs.\n", "fsm_init");
        return;
    }

    FSM_FOR_ALL_CBS(fcb, fsm_fcbs, FSM_MAX_FCBS) {
        fsm_init_fcb(fcb, CC_NO_CALL_ID, FSMDEF_NO_DCB, FSM_TYPE_NONE);
    }

    /*
     * Init call instance id map.
     */
    fsmutil_init_ci_map();
}

void
fsm_shutdown (void)
{
    fsmdef_shutdown();

    fsmb2bcnf_shutdown();

    fsmcnf_shutdown();
    fsmxfr_shutdown();

    fsm_cac_shutdown();

    cpr_free(fsm_fcbs);
    fsm_fcbs = NULL;

    /*
     * Free call instance id map.
     */
    fsmutil_free_ci_map();
}

/*
 *  ROUTINE:     fsm_set_fcb_dcbs
 *
 *  DESCRIPTION: Set the dcbs for the fsms. The fsm call chain is setup before
 *               the dcb is known, so this function is called after the dcb
 *               is known to set the dcb in the fcbs.
 *
 *  PARAMETERS:
 *      dcb
 *
 *  RETURNS:     NONE
 *
 *  NOTES:
 */
cc_causes_t
fsm_set_fcb_dcbs (fsmdef_dcb_t *dcb)
{
    callid_t        call_id = dcb->call_id;
    fsm_fcb_t      *fcb;
    fsm_types_t     i;

    for (i = FSM_TYPE_CNF; i < FSM_TYPE_MAX; i++) {
        fcb = fsm_get_fcb_by_call_id_and_type(call_id, i);
        if (fcb == NULL) {
            return CC_CAUSE_ERROR;
        }
        fcb->dcb = dcb;
    }

    return CC_CAUSE_OK;
}


/*
 *  ROUTINE:     fsm_get_new_outgoing_call_context
 *
 *  DESCRIPTION: get a new outgoing call context
 *
 *  PARAMETERS:
 *      fcb
 *      call_id
 *      line
 *
 *  RETURNS:     rc
 *      FSM_SUCCESS: context successfully created
 *      FSM_SUCCESS: context unsuccessfully created
 *
 *  NOTES:
 */
cc_causes_t
fsm_get_new_outgoing_call_context (callid_t call_id, line_t line,
                                  fsm_fcb_t *fcb, boolean expline)
{
    static const char fname[] = "fsm_get_new_outgoing_call_context";
    fsmdef_dcb_t   *dcb;
    cc_causes_t     cause = CC_CAUSE_OK;
    cc_causes_t     lsm_rc;

    /*
     * Get a dcb to handle the call.
     */
    dcb = fsmdef_get_new_dcb(call_id);
    if (dcb == NULL) {
        return CC_CAUSE_NO_RESOURCE;
    }

    /*
     * Get a free facility associated with this line.
     */
    lsm_rc = lsm_get_facility_by_line(call_id, line, expline, dcb);
    if (lsm_rc != CC_CAUSE_OK) {
        FSM_DEBUG_SM(get_debug_string(FSM_DBG_FAC_ERR), call_id, fname,
                     "lsm_get_facility_by_line failed", cc_cause_name(lsm_rc));
    }

    /*
     * If no line was returned, then init the dcb with an invalid line to
     * indicate that no line was returned.
     */
    if (lsm_rc != CC_CAUSE_OK) {
        line = LSM_NO_LINE;
    }
    fsmdef_init_dcb(dcb, call_id, FSMDEF_CALL_TYPE_OUTGOING, NULL, line, fcb);

    cause = fsm_set_fcb_dcbs(dcb);
    if (cause == CC_CAUSE_OK) {
        cause = lsm_rc;
    }

    FSM_DEBUG_SM(get_debug_string(FSM_DBG_FAC_FOUND), call_id, fname,
                 dcb->line);

    return cause;
}


/*
 *  ROUTINE:     fsm_get_new_incoming_call_context
 *
 *  DESCRIPTION: get a new incoming call context
 *
 *  PARAMETERS:
 *      fcb
 *      call_id
 *
 *  RETURNS:     rc
 *      FSM_SUCCESS: context successfully created
 *      FSM_SUCCESS: context unsuccessfully created
 *
 *  NOTES:
 */
cc_causes_t
fsm_get_new_incoming_call_context (callid_t call_id, fsm_fcb_t *fcb,
                                   const char *called_number, boolean expline)
{
    static const char fname[] = "fsm_get_new_incoming_call_context";
    fsmdef_dcb_t   *dcb;
    line_t          free_line;
    cc_causes_t     cause;
    cc_causes_t     lsm_rc;


    /*
     * Get a dcb to handle the call.
     */
    dcb = fsmdef_get_new_dcb(call_id);
    if (dcb == NULL) {
        return CC_CAUSE_NO_RESOURCE;
    }

    /*
     * Get a free facility associated with this called_number.
     */
    if ((lsm_rc = lsm_get_facility_by_called_number(call_id, called_number,
                                                    &free_line, expline, dcb))
        != CC_CAUSE_OK) {
        /*
         * Set a default free line (This should really be changed
         * to a special invalid value and GSM modified to recognize it.)
         * The dcb is needed in order for GSM to clean up the call correctly.
         */
        free_line = 1;
        FSM_DEBUG_SM(get_debug_string(FSM_DBG_FAC_ERR), call_id, fname,
                     "lsm_get_facility_by_called_number",
                     cc_cause_name(lsm_rc));
    }

    fsmdef_init_dcb(dcb, call_id, FSMDEF_CALL_TYPE_INCOMING, called_number,
                    free_line, fcb);

    cause = fsm_set_fcb_dcbs(dcb);
    if (cause == CC_CAUSE_OK) {
        cause = lsm_rc;
    }

    FSM_DEBUG_SM(get_debug_string(FSM_DBG_FAC_FOUND), call_id, fname,
                 dcb->line);

    return cause;
}

/*
 * fsmutil_is_cnf_leg
 *
 * Description:
 *
 * Returns TRUE if conferencing is active for the call_id
 *
 *
 * Parameters:
 *
 * call_id - ccapi call identifier associated with the call.
 * fsmxcb_ccbs - pointer to all conf ccbs
 * max_ccbs - Max number of ccbs to check
 *
 * Returns TRUE if the conferencing is active for the call_id
 */
int
fsmutil_is_cnf_leg (callid_t call_id, fsmcnf_ccb_t *fsmcnf_ccbs,
                    unsigned short max_ccbs)
{
    fsmcnf_ccb_t *ccb;

    FSM_FOR_ALL_CBS(ccb, fsmcnf_ccbs, max_ccbs) {
        if ((ccb->cnf_call_id == call_id) || (ccb->cns_call_id == call_id)) {
            return TRUE;
        }
    }
    return FALSE;
}

/*
 * fsmutil_is_xfr_leg
 *
 * Description:
 *
 * Returns transfer mode if the specified leg identified by call_id is
 * participating in a xfer
 *
 * Parameters:
 *
 * call_id -   ccapi call identifier associated with the call.
 * fsmxfr_xcbs - pointer to all xfr ccbs
 * max_xcbs - Max number of ccbs to check
 *
 * Returns: fsmxfr_modes_t
 */
int
fsmutil_is_xfr_leg (callid_t call_id, fsmxfr_xcb_t *fsmxfr_xcbs,
                    unsigned short max_xcbs)
{
    fsmxfr_xcb_t *xcb;

    FSM_FOR_ALL_CBS(xcb, fsmxfr_xcbs, max_xcbs) {
        if ((xcb->xfr_call_id == call_id) || (xcb->cns_call_id == call_id)) {
            return xcb->mode;
        }
    }
    return FSMXFR_MODE_MIN;
}

void
fsm_display_no_free_lines (void)
{
    char tmp_str[STATUS_LINE_MAX_LEN];

    if ((platGetPhraseText(STR_INDEX_NO_FREE_LINES,
                                 (char *)tmp_str,
                                 (STATUS_LINE_MAX_LEN - 1))) == CPR_SUCCESS) {
        lsm_ui_display_notify(tmp_str, NO_FREE_LINES_TIMEOUT);
    }
}

/*
 *  Function: fsm_display_use_line_or_join_to_complete
 *
 *  Description:
 *      The function is used to put up the status line
 *      "Use Line or Join to Complete"
 *
 *  @param None
 *
 *  @return None
 */
void
fsm_display_use_line_or_join_to_complete (void)
{
    char tmp_str[STATUS_LINE_MAX_LEN];

    if ((platGetPhraseText(STR_INDEX_USE_LINE_OR_JOIN_TO_COMPLETE,
                                 (char *)tmp_str,
                                 (STATUS_LINE_MAX_LEN - 1))) == CPR_SUCCESS) {
        lsm_ui_display_notify(tmp_str, NO_FREE_LINES_TIMEOUT);
    }
}

void
fsm_display_feature_unavailable (void)
{
    char tmp_str[STATUS_LINE_MAX_LEN];

    if ((platGetPhraseText(STR_INDEX_FEAT_UNAVAIL,
                                 (char *)tmp_str,
                                 (STATUS_LINE_MAX_LEN - 1))) == CPR_SUCCESS) {
        lsm_ui_display_notify(tmp_str, NO_FREE_LINES_TIMEOUT);
    }
}

void
fsm_set_call_status_feature_unavailable (callid_t call_id, line_t line)
{
    char tmp_str[STATUS_LINE_MAX_LEN];

    if ((platGetPhraseText(STR_INDEX_FEAT_UNAVAIL,
                                 (char *)tmp_str,
                                 (STATUS_LINE_MAX_LEN - 1))) == CPR_SUCCESS) {
        ui_set_call_status(tmp_str, line, lsm_get_ui_id(call_id));
    }
}

/**
 *
 * Get total number of selected calls.
 *
 * @param void
 *
 * @return  uint16_t
 *
 * @pre     (none)
 */
uint16_t fsmutil_get_num_selected_calls (void)
{
    return(g_numofselected_calls);
}

/**
 * This function will hide/unhide ringingin calls.
 *
 * @param[in] hide - indicates whether calls should be hidden or unhidden
 *
 * @return none
 */
void fsm_display_control_ringin_calls (boolean hide)
{
    fsm_fcb_t      *fcb;

    FSM_FOR_ALL_CBS(fcb, fsm_fcbs, FSM_MAX_FCBS) {
        if ((fcb->state == FSMDEF_S_INCOMING_ALERTING) &&
            (lsm_is_it_priority_call(fcb->call_id) == FALSE)) { /* priority call should not be hidden */
            lsm_display_control_ringin_call (fcb->call_id, fcb->dcb->line, hide);
            if (hide == TRUE) {
                fsmutil_clear_shown_calls_ci_element(fcb->dcb->caller_id.call_instance_id, fcb->dcb->line);
            } else {
                fsmutil_set_shown_calls_ci_element(fcb->dcb->caller_id.call_instance_id, fcb->dcb->line);
            }
        }
    }
}

/*
 * fsmutil_init_groupid
 *
 * Description:
 *
 * Assign the group id to the default control block.
 *
 * Parameters:
 *    dcb       - default control block
 *    call_id   - ccapi call identifier associated with the call
 *    call_type - incoming or outgoing call
 *
 * Returns: None. groupid will be assigned in the dcb.
 */
void
fsmutil_init_groupid (fsmdef_dcb_t *dcb, callid_t call_id,
                      fsmdef_call_types_t call_type)
{
    fsmcnf_ccb_t *ccb = NULL;

    /* if this was a consult leg of a conference then there will be
     * a ccb on the primary leg
     */
    dcb->group_id = CC_NO_GROUP_ID;
    if (call_type != FSMDEF_CALL_TYPE_NONE) {
        ccb = fsmcnf_get_ccb_by_call_id(call_id);
        if (ccb) {
            /* consult leg of a conference */
            fsmdef_dcb_t *other_dcb = NULL;

            other_dcb =
                fsmdef_get_dcb_by_call_id(fsmcnf_get_other_call_id(ccb, call_id));
            if (other_dcb) {
                dcb->group_id = other_dcb->group_id;
            }
        } else {
            /* either a primary or some other leg; not part of any conference */

            dcb->group_id = dcb->call_id;
        }
    }
    return;
}

/**
 *
 *    Find out if the call pointed by call_id is a consult call
 *    of a conference, transfer.
 *
 * @param line       line related to that call
 * @param call_id    call_id
 *
 * @return  call attributes
 *
 * @pre     none
 */
int
fsmutil_get_call_attr (fsmdef_dcb_t *dcb, 
                    line_t line, callid_t call_id)
{
    int call_attr;

    if (fsmutil_is_cnf_consult_call(call_id) == TRUE) {
        call_attr = LOCAL_CONF_CONSULT;
    } else if (fsmutil_is_b2bcnf_consult_call(call_id) == TRUE) {
        call_attr = CONF_CONSULT;
    } else if (fsmutil_is_xfr_consult_call(call_id) == TRUE) {
        call_attr = XFR_CONSULT;
    } else {
        if (dcb == NULL) {
            return(NORMAL_CALL);
        }

        switch (dcb->active_feature) {
        case CC_FEATURE_CFWD_ALL:
            call_attr = CC_ATTR_CFWD_ALL;
            break;
        default:
            call_attr = NORMAL_CALL;
            break;
        }
    }
    return call_attr;
}

/*
 * fsmutil_is_cnf_consult_leg
 *
 * Description:
 *    Returns TRUE if the specified call_id is for a call that is
 *    the consultative call of a conference.
 *
 * Parameters:
 *    call_id     - ccapi call identifier associated with the call.
 *    fsmxcb_ccbs - pointer to all conf ccbs
 *    max_ccbs    - Max number of ccbs to check
 *
 * Returns: TRUE if consultative call; otherwise, FALSE.
 */
int
fsmutil_is_cnf_consult_leg (callid_t call_id, fsmcnf_ccb_t *fsmcnf_ccbs,
                            uint16_t max_ccbs)
{
    fsmcnf_ccb_t *ccb;

    FSM_FOR_ALL_CBS(ccb, fsmcnf_ccbs, max_ccbs) {
        if (ccb->cns_call_id == call_id) {
            return TRUE;
        }
    }

    return FALSE;
}


/*
 * fsmutil_is_xfr_consult_leg
 *
 * Description:
 *    Returns TRUE if the specified call_id is for a call that is
 *    the consultative call of a transfer only when that consult
 *    was an initiated transfer.
 *
 * Parameters:
 *    call_id     - ccapi call identifier associated with the call.
 *    fsmxfr_xcbs - pointer to all xfr ccbs
 *    max_xcbs    - Max number of ccbs to check
 *
 * Returns: TRUE if consultative call; otherwise, FALSE.
 */
int
fsmutil_is_xfr_consult_leg (callid_t call_id, fsmxfr_xcb_t *fsmxfr_xcbs,
                            uint16_t max_xcbs)
{
    fsmxfr_xcb_t *xcb;

    FSM_FOR_ALL_CBS(xcb, fsmxfr_xcbs, max_xcbs) {
        if ((xcb->mode == FSMXFR_MODE_TRANSFEROR) &&
            (xcb->cns_call_id == call_id)) {
            return TRUE;
        }
    }
    return FALSE;
}

/*
 * fsmutil_clear_feature_invocation_state
 *
 * Description:
 *    This function clears the feature invocation state of the feature id
 *    supplied. This function is used by the code that would receive the
 *    feature ack (from SIP stack).
 *
 * Note: Code responsible for invoking features and receiving feature acks
 *       must call this function to clear the feature invocation state.
 *
 * Parameters:
 *    fsmdef_dcb_t  *dcb - dcb associated with the call
 *    cc_features_t feature_id - feature id in question
 *
 * Returns: None
 */
static void
fsmutil_clear_feature_invocation_state (fsmdef_dcb_t *dcb,
                                       cc_features_t feature_id)
{
    if ((feature_id < CC_FEATURE_NONE) || (feature_id >= CC_FEATURE_MAX)) {
        /* Log Error */
        GSM_ERR_MSG(GSM_L_C_F_PREFIX"Invalid feature id -> %d\n", dcb->line, dcb->call_id, "fsmutil_clear_feature_invocation_state", feature_id);
        return;
    }

    rm_clear_element((resource_manager_t *) dcb->feature_invocation_state,
                     (int16_t) feature_id);
}

/*
 * fsmutil_process_feature_ack
 *
 * Description:
 *    This function implements the generic feature ack processing.
 *    Feature invocation state is Cleared when feature ack is received.
 *
 * Parameters:
 *    fsmdef_dcb_t  *dcb - dcb associated with the call
 *    cc_features_t feature_id - feature id in question
 *
 * Returns: None
 */
void
fsmutil_process_feature_ack (fsmdef_dcb_t *dcb, cc_features_t feature_id)
{
    /* clear the feature invocation state (was set when invoked) */
    fsmutil_clear_feature_invocation_state(dcb, feature_id);
}

/*
 * fsmutil_clear_all_feature_invocation_state
 *
 * Description:
 *    This function clears the feature invocation state of ALL features.
 *
 *    This function may be used by the code that would initialize/reset
 *    the state machine.
 *
 * Parameters:
 *    fsmdef_dcb_t  *dcb - dcb associated with the call
 *
 * Returns: None
 */
void
fsmutil_clear_all_feature_invocation_state (fsmdef_dcb_t *dcb)
{
    rm_clear_all_elements((resource_manager_t *) dcb->feature_invocation_state);
}

/*
 * fsmutil_init_feature_invocation_state
 *
 * Description:
 *    Utility function to allocate and init a feature invocation state table.
 *
 * Parameters:
 *    dcb - dcb whose feature invocation state table is being created
 *
 * Returns:
 *    none
 */
void
fsmutil_init_feature_invocation_state (fsmdef_dcb_t *dcb)
{
    static const char fname[] = "fsmutil_init_feature_invocation_state";

    dcb->feature_invocation_state = rm_create(CC_FEATURE_MAX);

    if (!dcb->feature_invocation_state) {
        GSM_ERR_MSG(GSM_L_C_F_PREFIX"failed to allocate feature invocation state table\n", dcb->line, dcb->call_id,
                     fname);
    }
}


/*
 * fsmutil_free_feature_invocation_state
 *
 * Description:
 *    Utility function to free the feature invocation state table of the
 *    specified dcb.
 *
 * Parameters:
 *    None
 *
 * Returns:
 *    None
 */
void
fsmutil_free_feature_invocation_state (fsmdef_dcb_t *dcb)
{
    rm_destroy((resource_manager_t *) dcb->feature_invocation_state);
    dcb->feature_invocation_state = NULL;
}

/*
 * fsmutil_free_all_ci_id
 *
 * Description:
 *    This function clears the entire call instance map.
 *
 * Parameters:
 *    None
 *
 * Returns:
 *    None
 */
void
fsmutil_free_all_ci_id (void)
{
    uint16_t line;

    for (line = 1; line <= MAX_REG_LINES; line++) {
        rm_clear_all_elements((resource_manager_t *) ci_map_p[line]);
    }
}

/*
 * fsmutil_free_ci_id
 *
 * Description:
 *    This function clears a specified call instance id from
 *    the call instance map.
 *
 * Note that call instance ids are one based. The resource manager
 * used to implement the call instance map is zero based so we have
 * to adjust the call instance id when using the resource manager API.
 *
 * Parameters:
 *    id - the call instance id to be freed
 *    line - line from where to free the call instance id
 *
 * Returns:
 *    None
 */
void
fsmutil_free_ci_id (uint16_t id, line_t line)
{
    static const char fname[] = "fsmutil_free_ci_id";

    if (id < 1 || id > MAX_CALLS) {
        GSM_ERR_MSG(GSM_F_PREFIX"specified id %d is invalid\n", fname, id);
        return;
    }

    if (line < 1 || line > MAX_REG_LINES) {
        GSM_ERR_MSG(GSM_F_PREFIX"specified line %d is invalid\n", fname, line);
        return;
    }

    rm_clear_element((resource_manager_t *) ci_map_p[line], (int16_t) --id);
}

#ifdef LOCAL_UI_CALLINSTANCE_ID
/*This routine is not needed now as the local assignment 
 *of call instance id is no longer supported.
 */
/*
 * fsmutil_get_ci_id
 *
 * Description:
 *    This function locates the next available call instance id in
 *    the call instance map. This function is utilized when operating in
 *    the peer to peer mode to assign call instance ids for inbound
 *    and outbound calls.
 *
 * Note that call instance ids are one based. The resource manager
 * used to implement the call instance map is zero based so we have
 * to adjust the call instance id when using the resource manager API.
 *
 * Parameters:
 *    line - line from where to retrieve the call instance id
 *
 * Returns:
 *    uint16_t - Non-zero call instance id. 0 if no call instance ids
 *               are available.
 */
uint16_t
fsmutil_get_ci_id (line_t line)
{
    static const char fname[] = "fsmutil_get_ci_id";
    int16_t         id;
    uint16_t        return_id = 0;

    if (line < 1 || line > MAX_REG_LINES) {
        GSM_ERR_MSG("specified line %d is invalid\n", fname, line);
        return 0;
    }

    id = rm_get_free_element(ci_map_p[line]);
    if (id >= 0) {
        return_id = ++id;
    }
    return (return_id);
}
#endif

/*
 * fsmutil_set_ci_id
 *
 * Description:
 *    This function marks a specified call instance id as being in
 *    use in the call instance map. This function is used when in
 *    CCM mode to store the call instance id specified by the CCM
 *    for inbound and outbound calls.
 *
 * Note that call instance ids are one based. The resource manager
 * used to implement the call instance map is zero based so we have
 * to adjust the call instance id when using the resource manager API.
 * Parameters:
 *
 * Parameters:
 *    id - The call instance id to set.
 *    line - Line being used for the call.
 *
 * Returns:
 *    None
 */
void
fsmutil_set_ci_id (uint16_t id, line_t line)
{
    static const char fname[] = "fsmutil_set_ci_id";

    if (id < 1 || id > MAX_CALLS) {
        GSM_ERR_MSG(GSM_F_PREFIX"specified id %d is invalid\n", fname, id);
        return;
    }

    if (line < 1 || line > MAX_REG_LINES) {
        GSM_ERR_MSG(GSM_F_PREFIX"specified line %d is invalid\n", fname, line);
        return;
    }

    rm_set_element(ci_map_p[line], (int16_t) --id);
}

/*
 * fsmutil_init_ci_map
 *
 * Description:
 *    Utility function to allocate and init the call instance id map.
 *
 * Parameters:
 *    None
 *
 * Returns:
 *    None
 */
void
fsmutil_init_ci_map (void)
{
    static const char fname[] = "fsmutil_init_ci_map";
    uint16_t line;

    for (line = 1; line <= MAX_REG_LINES; line++) {
        ci_map_p[line] = rm_create(MAX_CALLS);

        if (!ci_map_p[line]) {
            GSM_ERR_MSG(GSM_F_PREFIX"failed to allocate call instance id map for line %d",
                         fname, line);
        }
    }
}

/**
 * This function will allocate and initialize the shown_calls_call_instnace map.
 *
 * @return none
 */
void fsmutil_init_shown_calls_ci_map (void)
{
    static const char fname[] = "fsmutil_init_shown_calls_ci_map";
    uint16_t line;

    for (line = 1; line <= MAX_REG_LINES; line++) {
        shown_calls_ci_map_p[line] = rm_create(MAX_CALLS);

        if (!shown_calls_ci_map_p[line]) {
            GSM_ERR_MSG(GSM_F_PREFIX"failed to allocate shown calls call instance id map for line %d",
                         fname, line);
        }
    }
}

/**
 * This function will set the shown_calls_call_instnace map to zeros.
 *
 * @return none
 */
void fsmutil_free_all_shown_calls_ci_map (void)
{
    uint16_t line;

    for (line = 1; line <= MAX_REG_LINES; line++) {
        rm_clear_all_elements((resource_manager_t *) shown_calls_ci_map_p[line]);
    }
}

/**
 * This function marks a given call to be hidden.
 *
 * @param[in] id - call instance id of the call to be hidden.
 * @param[in] line - the line being used for the call.
 *
 * @return none
 */
void fsmutil_clear_shown_calls_ci_element (uint16_t id, line_t line)
{
    static const char fname[] = "fsmutil_clear_shown_calls_ci_element";

    if (id < 1 || id > MAX_CALLS) {
        GSM_ERR_MSG(GSM_F_PREFIX"specified id %d is invalid\n", fname, id);
        return;
    }

    if (line < 1 || line > MAX_REG_LINES) {
        GSM_ERR_MSG(GSM_F_PREFIX"specified line %d is invalid\n", fname, line);
        return;
    }

    rm_clear_element((resource_manager_t *) shown_calls_ci_map_p[line], (int16_t)(id - 1));
}

/**
 * This function marks a given call to be shown.
 *
 * @param[in] id - call instance id of the call to be shown.
 * @param[in] line - the line being used for the call.
 *
 * @return none
 */
void fsmutil_set_shown_calls_ci_element (uint16_t id, line_t line)
{
    static const char fname[] = "fsmutil_set_shown_calls_ci_element";

    if (id < 1 || id > MAX_CALLS) {
        GSM_ERR_MSG(GSM_F_PREFIX"specified id %d is invalid\n", fname, id);
        return;
    }

    if (line < 1 || line > MAX_REG_LINES) {
        GSM_ERR_MSG(GSM_F_PREFIX"specified line %d is invalid\n", fname, line);
        return;
    }

    rm_set_element(shown_calls_ci_map_p[line], (int16_t)(id - 1));
}

/**
 * This function checks if a given call is to be shown.
 *
 * @param[in] id - call instance id of the call.
 * @param[in] line - the line being used for the call.
 *
 * @return none
 */
boolean fsmutil_is_shown_calls_ci_element_set (uint16_t id, line_t line)
{
    static const char fname[] = "fsmutil_is_shown_calls_ci_element_set";

    if (id < 1 || id > MAX_CALLS) {
        GSM_ERR_MSG(GSM_F_PREFIX"specified id %d is invalid\n", fname, id);
        return FALSE;
    }

    if (line < 1 || line > MAX_REG_LINES) {
        GSM_ERR_MSG(GSM_F_PREFIX"specified line %d is invalid\n", fname, line);
        return FALSE;
    }

    if (rm_is_element_set(shown_calls_ci_map_p[line], (int16_t)(id - 1))) {
        return (TRUE);
    }

    return (FALSE);
}

/*
 * fsmutil_free_ci_map
 *
 * Description:
 *    Utility function to free the call instance id map.
 *
 * Parameters:
 *    None
 *
 * Returns:
 *    None
 */
void
fsmutil_free_ci_map (void)
{
    uint16_t line;

    for (line = 1; line <= MAX_REG_LINES; line++) {
        rm_destroy((resource_manager_t *) ci_map_p[line]);
        ci_map_p[line] = NULL;
    }
}

/*
 * fsmutil_show_ci_map
 *
 * Description:
 *    Utility function to display the current state of the call
 *    instance map.
 *
 * Parameters:
 *    None
 *
 * Returns:
 *    None
 */
void
fsmutil_show_ci_map (void)
{
    uint16_t line;

    for (line = 1; line <= MAX_REG_LINES; line++) {
        rm_show(ci_map_p[line]);
    }
}
