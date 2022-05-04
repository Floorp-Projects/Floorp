/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

package org.mozilla.focus.settings.privacy

import android.content.SharedPreferences
import android.os.Build
import android.os.Bundle
import androidx.preference.Preference
import androidx.preference.SwitchPreferenceCompat
import mozilla.components.lib.auth.canUseBiometricFeature
import mozilla.components.service.glean.private.NoExtras
import org.mozilla.focus.GleanMetrics.PrivacySettings
import org.mozilla.focus.GleanMetrics.TrackingProtectionExceptions
import org.mozilla.focus.R
import org.mozilla.focus.engine.EngineSharedPreferencesListener
import org.mozilla.focus.ext.requireComponents
import org.mozilla.focus.ext.settings
import org.mozilla.focus.nimbus.FocusNimbus
import org.mozilla.focus.settings.BaseSettingsFragment
import org.mozilla.focus.state.AppAction
import org.mozilla.focus.state.Screen
import org.mozilla.focus.telemetry.TelemetryWrapper
import org.mozilla.focus.widget.CookiesPreference

class PrivacySecuritySettingsFragment :
    BaseSettingsFragment(),
    SharedPreferences.OnSharedPreferenceChangeListener {
    override fun onCreatePreferences(p0: Bundle?, p1: String?) {
        addPreferencesFromResource(R.xml.privacy_security_settings)

        val biometricPreference: SwitchPreferenceCompat? = findPreference(getString(R.string.pref_key_biometric))
        val appName = getString(R.string.app_name)
        biometricPreference?.summary =
            getString(R.string.preference_security_biometric_summary2, appName)

        // Remove the biometric toggle if the software or hardware do not support it
        if (Build.VERSION.SDK_INT < Build.VERSION_CODES.M || !requireContext().canUseBiometricFeature()
        ) {
            preferenceScreen.removePreference(biometricPreference)
        }
        if (!FocusNimbus.features.onboarding.value().isCfrEnabled ||
            !requireContext().settings.shouldShowPrivacySecuritySettingsToolTip
        ) {
            preferenceScreen.removePreference(findPreference(getString(R.string.pref_key_tool_tip)))
        }

        val preferencesListener = EngineSharedPreferencesListener(requireContext())

        val cookiesPreference =
            findPreference(getString(R.string.pref_key_performance_enable_cookies)) as? CookiesPreference
        cookiesPreference?.updateSummary()

        val safeBrowsingSwitchPreference =
            findPreference(getString(R.string.pref_key_safe_browsing)) as? SwitchPreferenceCompat
        val javaScriptPreference =
            findPreference(getString(R.string.pref_key_performance_block_javascript)) as? SwitchPreferenceCompat
        val webFontsPreference =
            findPreference(getString(R.string.pref_key_performance_block_webfonts)) as? SwitchPreferenceCompat

        cookiesPreference?.onPreferenceChangeListener = preferencesListener
        safeBrowsingSwitchPreference?.onPreferenceChangeListener = preferencesListener
        javaScriptPreference?.onPreferenceChangeListener = preferencesListener
        webFontsPreference?.onPreferenceChangeListener = preferencesListener
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
        recordTelemetry(key, sharedPreferences.all[key])
        TelemetryWrapper.settingsEvent(key, sharedPreferences.all[key].toString())
        updateStealthToggleAvailability()
    }

    private fun recordTelemetry(key: String, newValue: Any?) {
        when (key) {
            getString(R.string.pref_key_telemetry) -> PrivacySettings.telemetrySettingChanged.record(
                PrivacySettings.TelemetrySettingChangedExtra(newValue as? Boolean)
            )
            getString(R.string.pref_key_safe_browsing) -> PrivacySettings.safeBrowsingSettingChanged.record(
                PrivacySettings.SafeBrowsingSettingChangedExtra(newValue as? Boolean)
            )
            getString(R.string.pref_key_biometric) -> PrivacySettings.unlockSettingChanged.record(
                PrivacySettings.UnlockSettingChangedExtra(newValue as? Boolean)
            )
            getString(R.string.pref_key_secure) -> PrivacySettings.stealthSettingChanged.record(
                PrivacySettings.StealthSettingChangedExtra(newValue as? Boolean)
            )
            getString(R.string.pref_key_performance_enable_cookies) -> PrivacySettings.blockCookiesChanged.record(
                PrivacySettings.BlockCookiesChangedExtra(newValue as? String)
            )
            else -> {
                // Telemetry for the change is recorded elsewhere.
            }
        }
    }

    private fun updateBiometricsToggleAvailability() {
        val switch = preferenceScreen.findPreference(resources.getString(R.string.pref_key_biometric))
            as? SwitchPreferenceCompat

        if (!requireContext().canUseBiometricFeature()) {
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
        val settings = requireContext().settings
        val engineSharedPreferencesListener = EngineSharedPreferencesListener(requireContext())
        when (preference.key) {
            resources.getString(R.string.pref_key_screen_exceptions) -> {
                TrackingProtectionExceptions.allowListOpened.record(NoExtras())

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

            resources.getString(R.string.pref_key_privacy_block_social) ->
                engineSharedPreferencesListener.updateTrackingProtectionPolicy(
                    EngineSharedPreferencesListener.ChangeSource.SETTINGS.source,
                    EngineSharedPreferencesListener.TrackerChanged.SOCIAL.tracker,
                    settings.shouldBlockSocialTrackers()
                )

            resources.getString(R.string.pref_key_privacy_block_ads) ->
                engineSharedPreferencesListener.updateTrackingProtectionPolicy(
                    EngineSharedPreferencesListener.ChangeSource.SETTINGS.source,
                    EngineSharedPreferencesListener.TrackerChanged.ADVERTISING.tracker,
                    settings.shouldBlockAdTrackers()
                )

            resources.getString(R.string.pref_key_privacy_block_analytics) ->
                engineSharedPreferencesListener.updateTrackingProtectionPolicy(
                    EngineSharedPreferencesListener.ChangeSource.SETTINGS.source,
                    EngineSharedPreferencesListener.TrackerChanged.ANALYTICS.tracker,
                    settings.shouldBlockAnalyticTrackers()
                )

            resources.getString(R.string.pref_key_privacy_block_other3) ->
                engineSharedPreferencesListener.updateTrackingProtectionPolicy(
                    EngineSharedPreferencesListener.ChangeSource.SETTINGS.source,
                    EngineSharedPreferencesListener.TrackerChanged.CONTENT.tracker,
                    settings.shouldBlockOtherTrackers()
                )
            resources.getString(R.string.pref_key_site_permissions) ->
                requireComponents.appStore.dispatch(
                    AppAction.OpenSettings(page = Screen.Settings.Page.SitePermissions)
                )
            resources.getString(R.string.pref_key_studies) ->
                requireComponents.appStore.dispatch(
                    AppAction.OpenSettings(page = Screen.Settings.Page.Studies)
                )
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
