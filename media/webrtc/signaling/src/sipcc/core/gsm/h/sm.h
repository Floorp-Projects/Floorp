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
