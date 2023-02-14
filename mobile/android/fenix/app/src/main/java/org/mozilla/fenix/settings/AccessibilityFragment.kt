/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

package org.mozilla.fenix.settings

import android.os.Bundle
import androidx.preference.PreferenceFragmentCompat
import androidx.preference.SwitchPreference
import org.mozilla.fenix.R
import org.mozilla.fenix.ext.components
import org.mozilla.fenix.ext.settings
import org.mozilla.fenix.ext.showToolbar

/**
 * Displays font size controls for accessibility.
 *
 * Includes an automatic font sizing toggle. When turned on, font sizing follows the Android device settings.
 * When turned off, the font sizing can be controlled manually within the app.
 */
class AccessibilityFragment : PreferenceFragmentCompat() {
    override fun onResume() {
        super.onResume()
        showToolbar(getString(R.string.preferences_accessibility))

        val forceZoomPreference = requirePreference<SwitchPreference>(
            R.string.pref_key_accessibility_force_enable_zoom,
        )

        forceZoomPreference.setOnPreferenceChangeListener<Boolean> { preference, shouldForce ->
            val settings = preference.context.settings()
            val components = preference.context.components

            settings.forceEnableZoom = shouldForce
            components.core.engine.settings.forceUserScalableContent = shouldForce

            true
        }

        val textSizePreference = requirePreference<TextPercentageSeekBarPreference>(
            R.string.pref_key_accessibility_font_scale,
        )
        textSizePreference.setOnPreferenceChangeListener<Int> { preference, newTextSize ->
            val settings = preference.context.settings()
            val components = preference.context.components

            // Value is mapped from 0->30 in steps of 1 so let's convert to float in range 0.5->2.0
            val newTextScale =
                ((newTextSize * STEP_SIZE) + MIN_SCALE_VALUE).toFloat() / PERCENT_TO_DECIMAL

            // Save new text scale value. We assume auto sizing is off if this change listener was called.
            settings.fontSizeFactor = newTextScale
            components.core.engine.settings.fontSizeFactor = newTextScale

            // Reload the current session to reflect the new text scale
            components.useCases.sessionUseCases.reload()
            true
        }
        textSizePreference.isEnabled = !requireContext().settings().shouldUseAutoSize

        val useAutoSizePreference =
            requirePreference<SwitchPreference>(R.string.pref_key_accessibility_auto_size)
        useAutoSizePreference.setOnPreferenceChangeListener<Boolean> { preference, useAutoSize ->
            val settings = preference.context.settings()
            val components = preference.context.components

            // Save the new setting value
            settings.shouldUseAutoSize = useAutoSize
            components.core.engine.settings.automaticFontSizeAdjustment = useAutoSize
            components.core.engine.settings.fontInflationEnabled = useAutoSize

            // If using manual sizing, update the engine settings with the local saved setting
            if (!useAutoSize) {
                components.core.engine.settings.fontSizeFactor = settings.fontSizeFactor
            }

            // Enable the manual sizing controls if automatic sizing is turned off.
            textSizePreference.isEnabled = !useAutoSize

            // Reload the current session to reflect the new text scale
            components.useCases.sessionUseCases.reload()
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
