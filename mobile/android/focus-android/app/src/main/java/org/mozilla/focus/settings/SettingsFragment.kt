/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */
@file:Suppress("ForbiddenComment")

package org.mozilla.focus.settings

import android.content.SharedPreferences
import android.os.Bundle
import org.mozilla.focus.R
import org.mozilla.focus.ext.requireComponents
import org.mozilla.focus.state.AppAction
import org.mozilla.focus.state.Screen
import org.mozilla.focus.telemetry.TelemetryWrapper

class SettingsFragment : BaseSettingsFragment(), SharedPreferences.OnSharedPreferenceChangeListener {

    override fun onCreatePreferences(bundle: Bundle?, s: String?) {
        addPreferencesFromResource(R.xml.settings)
    }

    override fun onResume() {
        super.onResume()

        preferenceManager.sharedPreferences.registerOnSharedPreferenceChangeListener(this)

        updateTitle(R.string.menu_settings)
    }

    override fun onPause() {
        preferenceManager.sharedPreferences.unregisterOnSharedPreferenceChangeListener(this)
        super.onPause()
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
            AppAction.OpenSettings(page)
        )

        return super.onPreferenceTreeClick(preference)
    }

    override fun onSharedPreferenceChanged(sharedPreferences: SharedPreferences, key: String) {
        TelemetryWrapper.settingsEvent(key, sharedPreferences.all[key].toString())
    }

    companion object {
        const val TAG = "settings"

        fun newInstance(): SettingsFragment {
            return SettingsFragment()
        }
    }
}
