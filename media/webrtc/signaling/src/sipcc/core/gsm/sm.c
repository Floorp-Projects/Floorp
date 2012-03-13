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

#include "cpr_stdio.h"
#include "sm.h"
#include "gsm.h"
#include "fsm.h"
#include "text_strings.h"
#include "ccapi.h"

sm_rcs_t
sm_process_event (sm_table_t *tbl, sm_event_t *event)
{
    static const char fname[] = "sm_process_event";
    int        state_id = event->state;
    int        event_id = event->event;
    sm_rcs_t   rc       = SM_RC_ERROR;
    fsm_fcb_t *fcb      = (fsm_fcb_t *) event->data;
    cc_feature_t  *feat_msg = NULL;
    line_t        line_id;
    fsm_types_t   fsm_type;
    callid_t      call_id;
    sm_function_t hdlr; /* cached handler in order to compute its addr once */

    /*
     * validate the state and event
     * and that there is a valid function for this state-event pair.
     */
    if ((state_id > tbl->min_state) &&
        (state_id < tbl->max_state) &&
        (event_id > tbl->min_event) &&
        (event_id < tbl->max_event)) {
        rc = SM_RC_DEF_CONT;
        /*
         * Save some paramters for debuging, the event handler may 
         * free the fcb once returned.
         */
        fsm_type = fcb->fsm_type;
        call_id  = fcb->call_id;
        if ((hdlr = tbl->table[tbl->max_event * state_id + event_id]) != NULL) {
            FSM_DEBUG_SM(DEB_F_PREFIX"%s %-4d: 0x%08lx: sm entry: (%s:%s)\n",
                     DEB_F_PREFIX_ARGS(FSM, fname), fsm_type_name(fsm_type), call_id,
                     tbl->table[tbl->max_event * state_id + event_id],
                     fsm_state_name(fsm_type, state_id),
                     cc_msg_name((cc_msgs_t)(event_id)));

            rc = hdlr(event);
        }

        if (rc != SM_RC_DEF_CONT) {
            /* For event_id == CC_MSG_FEATURE then display the
             * feature associated with it.
             */
            if (event_id == CC_MSG_FEATURE) {
                feat_msg = (cc_feature_t *) event->msg;
            }
            line_id = ((cc_feature_t *) event->msg)->line;

            DEF_DEBUG(DEB_L_C_F_PREFIX"%-5s :(%s:%s%s)\n",
                        DEB_L_C_F_PREFIX_ARGS(GSM, line_id, call_id, fname),
                        fsm_type_name(fsm_type),
                        fsm_state_name(fsm_type, state_id),
                        cc_msg_name((cc_msgs_t)(event_id)),
                        feat_msg ? cc_feature_name(feat_msg->feature_id):" ");
        }
    }
    /*
     * Invalid state-event pair.
     */
    else {
        GSM_ERR_MSG(GSM_F_PREFIX"illegal state-event pair: (%d <-- %d)\n",
                    fname, state_id, event_id);
        rc = SM_RC_ERROR;
    }

    return rc;
}
