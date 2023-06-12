/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

package org.mozilla.focus.settings

import android.os.Bundle
import org.mozilla.focus.R
import org.mozilla.focus.ext.requireComponents
import org.mozilla.focus.ext.showToolbar
import org.mozilla.focus.state.AppAction
import org.mozilla.focus.state.Screen

class SettingsFragment : BaseSettingsFragment() {

    override fun onCreatePreferences(bundle: Bundle?, s: String?) {
        addPreferencesFromResource(R.xml.settings)
    }

    override fun onResume() {
        super.onResume()

        showToolbar(getString(R.string.menu_settings))
    }

    override fun onPreferenceTreeClick(preference: androidx.preference.Preference): Boolean {
        val resources = resources

        val page = when (preference.key) {
            resources.getString(R.string.pref_key_general_screen) -> Screen.Settings.Page.General
            resources.getString(R.string.pref_key_privacy_security_screen) -> Screen.Settings.Page.Privacy
            resources.getString(R.string.pref_key_search_screen) -> Screen.Settings.Page.Search
            resources.getString(R.string.pref_key_advanced_screen) -> Screen.Settings.Page.Advanced
            resources.getString(R.string.pref_key_mozilla_screen) -> Screen.Settings.Page.Mozilla
            else -> throw IllegalStateException("Unknown preference: ${preference.key}")
        }

        requireComponents.appStore.dispatch(
            AppAction.OpenSettings(page),
        )

        return super.onPreferenceTreeClick(preference)
    }

    companion object {
        fun newInstance(): SettingsFragment {
            return SettingsFragment()
        }
    }
}
