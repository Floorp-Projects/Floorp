/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

package org.mozilla.focus.settings.advanced

import android.content.SharedPreferences
import android.os.Bundle
import androidx.preference.Preference
import org.mozilla.focus.GleanMetrics.AdvancedSettings
import org.mozilla.focus.R
import org.mozilla.focus.ext.application
import org.mozilla.focus.ext.getPreferenceKey
import org.mozilla.focus.ext.requireComponents
import org.mozilla.focus.ext.showToolbar
import org.mozilla.focus.settings.BaseSettingsFragment
import org.mozilla.focus.state.AppAction
import org.mozilla.focus.state.Screen
import org.mozilla.focus.utils.AppConstants.isDevBuild

class AdvancedSettingsFragment :
    BaseSettingsFragment(),
    SharedPreferences.OnSharedPreferenceChangeListener {

    override fun onCreatePreferences(p0: Bundle?, p1: String?) {
        addPreferencesFromResource(R.xml.advanced_settings)
        findPreference<Preference>(getPreferenceKey(R.string.pref_key_secret_settings))?.isVisible =
            requireComponents.appStore.state.secretSettingsEnabled
        findPreference<Preference>(getPreferenceKey(R.string.pref_key_leakcanary))?.isVisible =
            isDevBuild
    }

    override fun onResume() {
        super.onResume()

        preferenceManager.sharedPreferences?.registerOnSharedPreferenceChangeListener(this)

        showToolbar(getString(R.string.preference_category_advanced))
    }

    override fun onPause() {
        preferenceManager.sharedPreferences?.unregisterOnSharedPreferenceChangeListener(this)
        super.onPause()
    }

    override fun onSharedPreferenceChanged(sharedPreferences: SharedPreferences, key: String?) {
        when (key) {
            getString(R.string.pref_key_remote_debugging) -> {
                requireComponents.engine.settings.remoteDebuggingEnabled =
                    sharedPreferences.getBoolean(key, false)

                AdvancedSettings.remoteDebugSettingChanged.record(
                    AdvancedSettings.RemoteDebugSettingChangedExtra(sharedPreferences.all[key] as Boolean),
                )
            }

            getString(R.string.pref_key_open_links_in_external_app) ->
                AdvancedSettings.openLinksSettingChanged.record(
                    AdvancedSettings.OpenLinksSettingChangedExtra(sharedPreferences.all[key] as Boolean),
                )
            getString(R.string.pref_key_leakcanary) -> {
                context?.application?.updateLeakCanaryState(sharedPreferences.all[key] as Boolean)
            }
        }
    }

    override fun onPreferenceTreeClick(preference: Preference): Boolean {
        if (preference.key == resources.getString(R.string.pref_key_secret_settings)) {
            requireComponents.appStore.dispatch(AppAction.OpenSettings(page = Screen.Settings.Page.SecretSettings))
        }
        return super.onPreferenceTreeClick(preference)
    }

    companion object {

        fun newInstance(): AdvancedSettingsFragment {
            return AdvancedSettingsFragment()
        }
    }
}
