/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

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
