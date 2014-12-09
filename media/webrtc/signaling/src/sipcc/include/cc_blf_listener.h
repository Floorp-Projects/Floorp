/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifndef _CC_BLF_LISTENER_H_
#define _CC_BLF_LISTENER_H_

#include "cc_constants.h"

/**
 * Update BLF notification
 * @param request_id
 * @param blf_state the BLF state, it depends on app_id (0 for call list) or line features (speeddial blf
 * @param app_id
 * @return void
 */
void CC_BLF_notification(int request_id, cc_blf_state_t blf_state, int app_id);

#endif /* _CC_BLF_LISTENER_H_ */

