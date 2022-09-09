/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

package org.mozilla.fenix.browser.browsingmode

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

/**
 * Wraps a [BrowsingMode] and executes a callback whenever [mode] is updated.
 */
class DefaultBrowsingModeManager(
    private var _mode: BrowsingMode,
    private val settings: Settings,
    private val modeDidChange: (BrowsingMode) -> Unit,
) : BrowsingModeManager {

    override var mode: BrowsingMode
        get() = _mode
        set(value) {
            _mode = value
            modeDidChange(value)
            settings.lastKnownMode = value
        }
}
