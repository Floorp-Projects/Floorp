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

#ifndef _LSM_PRIV_H_
#define _LSM_PRIV_H_

#include "phone_types.h"
#include "ccapi.h"

#define LSM_ERR_MSG err_msg

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
