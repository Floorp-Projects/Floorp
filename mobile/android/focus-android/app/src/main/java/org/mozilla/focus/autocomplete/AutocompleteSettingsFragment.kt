/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

package org.mozilla.focus.autocomplete

import android.content.SharedPreferences
import android.os.Bundle
import org.mozilla.focus.R
import org.mozilla.focus.settings.BaseSettingsFragment
import org.mozilla.focus.telemetry.TelemetryWrapper

/**
 * Settings UI for configuring autocomplete.
 */
class AutocompleteSettingsFragment : BaseSettingsFragment(), SharedPreferences.OnSharedPreferenceChangeListener {
    override fun onCreatePreferences(p0: Bundle?, p1: String?) {
        addPreferencesFromResource(R.xml.autocomplete)
    }

    override fun onResume() {
        super.onResume()

        val updater = activity as BaseSettingsFragment.ActionBarUpdater
        updater.updateTitle(R.string.preference_subitem_autocomplete)
        updater.updateIcon(R.drawable.ic_back)

        preferenceManager.sharedPreferences.registerOnSharedPreferenceChangeListener(this)
    }

    override fun onPause() {
        super.onPause()

        preferenceManager.sharedPreferences.unregisterOnSharedPreferenceChangeListener(this)
    }

    override fun onPreferenceTreeClick(preference: androidx.preference.Preference?): Boolean {
        preference?.let {
            if (it.key == getString(R.string.pref_key_screen_custom_domains)) {
                navigateToFragment(AutocompleteListFragment())
            }
        }
        return super.onPreferenceTreeClick(preference)
    }

    override fun onSharedPreferenceChanged(sharedPreferences: SharedPreferences?, key: String?) {
        if (key == null || sharedPreferences == null) {
            return
        }

        TelemetryWrapper.settingsEvent(key, sharedPreferences.all[key].toString())
    }
}
