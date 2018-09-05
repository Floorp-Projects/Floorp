/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

package org.mozilla.focus.settings

import android.content.SharedPreferences
import android.os.Bundle
import android.support.v7.preference.PreferenceFragmentCompat
import android.support.v7.preference.SwitchPreferenceCompat
import com.jakewharton.processphoenix.ProcessPhoenix
import org.mozilla.focus.R
import org.mozilla.focus.session.SessionManager
import org.mozilla.focus.utils.AppConstants
import org.mozilla.focus.utils.app
import org.mozilla.focus.utils.geckoEngineExperimentDescriptor
import org.mozilla.focus.web.Config
import org.mozilla.focus.web.ENGINE_PREF_STRING_KEY

class ExperimentsSettingsFragment : PreferenceFragmentCompat(),
        SharedPreferences.OnSharedPreferenceChangeListener {
    companion object {
        const val FRAGMENT_TAG = "ExperimentSettings"
    }

    private var enginePref: SwitchPreferenceCompat? = null

    override fun onCreatePreferences(savedInstanceState: Bundle?, rootKey: String?) {
        addPreferencesFromResource(R.xml.experiments_settings)
        enginePref = preferenceManager!!.findPreference(ENGINE_PREF_STRING_KEY) as SwitchPreferenceCompat?
        enginePref?.isChecked = AppConstants.isGeckoBuild
    }

    override fun onResume() {
        super.onResume()
        preferenceScreen?.sharedPreferences?.registerOnSharedPreferenceChangeListener(this)
    }

    override fun onPause() {
        super.onPause()
        preferenceScreen?.sharedPreferences?.unregisterOnSharedPreferenceChangeListener(this)
        if (AppConstants.isGeckoBuild != enginePref?.isChecked) {
            val launcherIntent = activity?.packageManager?.getLaunchIntentForPackage(activity!!.packageName)
            ProcessPhoenix.triggerRebirth(context, launcherIntent)
        } else {
            SessionManager.getInstance().removeAllSessions()
        }
    }

    override fun onSharedPreferenceChanged(sharedPreferences: SharedPreferences?, key: String?) {
        when (key) {
            ENGINE_PREF_STRING_KEY -> {
                val newValue = sharedPreferences!!.getBoolean(key, Config.DEFAULT_NEW_RENDERER)
                activity!!.app.fretboard.setOverride(
                    activity!!.app, geckoEngineExperimentDescriptor, newValue
                )
            }
        }
    }
}
