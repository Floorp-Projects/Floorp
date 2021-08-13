/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

package org.mozilla.fenix.settings

import android.os.Bundle
import android.view.View
import androidx.preference.PreferenceCategory
import androidx.preference.PreferenceFragmentCompat
import org.mozilla.fenix.FeatureFlags
import org.mozilla.fenix.R
import org.mozilla.fenix.components.metrics.Event
import org.mozilla.fenix.components.metrics.Event.TabViewSettingChanged
import org.mozilla.fenix.components.metrics.Event.TabViewSettingChanged.Type
import org.mozilla.fenix.ext.components
import org.mozilla.fenix.ext.showToolbar
import org.mozilla.fenix.utils.view.addToRadioGroup

/**
 * Lets the user customize auto closing tabs.
 */
class TabsSettingsFragment : PreferenceFragmentCompat() {
    private lateinit var listRadioButton: RadioButtonPreference
    private lateinit var gridRadioButton: RadioButtonPreference
    private lateinit var radioManual: RadioButtonPreference
    private lateinit var radioOneDay: RadioButtonPreference
    private lateinit var radioOneWeek: RadioButtonPreference
    private lateinit var radioOneMonth: RadioButtonPreference
    private lateinit var startOnHomeRadioFourHours: RadioButtonPreference
    private lateinit var startOnHomeRadioAlways: RadioButtonPreference
    private lateinit var startOnHomeRadioNever: RadioButtonPreference

    override fun onCreatePreferences(savedInstanceState: Bundle?, rootKey: String?) {
        setPreferencesFromResource(R.xml.tabs_preferences, rootKey)
    }

    override fun onViewCreated(view: View, savedInstanceState: Bundle?) {
        super.onViewCreated(view, savedInstanceState)
        view.context.components.analytics.metrics.track(Event.TabSettingsOpened)
    }

    override fun onResume() {
        super.onResume()
        showToolbar(getString(R.string.preferences_tabs))
        setupPreferences()
    }

    private fun setupPreferences() {
        // This should be the only use case for pref_key_tab_view_list_do_not_use.
        // In the Fenix impl of RadioGroups, we need to always have an individual pref value
        // for it to work. This is weird for a radio group that should hold a value from that group.
        // For the tabs tray, we only need a boolean value, so let's rely on only the
        // pref_key_tab_view_grid and look into using the native RadioGroup in the future.
        listRadioButton = requirePreference(R.string.pref_key_tab_view_list_do_not_use)
        gridRadioButton = requirePreference(R.string.pref_key_tab_view_grid)

        radioManual = requirePreference(R.string.pref_key_close_tabs_manually)
        radioOneDay = requirePreference(R.string.pref_key_close_tabs_after_one_day)
        radioOneWeek = requirePreference(R.string.pref_key_close_tabs_after_one_week)
        radioOneMonth = requirePreference(R.string.pref_key_close_tabs_after_one_month)

        startOnHomeRadioFourHours = requirePreference(R.string.pref_key_start_on_home_after_four_hours)
        startOnHomeRadioAlways = requirePreference(R.string.pref_key_start_on_home_always)
        startOnHomeRadioNever = requirePreference(R.string.pref_key_start_on_home_never)

        requirePreference<PreferenceCategory>(R.string.pref_key_start_on_home_category).isVisible =
            FeatureFlags.showStartOnHomeSettings

        listRadioButton.onClickListener(::sendTabViewTelemetry)
        gridRadioButton.onClickListener(::sendTabViewTelemetry)

        setupRadioGroups()
    }

    private fun setupRadioGroups() {
        addToRadioGroup(
            listRadioButton,
            gridRadioButton
        )

        addToRadioGroup(
            radioManual,
            radioOneDay,
            radioOneMonth,
            radioOneWeek
        )

        addToRadioGroup(
            startOnHomeRadioFourHours,
            startOnHomeRadioAlways,
            startOnHomeRadioNever
        )
    }

    private fun sendTabViewTelemetry() {
        val metrics = requireContext().components.analytics.metrics

        if (listRadioButton.isChecked && !gridRadioButton.isChecked) {
            metrics.track(TabViewSettingChanged(Type.LIST))
        } else {
            metrics.track(TabViewSettingChanged(Type.GRID))
        }
    }
}
