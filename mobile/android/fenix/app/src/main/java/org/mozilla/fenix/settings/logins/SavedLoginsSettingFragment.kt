/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

package org.mozilla.fenix.settings.logins

import android.os.Bundle
import androidx.preference.Preference
import androidx.preference.PreferenceFragmentCompat
import org.mozilla.fenix.R
import org.mozilla.fenix.components.metrics.Event
import org.mozilla.fenix.ext.components
import org.mozilla.fenix.ext.metrics
import org.mozilla.fenix.ext.showToolbar
import org.mozilla.fenix.settings.RadioButtonPreference
import org.mozilla.fenix.settings.SharedPreferenceUpdater
import org.mozilla.fenix.settings.requirePreference

class SavedLoginsSettingFragment : PreferenceFragmentCompat() {
    override fun onCreatePreferences(savedInstanceState: Bundle?, rootKey: String?) {
        setPreferencesFromResource(R.xml.save_logins_preferences, rootKey)
    }

    override fun onResume() {
        super.onResume()
        showToolbar(getString(R.string.preferences_passwords_save_logins))
        val save = bindSave()
        val neverSave = bindNeverSave()
        setupRadioGroups(save, neverSave)
    }

    private fun bindSave(): RadioButtonPreference {
        val preferenceSave = requirePreference<RadioButtonPreference>(R.string.pref_key_save_logins)
        preferenceSave.onPreferenceChangeListener = object : SharedPreferenceUpdater() {
            override fun onPreferenceChange(preference: Preference, newValue: Any?): Boolean {
                if (newValue == true) {
                    context?.metrics?.track(
                        Event.SaveLoginsSettingChanged(
                            Event.SaveLoginsSettingChanged.Setting.ASK_TO_SAVE
                        )
                    )
                }
                // We want to reload the current session here so we can try to fill the current page
                context?.components?.useCases?.sessionUseCases?.reload?.invoke()
                return super.onPreferenceChange(preference, newValue)
            }
        }
        return preferenceSave
    }

    private fun bindNeverSave(): RadioButtonPreference {
        val preferenceNeverSave = requirePreference<RadioButtonPreference>(R.string.pref_key_never_save_logins)
        preferenceNeverSave.onPreferenceChangeListener = object : SharedPreferenceUpdater() {
            override fun onPreferenceChange(preference: Preference, newValue: Any?): Boolean {
                if (newValue == true) {
                    context?.metrics?.track(
                        Event.SaveLoginsSettingChanged(
                            Event.SaveLoginsSettingChanged.Setting.NEVER_SAVE
                        )
                    )
                }
                // We want to reload the current session here so we don't save any currently inserted login
                context?.components?.useCases?.sessionUseCases?.reload?.invoke()
                return super.onPreferenceChange(preference, newValue)
            }
        }
        return preferenceNeverSave
    }

    private fun setupRadioGroups(
        radioNeverSave: RadioButtonPreference,
        radioSave: RadioButtonPreference
    ) {
        radioNeverSave.addToRadioGroup(radioSave)
        radioSave.addToRadioGroup(radioNeverSave)
    }
}
