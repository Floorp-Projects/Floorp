/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

package mozilla.components.feature.addons.update

import androidx.annotation.VisibleForTesting
import mozilla.components.feature.addons.AddonManager

/**
 * Provides global access to the dependencies needed for updating add-ons.
 */
/** @suppress */
object GlobalAddonDependencyProvider {
    @VisibleForTesting
    internal var addonManager: AddonManager? = null

    @VisibleForTesting
    internal var updater: AddonUpdater? = null

    internal var onCrash: ((Throwable) -> Unit)? = null

    /**
     * Initializes the AddonManager, AddonUpdater and an optional onCrash lambda function.
     */
    fun initialize(manager: AddonManager, updater: AddonUpdater, onCrash: ((Throwable) -> Unit)? = null) {
        this.addonManager = manager
        this.updater = updater
        this.onCrash = onCrash
    }

    internal fun requireAddonManager(): AddonManager {
        return requireNotNull(addonManager) {
            "initialize must be called before trying to access the AddonManager"
        }
    }

    internal fun requireAddonUpdater(): AddonUpdater {
        return requireNotNull(updater) {
            "initialize must be called before trying to access the AddonUpdater"
        }
    }
}
