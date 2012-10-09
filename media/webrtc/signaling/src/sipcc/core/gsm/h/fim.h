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

#ifndef _FIM_H_
#define _FIM_H_

#include "sm.h"
#include "fsm.h"

/*
 * This is an overlay structure.
 * Every entity cb must have these two fields at the start of the cb
 */
typedef struct fim_cb_hdr_ {
    callid_t call_id;
    int state;
} fim_cb_hdr_t;

#ifndef fim_icb_t__
#define fim_icb_t__
struct fim_icb_t_;
typedef struct fim_icb_t_ fim_icb_t;
#endif

typedef void (*fim_func_t)(fim_icb_t *elem, callid_t call_id);

typedef struct fim_scb_t_ {
    fsm_types_t type;
    sm_table_t  *sm;
    fim_func_t  get_cb;
    fim_func_t  free_cb;
} fim_scb_t;

struct fim_icb_t_ {
    struct fim_icb_t_ *next_chn;
    struct fim_icb_t_ *next_icb;
    callid_t          call_id;
    boolean           ui_locked;
    void              *cb;
    fim_scb_t         *scb;
};


const char *fim_event_name(int event);
boolean fim_process_event(void *data, boolean cac_passed);
void fim_free_event(void *data);
void fim_init(void);
void fim_shutdown(void);
void fsmcnf_free_cb(fim_icb_t *icb, callid_t call_id);
void fsmxfr_free_cb(fim_icb_t *icb, callid_t call_id);
void fsmdef_free_cb(fim_icb_t *icb, callid_t call_id);
void fsmb2bcnf_free_cb(fim_icb_t *icb, callid_t call_id);

void fim_lock_ui(callid_t call_id);
void fim_unlock_ui(callid_t call_id);

cc_causes_t
fsm_cac_process_bw_avail_resp(void);
cc_causes_t
fsm_cac_process_bw_failed_resp(void);
cc_causes_t
fsm_cac_call_bandwidth_req(callid_t call_id, uint32_t sessions,
                            void *msg);

#endif /* _FIM_H_ */
