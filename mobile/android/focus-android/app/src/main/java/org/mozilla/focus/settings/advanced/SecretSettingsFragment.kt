/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

package org.mozilla.focus.settings.advanced

import android.content.SharedPreferences
import android.os.Bundle
import androidx.preference.SwitchPreferenceCompat
import org.mozilla.focus.R
import org.mozilla.focus.ext.getPreferenceKey
import org.mozilla.focus.ext.requireComponents
import org.mozilla.focus.ext.requirePreference
import org.mozilla.focus.ext.settings
import org.mozilla.focus.ext.showToolbar
import org.mozilla.focus.settings.BaseSettingsFragment
import kotlin.system.exitProcess

class SecretSettingsFragment :
    BaseSettingsFragment(),
    SharedPreferences.OnSharedPreferenceChangeListener {

    override fun onStart() {
        super.onStart()
        showToolbar(getString(R.string.preference_secret_settings))
        preferenceManager.sharedPreferences?.registerOnSharedPreferenceChangeListener(this)
    }

    override fun onCreatePreferences(savedInstanceState: Bundle?, rootKey: String?) {
        addPreferencesFromResource(R.xml.secret_settings)

        requirePreference<SwitchPreferenceCompat>(R.string.pref_key_remote_server_prod).apply {
            isVisible = true
            isChecked = context.settings.useProductionRemoteSettingsServer
            onPreferenceChangeListener = SharedPreferenceUpdater()
        }
    }

    override fun onSharedPreferenceChanged(sharedPreferences: SharedPreferences, key: String?) {
        findPreference<SwitchPreferenceCompat>(
            getPreferenceKey(R.string.pref_key_use_nimbus_preview),
        )?.let { nimbusPreviewPref ->
            if (key == nimbusPreviewPref.key) {
                requireComponents.settings.shouldUseNimbusPreview = nimbusPreviewPref.isChecked
                quitTheApp()
            }
        }
    }

    private fun quitTheApp() {
        exitProcess(0)
    }
}
