/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

package org.mozilla.focus.settings

import android.content.SharedPreferences
import android.os.Bundle
import androidx.preference.PreferenceFragmentCompat
import androidx.preference.SwitchPreferenceCompat
import com.jakewharton.processphoenix.ProcessPhoenix
import kotlinx.coroutines.CoroutineScope
import kotlinx.coroutines.Dispatchers
import kotlinx.coroutines.Dispatchers.IO
import kotlinx.coroutines.Job
import kotlinx.coroutines.launch
import org.mozilla.focus.R
import org.mozilla.focus.utils.AppConstants
import org.mozilla.focus.utils.app
import org.mozilla.focus.utils.geckoEngineExperimentDescriptor
import org.mozilla.focus.web.Config
import org.mozilla.focus.web.ENGINE_PREF_STRING_KEY
import kotlin.coroutines.CoroutineContext

class ExperimentsSettingsFragment : PreferenceFragmentCompat(),
        SharedPreferences.OnSharedPreferenceChangeListener,
        CoroutineScope {
    companion object {
        const val FRAGMENT_TAG = "ExperimentSettings"
    }

    private var job = Job()
    override val coroutineContext: CoroutineContext
        get() = job + Dispatchers.Main
    private var enginePref: SwitchPreferenceCompat? = null

    override fun onCreatePreferences(savedInstanceState: Bundle?, rootKey: String?) {
        addPreferencesFromResource(R.xml.experiments_settings)
        enginePref = preferenceManager!!.findPreference(ENGINE_PREF_STRING_KEY) as SwitchPreferenceCompat?
        enginePref?.isChecked = AppConstants.isGeckoBuild
    }

    override fun onResume() {
        super.onResume()

        if (job.isCancelled) {
            job = Job()
        }

        preferenceScreen?.sharedPreferences?.registerOnSharedPreferenceChangeListener(this)
    }

    override fun onPause() {
        preferenceScreen?.sharedPreferences?.unregisterOnSharedPreferenceChangeListener(this)
        job.cancel()
        super.onPause()
    }

    override fun onSharedPreferenceChanged(sharedPreferences: SharedPreferences?, key: String?) {
        when (key) {
            ENGINE_PREF_STRING_KEY -> {
                val newValue = sharedPreferences!!.getBoolean(key, Config.DEFAULT_NEW_RENDERER)
                if (AppConstants.isGeckoBuild != newValue) {
                    val app = activity!!.app
                    launch(IO) {
                        activity!!.app.fretboard.setOverrideNow(
                            activity!!.app, geckoEngineExperimentDescriptor, newValue
                        )
                        val launcherIntent = app.packageManager?.getLaunchIntentForPackage(app.packageName)
                        ProcessPhoenix.triggerRebirth(app, launcherIntent)
                    }
                }
            }
        }
    }
}
