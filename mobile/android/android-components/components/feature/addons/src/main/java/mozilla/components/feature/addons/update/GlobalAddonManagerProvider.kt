/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

package mozilla.components.feature.addons.update

import androidx.annotation.VisibleForTesting
import mozilla.components.feature.addons.AddonManager

/**
 * Provides global access to the [AddonManager] for background workers.
 */
internal object GlobalAddonManagerProvider {
    @VisibleForTesting
    internal var addonManager: AddonManager? = null

    internal fun initialize(manager: AddonManager) {
        addonManager = manager
    }

    internal fun requireAddonManager(): AddonManager {
        return requireNotNull(addonManager) {
            "initialize must be called before trying to access the AddonManager"
        }
    }
}
