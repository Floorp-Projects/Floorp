/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

package org.mozilla.focus.settings

import android.content.SharedPreferences
import android.os.Bundle
import org.mozilla.focus.GleanMetrics.AdvancedSettings
import org.mozilla.focus.R
import org.mozilla.focus.ext.requireComponents

class AdvancedSettingsFragment :
    BaseSettingsFragment(),
    SharedPreferences.OnSharedPreferenceChangeListener {

    override fun onCreatePreferences(p0: Bundle?, p1: String?) {
        addPreferencesFromResource(R.xml.advanced_settings)
    }

    override fun onResume() {
        super.onResume()

        preferenceManager.sharedPreferences.registerOnSharedPreferenceChangeListener(this)

        updateTitle(R.string.preference_category_advanced)
    }

    override fun onPause() {
        preferenceManager.sharedPreferences.unregisterOnSharedPreferenceChangeListener(this)
        super.onPause()
    }

    override fun onSharedPreferenceChanged(sharedPreferences: SharedPreferences, key: String) {
        when (key) {
            getString(R.string.pref_key_remote_debugging) -> {
                requireComponents.engine.settings.remoteDebuggingEnabled =
                    sharedPreferences.getBoolean(key, false)

                AdvancedSettings.remoteDebugSettingChanged.record(
                    AdvancedSettings.RemoteDebugSettingChangedExtra(sharedPreferences.all[key] as Boolean)
                )
            }

            getString(R.string.pref_key_open_links_in_external_app) ->
                AdvancedSettings.openLinksSettingChanged.record(
                    AdvancedSettings.OpenLinksSettingChangedExtra(sharedPreferences.all[key] as Boolean)
                )
        }
    }

    companion object {

        fun newInstance(): AdvancedSettingsFragment {
            return AdvancedSettingsFragment()
        }
    }
}
