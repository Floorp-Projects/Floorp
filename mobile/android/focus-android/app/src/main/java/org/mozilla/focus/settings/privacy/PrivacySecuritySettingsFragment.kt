/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

package org.mozilla.focus.settings.privacy

import android.content.SharedPreferences
import android.os.Build
import android.os.Bundle
import androidx.preference.Preference
import androidx.preference.SwitchPreferenceCompat
import org.mozilla.focus.R
import org.mozilla.focus.biometrics.Biometrics
import org.mozilla.focus.ext.requireComponents
import org.mozilla.focus.settings.BaseSettingsFragment
import org.mozilla.focus.state.AppAction
import org.mozilla.focus.state.Screen
import org.mozilla.focus.telemetry.TelemetryWrapper
import org.mozilla.focus.widget.CookiesPreference

class PrivacySecuritySettingsFragment : BaseSettingsFragment(),
    SharedPreferences.OnSharedPreferenceChangeListener {
    override fun onCreatePreferences(p0: Bundle?, p1: String?) {
        addPreferencesFromResource(R.xml.privacy_security_settings)

        val biometricPreference: SwitchPreferenceCompat? = findPreference(getString(R.string.pref_key_biometric))
        val appName = getString(R.string.app_name)
        biometricPreference?.summary =
                getString(R.string.preference_security_biometric_summary, appName)

        // Remove the biometric toggle if the software or hardware do not support it
        if (Build.VERSION.SDK_INT < Build.VERSION_CODES.M || !Biometrics.hasFingerprintHardware(
                requireContext()
            )
        ) {
            preferenceScreen.removePreference(biometricPreference)
        }

        val cookiesPreference =
            findPreference(getString(R.string.pref_key_performance_enable_cookies)) as? CookiesPreference

        cookiesPreference?.updateSummary()
    }

    override fun onResume() {
        super.onResume()
        updateBiometricsToggleAvailability()
        updateStealthToggleAvailability()
        updateExceptionSettingAvailability()

        preferenceManager.sharedPreferences.registerOnSharedPreferenceChangeListener(this)

        // Update title and icons when returning to fragments.
        updateTitle(R.string.preference_privacy_and_security_header)
    }

    override fun onPause() {
        preferenceManager.sharedPreferences.unregisterOnSharedPreferenceChangeListener(this)
        super.onPause()
    }

    override fun onSharedPreferenceChanged(sharedPreferences: SharedPreferences, key: String) {
        TelemetryWrapper.settingsEvent(key, sharedPreferences.all[key].toString())
        updateStealthToggleAvailability()
    }

    private fun updateBiometricsToggleAvailability() {
        val switch = preferenceScreen.findPreference(resources.getString(R.string.pref_key_biometric))
                as? SwitchPreferenceCompat

        if (!Biometrics.hasFingerprintHardware(requireContext())) {
            switch?.isChecked = false
            switch?.isEnabled = false
            preferenceManager.sharedPreferences
                    .edit()
                    .putBoolean(resources.getString(R.string.pref_key_biometric), false)
                    .apply()
        } else {
            switch?.isEnabled = true
        }
    }

    private fun updateExceptionSettingAvailability() {
        val exceptionsPreference: Preference? = findPreference(getString(R.string.pref_key_screen_exceptions))
        exceptionsPreference?.isEnabled = false

        requireComponents.trackingProtectionUseCases.fetchExceptions.invoke { exceptions ->
            exceptionsPreference?.isEnabled = exceptions.isNotEmpty()
        }
    }

    override fun onPreferenceTreeClick(preference: Preference): Boolean {
        when (preference.key) {
            resources.getString(R.string.pref_key_screen_exceptions) -> {
                TelemetryWrapper.openExceptionsListSetting()
                requireComponents.appStore.dispatch(
                    AppAction.OpenSettings(page = Screen.Settings.Page.PrivacyExceptions)
                )
            }
            resources.getString(R.string.pref_key_secure),
            resources.getString(R.string.pref_key_biometric) -> {
                // We need to recreate the activity to apply the SECURE flags.
                requireActivity().recreate()
            }
        }
        return super.onPreferenceTreeClick(preference)
    }

    private fun updateStealthToggleAvailability() {
        val switch =
            preferenceScreen.findPreference(resources.getString(R.string.pref_key_secure)) as? SwitchPreferenceCompat
        if (preferenceManager.sharedPreferences
                .getBoolean(
                    resources.getString(R.string.pref_key_biometric),
                    false
                )
        ) {
            preferenceManager.sharedPreferences
                .edit().putBoolean(
                    resources.getString(R.string.pref_key_secure),
                    true
                ).apply()
            // Disable the stealth switch
            switch?.isChecked = true
            switch?.isEnabled = false
        } else {
            // Enable the stealth switch
            switch?.isEnabled = true
        }
    }

    companion object {
        const val FRAGMENT_TAG = "PrivacySecuritySettings"

        fun newInstance(): PrivacySecuritySettingsFragment {
            return PrivacySecuritySettingsFragment()
        }
    }
}
