/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifndef _SM_H_
#define _SM_H_

#include "phone_debug.h"

typedef enum {
    SM_RC_MIN = -2,
    SM_RC_ERROR = -1,
    SM_RC_SUCCESS,
    SM_RC_CONT,
    SM_RC_DEF_CONT,
    SM_RC_END,
    SM_RC_CLEANUP,
    SM_RC_FSM_ERROR,
    SM_RC_CSM_ERROR,
    SM_RC_MAX
} sm_rcs_t;

typedef struct _sm_event_t {
    int  state;
    int  event;
    void *data;
    void *msg;
} sm_event_t;

typedef sm_rcs_t (*sm_function_t)(sm_event_t *event);

typedef struct _sm_table_t {
    int min_state;
    int max_state;
    int min_event;
    int max_event;
    sm_function_t *table;
} sm_table_t;

sm_rcs_t sm_process_event(sm_table_t *tbl, sm_event_t *event );
sm_rcs_t sm_process_event2(int state_id, int event_id, sm_table_t *tbl,
                           void *msg, void *cb);

#endif
