/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

package org.mozilla.fenix

import org.mozilla.fenix.utils.Settings

interface BrowsingModeManager {
    enum class Mode {
        Normal, Private;

        fun isPrivate(): Boolean = this == Private
    }

    val isPrivate: Boolean
    var mode: Mode
}

interface BrowserModeStorage {
    fun setMode(mode: BrowsingModeManager.Mode)
    fun currentMode(): BrowsingModeManager.Mode
}

fun Settings.createBrowserModeStorage(): BrowserModeStorage = object : BrowserModeStorage {
    override fun currentMode(): BrowsingModeManager.Mode {
        return if (this@createBrowserModeStorage.usePrivateMode) {
            BrowsingModeManager.Mode.Private
        } else {
            BrowsingModeManager.Mode.Normal
        }
    }

    override fun setMode(mode: BrowsingModeManager.Mode) {
        this@createBrowserModeStorage.setPrivateMode(mode == BrowsingModeManager.Mode.Private)
    }
}

class DefaultBrowsingModeManager(
    private val storage: BrowserModeStorage,
    private val modeDidChange: (BrowsingModeManager.Mode) -> Unit
) : BrowsingModeManager {
    override val isPrivate: Boolean
        get() = mode == BrowsingModeManager.Mode.Private

    override var mode: BrowsingModeManager.Mode
        get() = storage.currentMode()
        set(value) {
            storage.setMode(value)
            modeDidChange(mode)
        }
}

class CustomTabBrowsingModeManager : BrowsingModeManager {
    override val isPrivate = false
    override var mode: BrowsingModeManager.Mode
        get() = BrowsingModeManager.Mode.Normal
        set(_) { return }
}
