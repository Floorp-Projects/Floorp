/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

package org.mozilla.fenix.ext

import mozilla.components.feature.addons.Addon
import mozilla.components.feature.addons.AddonManager
import org.mozilla.fenix.BuildConfig
import org.mozilla.fenix.Config

/**
 * Returns the list of all installed and recommended add-ons filters out all the
 * add-ons on [BuildConfig.MOZILLA_ONLINE_ADDON_EXCLUSIONS].
 */
suspend fun AddonManager.getFenixAddons(allowCache: Boolean = true): List<Addon> {
    val addons = getAddons(allowCache = allowCache)
    val excludedAddonIDs = if (Config.channel.isMozillaOnline &&
        !BuildConfig.MOZILLA_ONLINE_ADDON_EXCLUSIONS.isNullOrEmpty()
    ) {
        BuildConfig.MOZILLA_ONLINE_ADDON_EXCLUSIONS.toList()
    } else {
        emptyList<String>()
    }
    return addons.filterNot { it.id in excludedAddonIDs }
}
