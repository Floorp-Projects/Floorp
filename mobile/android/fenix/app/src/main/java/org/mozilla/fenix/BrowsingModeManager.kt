/* This Source Code Form is subject to the terms of the Mozilla Public
   License, v. 2.0. If a copy of the MPL was not distributed with this
   file, You can obtain one at http://mozilla.org/MPL/2.0/. */

package org.mozilla.fenix

import android.preference.PreferenceManager

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
            updateTheme(value)
            setPreference()
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
        PreferenceManager.getDefaultSharedPreferences(homeActivity)
            .edit().putBoolean(homeActivity.getString(R.string.pref_key_private_mode), isPrivate).apply()
    }

    init {
        if (temporaryModeStorage == null) {
            mode = when (PreferenceManager.getDefaultSharedPreferences(homeActivity)
                .getBoolean(homeActivity.getString(R.string.pref_key_private_mode), false)) {
                true -> BrowsingModeManager.Mode.Private
                false -> BrowsingModeManager.Mode.Normal
            }
        }
    }
}
