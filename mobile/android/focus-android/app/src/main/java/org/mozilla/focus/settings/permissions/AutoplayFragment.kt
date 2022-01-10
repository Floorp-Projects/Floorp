/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */
package org.mozilla.focus.settings.permissions

import android.os.Bundle
import androidx.preference.Preference
import org.mozilla.focus.R
import org.mozilla.focus.ext.requireComponents
import org.mozilla.focus.ext.requirePreference
import org.mozilla.focus.settings.BaseSettingsFragment
import org.mozilla.focus.settings.RadioButtonPreference
import org.mozilla.focus.settings.addToRadioGroup
import org.mozilla.focus.state.AppAction

class AutoplayFragment : BaseSettingsFragment() {

    private lateinit var radioAllowAudioVideo: RadioButtonPreference
    private lateinit var radioBlockAudioOnly: RadioButtonPreference
    private lateinit var radioBlockAudioVideo: RadioButtonPreference

    override fun onStart() {
        super.onStart()
        updateTitle(R.string.preference_autoplay)
    }

    override fun onCreatePreferences(savedInstanceState: Bundle?, rootKey: String?) {
        addPreferencesFromResource(R.xml.site_permissions_autoplay)
        setupPreferences()
    }

    private fun setupPreferences() {
        radioAllowAudioVideo = requirePreference(R.string.pref_key_allow_autoplay_audio_video)
        radioAllowAudioVideo.onPreferenceChangeListener = radioButtonClickListener

        radioBlockAudioOnly = requirePreference(R.string.pref_key_block_autoplay_audio_only)
        radioBlockAudioOnly.onPreferenceChangeListener = radioButtonClickListener

        radioBlockAudioVideo = requirePreference(R.string.pref_key_block_autoplay_audio_video)
        radioBlockAudioVideo.onPreferenceChangeListener = radioButtonClickListener

        addToRadioGroup(
            radioAllowAudioVideo,
            radioBlockAudioOnly,
            radioBlockAudioVideo
        )

        requirePreference<RadioButtonPreference>(
            requireComponents.settings.currentAutoplayOption.prefKeyId
        ).updateRadioValue(
            true
        )
    }

    private val radioButtonClickListener =
        Preference.OnPreferenceChangeListener { preference, newValue ->
            if (newValue as Boolean) {
                requireComponents.settings.updateAutoplayPrefKey(preference.key)
                requireComponents.appStore.dispatch(AppAction.AutoplayChange(true))
            }
            true
        }
}
