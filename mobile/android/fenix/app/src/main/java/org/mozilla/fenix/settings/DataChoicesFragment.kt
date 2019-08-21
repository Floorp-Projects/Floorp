/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

package org.mozilla.fenix.settings

import android.content.SharedPreferences
import android.os.Bundle
import androidx.appcompat.app.AppCompatActivity
import androidx.preference.PreferenceFragmentCompat
import androidx.preference.SwitchPreference
import org.mozilla.fenix.R
import org.mozilla.fenix.ext.components
import org.mozilla.fenix.ext.getPreferenceKey
import org.mozilla.fenix.utils.Settings

/**
 * Lets the user toggle telemetry on/off.
 */
class DataChoicesFragment : PreferenceFragmentCompat() {

    private val preferenceChangeListener =
        SharedPreferences.OnSharedPreferenceChangeListener { sharedPreferences, key ->
            when (key) {
                getPreferenceKey(R.string.pref_key_telemetry) -> {
                    if (sharedPreferences.getBoolean(key, Settings.getInstance(requireContext()).isTelemetryEnabled)) {
                        context?.components?.analytics?.metrics?.start()
                    } else {
                        context?.components?.analytics?.metrics?.stop()
                    }
                }
            }
        }

    override fun onCreate(savedInstanceState: Bundle?) {
        super.onCreate(savedInstanceState)
        context?.let {
            preferenceManager.sharedPreferences.registerOnSharedPreferenceChangeListener(preferenceChangeListener)
        }
    }

    override fun onResume() {
        super.onResume()
        (activity as AppCompatActivity).title = getString(R.string.preferences_data_collection)
        (activity as AppCompatActivity).supportActionBar?.show()
    }

    override fun onDestroy() {
        context?.let {
            preferenceManager.sharedPreferences.unregisterOnSharedPreferenceChangeListener(preferenceChangeListener)
        }
        super.onDestroy()
    }

    override fun onCreatePreferences(savedInstanceState: Bundle?, rootKey: String?) {
        setPreferencesFromResource(R.xml.data_choices_preferences, rootKey)

        findPreference<SwitchPreference>(getPreferenceKey(R.string.pref_key_telemetry))?.apply {
            isChecked = Settings.getInstance(context).isTelemetryEnabled

            val appName = context.getString(R.string.app_name)
            summary = context.getString(R.string.preferences_usage_data_description, appName)

            onPreferenceChangeListener = SharedPreferenceUpdater()
        }
    }
}
