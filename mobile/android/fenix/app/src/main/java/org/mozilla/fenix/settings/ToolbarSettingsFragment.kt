/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

package org.mozilla.fenix.settings

import android.os.Bundle
import androidx.preference.PreferenceFragmentCompat
import org.mozilla.fenix.R
import org.mozilla.fenix.components.metrics.Event
import org.mozilla.fenix.ext.components
import org.mozilla.fenix.ext.getPreferenceKey
import org.mozilla.fenix.ext.showToolbar

/**
 * Settings to adjust the position of the browser toolbar.
 */
class ToolbarSettingsFragment : PreferenceFragmentCompat() {

    override fun onCreatePreferences(savedInstanceState: Bundle?, rootKey: String?) {
        setPreferencesFromResource(R.xml.toolbar_preferences, rootKey)
    }

    override fun onResume() {
        super.onResume()
        showToolbar(getString(R.string.preferences_toolbar))

        setupPreferences()
    }

    private fun setupPreferences() {
        val keyToolbarTop = getPreferenceKey(R.string.pref_key_toolbar_top)
        val topPreference = requireNotNull(findPreference<RadioButtonPreference>(keyToolbarTop))
        topPreference.onClickListener {
            requireContext().components.analytics.metrics.track(Event.ToolbarPositionChanged(
                Event.ToolbarPositionChanged.Position.TOP
            ))
        }

        val keyToolbarBottom = getPreferenceKey(R.string.pref_key_toolbar_bottom)
        val bottomPreference = requireNotNull(findPreference<RadioButtonPreference>(keyToolbarBottom))
        bottomPreference.onClickListener {
            requireContext().components.analytics.metrics.track(Event.ToolbarPositionChanged(
                Event.ToolbarPositionChanged.Position.BOTTOM
            ))
        }

        topPreference.addToRadioGroup(bottomPreference)
        bottomPreference.addToRadioGroup(topPreference)
    }
}
