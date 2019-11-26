/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

package org.mozilla.fenix.settings

import android.os.Bundle
import androidx.appcompat.app.AppCompatActivity
import androidx.preference.PreferenceFragmentCompat
import org.mozilla.fenix.R
import org.mozilla.fenix.components.metrics.Event
import org.mozilla.fenix.ext.components
import org.mozilla.fenix.ext.getPreferenceKey

class ToolbarSettingsFragment : PreferenceFragmentCompat() {
    private lateinit var topPreference: RadioButtonPreference
    private lateinit var bottomPreference: RadioButtonPreference

    override fun onCreatePreferences(savedInstanceState: Bundle?, rootKey: String?) {
        setPreferencesFromResource(R.xml.toolbar_preferences, rootKey)
    }

    override fun onResume() {
        super.onResume()
        (activity as AppCompatActivity).title = getString(R.string.preferences_toolbar)
        (activity as AppCompatActivity).supportActionBar?.show()

        setupClickListeners()
        setupRadioGroups()
    }

    private fun setupClickListeners() {
        val keyToolbarTop = getPreferenceKey(R.string.pref_key_toolbar_top)
        topPreference = requireNotNull(findPreference(keyToolbarTop))
        topPreference.onClickListener {
            requireContext().components.analytics.metrics.track(Event.ToolbarPositionChanged(
                Event.ToolbarPositionChanged.Position.TOP
            ))
        }

        val keyToolbarBottom = getPreferenceKey(R.string.pref_key_toolbar_bottom)
        bottomPreference = requireNotNull(findPreference(keyToolbarBottom))
        bottomPreference.onClickListener {
            requireContext().components.analytics.metrics.track(Event.ToolbarPositionChanged(
                Event.ToolbarPositionChanged.Position.BOTTOM
            ))
        }
    }

    private fun setupRadioGroups() {
        topPreference.addToRadioGroup(bottomPreference)
        bottomPreference.addToRadioGroup(topPreference)
    }
}
