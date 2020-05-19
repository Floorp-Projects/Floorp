/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */
@file:Suppress("ForbiddenComment")

package org.mozilla.focus.settings

import android.content.SharedPreferences
import android.os.Bundle
import org.mozilla.focus.R
import org.mozilla.focus.telemetry.TelemetryWrapper

class SettingsFragment : BaseSettingsFragment(), SharedPreferences.OnSharedPreferenceChangeListener {

    override fun onCreatePreferences(bundle: Bundle?, s: String?) {
        addPreferencesFromResource(R.xml.settings)
    }

    override fun onResume() {
        super.onResume()

        preferenceManager.sharedPreferences.registerOnSharedPreferenceChangeListener(this)

        // Update title and icons when returning to fragments.
        val updater = activity as BaseSettingsFragment.ActionBarUpdater?
        updater!!.updateTitle(R.string.menu_settings)
        updater.updateIcon(R.drawable.ic_back)
    }

    override fun onPause() {
        preferenceManager.sharedPreferences.unregisterOnSharedPreferenceChangeListener(this)
        super.onPause()
    }

    override fun onPreferenceTreeClick(preference: androidx.preference.Preference): Boolean {
        val resources = resources

        when {
            preference.key == resources.getString(R.string
                .pref_key_general_screen) -> navigateToFragment(GeneralSettingsFragment())
            preference.key == resources.getString(R.string
                    .pref_key_privacy_security_screen) -> navigateToFragment(PrivacySecuritySettingsFragment())
            preference.key == resources.getString(R.string
                    .pref_key_search_screen) -> navigateToFragment(SearchSettingsFragment())
            preference.key == resources.getString(R.string
                    .pref_key_advanced_screen) -> navigateToFragment(AdvancedSettingsFragment())
            preference.key == resources.getString(R.string
                    .pref_key_mozilla_screen) -> navigateToFragment(MozillaSettingsFragment())
        }

        return super.onPreferenceTreeClick(preference)
    }

    override fun onSharedPreferenceChanged(sharedPreferences: SharedPreferences, key: String) {
        TelemetryWrapper.settingsEvent(key, sharedPreferences.all[key].toString())
    }

    companion object {

        fun newInstance(): SettingsFragment {
            return SettingsFragment()
        }
    }
}
