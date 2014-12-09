/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifndef _LSM_PRIV_H_
#define _LSM_PRIV_H_

#include "phone_types.h"
#include "ccapi.h"

#define LSM_ERR_MSG(format, ...) CSFLogError("lsm" , format , ## __VA_ARGS__ )

#define LSM_DEFAULT_LINE     (1)
#define LSM_DEFAULT_INSTANCE (1)
#define LSM_DEFAULT_PLANE    (1)
#define LSM_DEFAULT_SPEAKER  (0)

#define LSM_MAX_LCBS         (LSM_MAX_CALLS)

#define LSM_FLAGS_SECURE_MEDIA         (1 << 0) /* encrypted tx media              */
#define LSM_FLAGS_SECURE_MEDIA_UPDATE  (1 << 1) /* update secure media icon needed */
#define LSM_FLAGS_ANSWER_PENDING       (1 << 2) /* answer is pending */
#define LSM_FLAGS_DIALED_STRING        (1 << 3) /* dialed a string */
#define LSM_FLAGS_CALL_PRIORITY_URGENT  (1 << 4)
#define LSM_FLAGS_PREVENT_RINGING       (1 << 5)
#define LSM_FLAGS_DUSTING               (1 << 6) /* the call is a dusting call */


typedef struct lsm_lcb_t_ {
    callid_t     call_id;
    line_t       line;
    call_events  previous_call_event;
    lsm_states_t state;
    int          mru;
    boolean      enable_ringback;
    callid_t     ui_id;
    uint32_t     flags;
    fsmdef_dcb_t *dcb; /* the corresponding DCB chain for this lcm */
    char         *gcid; /*global call identifier */
    int           vid_mute;
    int           vid_flags;
    int           vid_x;
    int           vid_y;
    int           vid_h;
    int           vid_w;
} lsm_lcb_t;

typedef struct lsm_info_t_ {
    int      call_in_progress;
    callid_t active_call_id;
    int      active_call_plane;
    line_t   primary_line;
    int      speaker;
} lsm_info_t;

lsm_lcb_t *lsm_get_lcb_by_call_id(callid_t call_id);

#endif
