/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

package org.mozilla.focus.settings

import android.content.SharedPreferences
import android.os.Bundle
import android.preference.SwitchPreference
import org.mozilla.focus.R
import org.mozilla.focus.telemetry.TelemetryWrapper

class PrivacySecuritySettingsFragment : BaseSettingsFragment(),
        SharedPreferences.OnSharedPreferenceChangeListener {

    override fun onCreate(savedInstanceState: Bundle?) {
        super.onCreate(savedInstanceState)
        addPreferencesFromResource(R.xml.privacy_security_settings)

        updateStealthToggleAvailability()
    }

    override fun onResume() {
        super.onResume()

        preferenceManager.sharedPreferences.registerOnSharedPreferenceChangeListener(this)

        // Update title and icons when returning to fragments.
        val updater = activity as BaseSettingsFragment.ActionBarUpdater
        updater.updateTitle(R.string.preference_privacy_and_security_header)
        updater.updateIcon(R.drawable.ic_back)
    }
    
    override fun onPause() {
        preferenceManager.sharedPreferences.unregisterOnSharedPreferenceChangeListener(this)
        super.onPause()
    }

    override fun onSharedPreferenceChanged(sharedPreferences: SharedPreferences, key: String) {
        TelemetryWrapper.settingsEvent(key, sharedPreferences.all[key].toString())
        updateStealthToggleAvailability()
    }

    fun updateStealthToggleAvailability() {
        val switch =  preferenceScreen.findPreference(resources.getString(R.string.pref_key_secure)) as SwitchPreference
        if (preferenceManager.sharedPreferences.getBoolean(resources.getString(R.string.pref_key_biometric), false)) {
            preferenceManager.sharedPreferences.edit().putBoolean(resources.getString(R.string.pref_key_secure), true).apply()
            // Disable the stealth switch
            switch.isChecked = true
            switch.isEnabled = false
        } else {
            // Enable the stealth switch
            switch.isEnabled = true
        }
    }

    companion object {
        fun newInstance(): PrivacySecuritySettingsFragment {
            return PrivacySecuritySettingsFragment()
        }
    }
}
