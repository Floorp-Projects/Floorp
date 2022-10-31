/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

package mozilla.components.service.fxa.manager.ext

import mozilla.components.concept.sync.DeviceConstellation
import mozilla.components.concept.sync.OAuthAccount
import mozilla.components.service.fxa.manager.FxaAccountManager

/**
 * Executes [block] and provides the [DeviceConstellation] of an [OAuthAccount] if present.
 */
inline fun FxaAccountManager.withConstellation(block: DeviceConstellation.() -> Unit) {
    authenticatedAccount()?.let {
        block(it.deviceConstellation())
    }
}
