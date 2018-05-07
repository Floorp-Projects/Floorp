/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

package org.mozilla.focus.settings

import android.content.SharedPreferences
import android.os.Bundle
import android.preference.Preference
import android.preference.PreferenceScreen
import org.mozilla.focus.R
import org.mozilla.focus.autocomplete.AutocompleteSettingsFragment
import org.mozilla.focus.telemetry.TelemetryWrapper

class SearchSettingsFragment : BaseSettingsFragment(),
        SharedPreferences.OnSharedPreferenceChangeListener {

    override fun onCreate(savedInstanceState: Bundle?) {
        super.onCreate(savedInstanceState)
        addPreferencesFromResource(R.xml.search_settings)
    }

    override fun onResume() {
        super.onResume()

        preferenceManager.sharedPreferences.registerOnSharedPreferenceChangeListener(this)

        // Update title and icons when returning to fragments.
        val updater = activity as BaseSettingsFragment.ActionBarUpdater
        updater.updateTitle(R.string.preference_category_search)
        updater.updateIcon(R.drawable.ic_back)
    }

    override fun onPause() {
        preferenceManager.sharedPreferences.unregisterOnSharedPreferenceChangeListener(this)
        super.onPause()
    }

    override fun onPreferenceTreeClick(preferenceScreen: PreferenceScreen?, preference: Preference?): Boolean {
        when (preference?.key) {
            resources.getString(R.string.pref_key_search_engine) -> run {
                navigateToFragment(InstalledSearchEnginesSettingsFragment())
                TelemetryWrapper.openSearchSettingsEvent()
            }
            resources.getString(R.string.pref_key_screen_autocomplete) -> navigateToFragment(AutocompleteSettingsFragment())
        }
        return super.onPreferenceTreeClick(preferenceScreen, preference)
    }

    override fun onSharedPreferenceChanged(sharedPreferences: SharedPreferences, key: String) {
        TelemetryWrapper.settingsEvent(key, sharedPreferences.all[key].toString())
    }

    companion object {

        fun newInstance(): SearchSettingsFragment {
            return SearchSettingsFragment()
        }
    }
}
