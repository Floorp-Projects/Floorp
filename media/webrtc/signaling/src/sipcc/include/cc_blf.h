/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifndef _CC_BLF_H_
#define _CC_BLF_H_
#include "cc_constants.h"

/**
 * Initialize the BLF stack
 * @return
 */
int CC_BLF_init();
/**
 * Start BLF subscription
 * @param request_id the request id
 * @param duration the subscription duration
 * @param watcher the DN of subscription watcher
 * @param presentity the DN that is watched/monitored.
 * @param app_id the application id for the BLF
 * @param feature_mask please refer to cc_blf_feature_mask_t
 * @return void
 */
void CC_BLF_subscribe(int request_id,
		int duration,
		const char *watcher,
        const char *presentity,
        int app_id,
        cc_blf_feature_mask_t feature_mask);
/**
 * Unsubscribe the BLF subscription
 * @param request_id the request id
 * @return void
 */
void CC_BLF_unsubscribe(int request_id);

/**
 * Unsubscribe all BLF subscription
 * @return void
 */
void CC_BLF_unsubscribe_All();

#endif /* _CC_BLF_H_ */

