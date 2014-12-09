/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "cc_blf.h"
#include "pres_sub_not_handler.h"

/**
 * Initialize the BLF stack
 * @return
 */
int CC_BLF_init() {
	pres_sub_handler_initialized();
	return CC_SUCCESS;
}
/**
 * Start BLF subscription
 * @param request_id the request id
 * @param duration the subscription duration
 * @param watcher the name of subscription watcher
 * @param presentity
 * @param app_id the application id for the BLF
 * @param feature_mask
 * @return void
 */
void CC_BLF_subscribe(int request_id,
		int duration,
		const char *watcher,
        const char *presentity,
        int app_id,
        cc_blf_feature_mask_t feature_mask) {
	pres_get_state(request_id, duration, watcher, presentity, app_id, feature_mask);
}
/**
 * Unsubscribe the BLF subscription
 * @param request_id the request id
 * @return void
 */
void CC_BLF_unsubscribe(int request_id) {
	pres_terminate_req(request_id);
}

/**
 * Unsubscribe all BLF subscription
 * @return void
 */
void CC_BLF_unsubscribe_All() {
	pres_terminate_req_all();
}

