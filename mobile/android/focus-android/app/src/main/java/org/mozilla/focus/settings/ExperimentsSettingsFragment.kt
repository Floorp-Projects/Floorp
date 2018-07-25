/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

package org.mozilla.focus.settings

import android.content.SharedPreferences
import android.os.Bundle
import android.support.v14.preference.SwitchPreference
import android.support.v7.preference.PreferenceFragmentCompat
import com.jakewharton.processphoenix.ProcessPhoenix
import org.mozilla.focus.R
import org.mozilla.focus.web.Config

private const val RENDERER_PREFERENCE_KEY = "renderer_preference"

class ExperimentsSettingsFragment : PreferenceFragmentCompat(), SharedPreferences.OnSharedPreferenceChangeListener {
    companion object {
        const val FRAGMENT_TAG = "ExperimentSettings"
    }

    private var rendererPreferenceChanged = false

    override fun onCreatePreferences(savedInstanceState: Bundle?, rootKey: String?) {
        addPreferencesFromResource(R.xml.experiments_settings)
        val enginePref: SwitchPreference? = preferenceManager
                .findPreference(RENDERER_PREFERENCE_KEY) as SwitchPreference?
        enginePref?.isChecked = preferenceManager.sharedPreferences
                .getBoolean(RENDERER_PREFERENCE_KEY, Config.DEFAULT_NEW_RENDERER)
    }

    override fun onResume() {
        super.onResume()
        preferenceScreen?.sharedPreferences?.registerOnSharedPreferenceChangeListener(this)
    }

    override fun onPause() {
        super.onPause()
        preferenceScreen?.sharedPreferences?.unregisterOnSharedPreferenceChangeListener(this)
        if (rendererPreferenceChanged) {
            val launcherIntent = activity?.packageManager?.getLaunchIntentForPackage(activity?.packageName)
            ProcessPhoenix.triggerRebirth(context, launcherIntent)
        }
    }

    override fun onSharedPreferenceChanged(sharedPreferences: SharedPreferences?, key: String?) {
        when (key) {
            RENDERER_PREFERENCE_KEY -> {
                rendererPreferenceChanged = true
            }
        }
    }
}
