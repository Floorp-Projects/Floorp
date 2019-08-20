/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

package org.mozilla.fenix.settings

import android.os.Bundle
import androidx.appcompat.app.AppCompatActivity
import androidx.preference.Preference
import androidx.preference.PreferenceFragmentCompat
import androidx.preference.SwitchPreference
import org.mozilla.fenix.R
import org.mozilla.fenix.ext.requireComponents
import org.mozilla.fenix.utils.Settings

class AccessibilityFragment : PreferenceFragmentCompat() {
    override fun onResume() {
        super.onResume()
        (activity as AppCompatActivity).title = getString(R.string.preferences_accessibility)
        (activity as AppCompatActivity).supportActionBar?.show()
        val textSizePreference =
            findPreference<TextPercentageSeekBarPreference>(getString(R.string.pref_key_accessibility_font_scale))
        textSizePreference?.onPreferenceChangeListener =
            Preference.OnPreferenceChangeListener { _, newValue ->
                (newValue as? Int).let {
                    // Value is mapped from 0->30 in steps of 1 so let's convert to float in range 0.5->2.0
                    val newTextScale = ((newValue as Int * STEP_SIZE) + MIN_SCALE_VALUE).toFloat() / PERCENT_TO_DECIMAL
                    Settings.getInstance(context!!).fontSizeFactor = newTextScale
                    requireComponents.core.engine.settings.fontSizeFactor = newTextScale
                    requireComponents.useCases.sessionUseCases.reload.invoke()
                }
                true
            }

        textSizePreference?.isVisible = !Settings.getInstance(context!!).shouldUseAutoSize

        val useAutoSizePreference =
            findPreference<SwitchPreference>(getString(R.string.pref_key_accessibility_auto_size))
        useAutoSizePreference?.setOnPreferenceChangeListener { _, newValue ->
            Settings.getInstance(context!!).shouldUseAutoSize = newValue as Boolean
            requireComponents.core.engine.settings.automaticFontSizeAdjustment = newValue
            if (!newValue) {
                requireComponents.core.engine.settings.fontSizeFactor = Settings.getInstance(context!!).fontSizeFactor
            }
            textSizePreference?.isVisible = !newValue
            requireComponents.useCases.sessionUseCases.reload.invoke()
            true
        }
    }

    override fun onCreatePreferences(savedInstanceState: Bundle?, rootKey: String?) {
        setPreferencesFromResource(R.xml.accessibility_preferences, rootKey)
    }

    companion object {
        const val MIN_SCALE_VALUE = 50
        const val STEP_SIZE = 5
        const val PERCENT_TO_DECIMAL = 100f
    }
}
