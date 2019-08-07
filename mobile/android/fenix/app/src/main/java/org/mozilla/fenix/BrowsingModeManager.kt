/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

package org.mozilla.fenix

import org.mozilla.fenix.utils.Settings

/**
 * Enum that represents whether or not private browsing is active.
 */
enum class BrowsingMode {
    Normal, Private;

    /**
     * Returns true if the [BrowsingMode] is [Private]
     */
    val isPrivate get() = this == Private

    companion object {

        /**
         * Convert a boolean into a [BrowsingMode].
         * True corresponds to [Private] and false corresponds to [Normal].
         */
        fun fromBoolean(isPrivate: Boolean) = if (isPrivate) Private else Normal
    }
}

interface BrowsingModeManager {
    var mode: BrowsingMode
}

class DefaultBrowsingModeManager(
    private val settings: Settings,
    private val modeDidChange: (BrowsingMode) -> Unit
) : BrowsingModeManager {
    override var mode: BrowsingMode
        get() = BrowsingMode.fromBoolean(settings.usePrivateMode)
        set(value) {
            settings.setPrivateMode(value.isPrivate)
            modeDidChange(value)
        }
}

class CustomTabBrowsingModeManager : BrowsingModeManager {
    override var mode
        get() = BrowsingMode.Normal
        set(_) { /* no-op */ }
}
