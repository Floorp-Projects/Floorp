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

interface BrowsingModeListener {
    fun onBrowsingModeChange(newMode: BrowsingMode)
}

/**
 * Wraps a [BrowsingMode] and executes a callback whenever [mode] is updated.
 */
class DefaultBrowsingModeManager(
    private var _mode: BrowsingMode = BrowsingMode.Normal
) : BrowsingModeManager {

    private var _browsingModeListener: BrowsingModeListener? = null

    fun registerBrowsingModeListener(browsingModeListener: BrowsingModeListener) {
        _browsingModeListener = browsingModeListener
    }

    fun unregisterBrowsingModeListener() {
        _browsingModeListener = null
    }

    override var mode: BrowsingMode
        get() = _mode
        set(value) {
            _mode = value
            _browsingModeListener?.onBrowsingModeChange(value)
            Settings.instance?.lastKnownMode = value
        }
}
