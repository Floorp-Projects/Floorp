/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

package org.mozilla.fenix.settings

import android.os.Bundle
import androidx.appcompat.app.AppCompatActivity
import androidx.preference.PreferenceFragmentCompat
import androidx.preference.SwitchPreference
import org.mozilla.fenix.R
import org.mozilla.fenix.ext.requireComponents
import org.mozilla.fenix.utils.Settings

class DataChoicesFragment : PreferenceFragmentCompat() {

    override fun onCreate(savedInstanceState: Bundle?) {
        super.onCreate(savedInstanceState)
        (activity as AppCompatActivity).title = getString(R.string.preferences_data_choices)
        (activity as AppCompatActivity).supportActionBar?.show()

        preferenceManager.sharedPreferences.registerOnSharedPreferenceChangeListener { sharedPreferences, key ->
            when (key) {
                getString(R.string.pref_key_telemetry) -> {
                    if (sharedPreferences.getBoolean(key, Settings.getInstance(requireContext()).isTelemetryEnabled)) {
                        requireComponents.analytics.metrics.start()
                    } else {
                        requireComponents.analytics.metrics.stop()
                    }
                }
            }
        }

        findPreference<SwitchPreference>(getString(R.string.pref_key_fenix_health_report))?.apply {
            val appName = getString(R.string.app_name)
            title = getString(R.string.preferences_fenix_health_report, appName)
        }
    }

    override fun onCreatePreferences(savedInstanceState: Bundle?, rootKey: String?) {
        setPreferencesFromResource(R.xml.data_choices_preferences, rootKey)

        findPreference<SwitchPreference>(getString(R.string.pref_key_telemetry))?.apply {
            isChecked = Settings.getInstance(context).isTelemetryEnabled
        }
    }
}
