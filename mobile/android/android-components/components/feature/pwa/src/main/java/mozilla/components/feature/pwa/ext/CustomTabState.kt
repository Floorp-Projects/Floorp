/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

package mozilla.components.feature.pwa.ext

import androidx.browser.customtabs.CustomTabsService.RELATION_HANDLE_ALL_URLS
import mozilla.components.feature.customtabs.store.CustomTabState
import mozilla.components.feature.customtabs.store.VerificationStatus.PENDING
import mozilla.components.feature.customtabs.store.VerificationStatus.SUCCESS

/**
 * Returns a list of trusted (or pending) origins.
 */
val CustomTabState.trustedOrigins
    get() = relationships.mapNotNull { (pair, status) ->
        if (pair.relation == RELATION_HANDLE_ALL_URLS && (status == PENDING || status == SUCCESS)) {
            pair.origin
        } else {
            null
        }
    }
