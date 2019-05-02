/* This Source Code Form is subject to the terms of the Mozilla Public
   License, v. 2.0. If a copy of the MPL was not distributed with this
   file, You can obtain one at http://mozilla.org/MPL/2.0/. */

package org.mozilla.fenix

import org.mozilla.fenix.utils.Settings

interface BrowsingModeManager {
    enum class Mode {
        Normal, Private
    }
}

var temporaryModeStorage: BrowsingModeManager.Mode? = null
class DefaultBrowsingModeManager(private val homeActivity: HomeActivity) : BrowsingModeManager {
    val isPrivate: Boolean
        get() = mode == BrowsingModeManager.Mode.Private
    var mode: BrowsingModeManager.Mode
        get() = temporaryModeStorage!!
        set(value) {
            temporaryModeStorage = value
            setPreference()
            updateTheme(value)
        }

    private fun updateTheme(mode: BrowsingModeManager.Mode) {
        homeActivity.themeManager.apply {
            val newTheme = when (mode) {
                BrowsingModeManager.Mode.Normal -> ThemeManager.Theme.Normal
                BrowsingModeManager.Mode.Private -> ThemeManager.Theme.Private
            }
            setTheme(newTheme)
        }
    }

    private fun setPreference() {
        Settings.getInstance(homeActivity).setPrivateMode(isPrivate)
    }

    init {
        if (temporaryModeStorage == null) {
            mode = when (Settings.getInstance(homeActivity).usePrivateMode) {
                true -> BrowsingModeManager.Mode.Private
                false -> BrowsingModeManager.Mode.Normal
            }
        }
    }
}
