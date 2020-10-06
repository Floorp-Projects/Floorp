/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

package org.mozilla.fenix.settings

import android.os.Bundle
import androidx.preference.PreferenceCategory
import androidx.preference.PreferenceFragmentCompat
import org.mozilla.fenix.R
import org.mozilla.fenix.ext.settings
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

    override fun onCreatePreferences(savedInstanceState: Bundle?, rootKey: String?) {
        setPreferencesFromResource(R.xml.tabs_preferences, rootKey)
    }

    override fun onResume() {
        super.onResume()
        showToolbar(getString(R.string.preferences_tabs))
        setupPreferences()
    }

    private fun setupPreferences() {
        listRadioButton = requirePreference(R.string.pref_key_tab_view_list)
        gridRadioButton = requirePreference(R.string.pref_key_tab_view_grid)

        radioManual = requirePreference(R.string.pref_key_close_tabs_manually)
        radioOneDay = requirePreference(R.string.pref_key_close_tabs_after_one_day)
        radioOneWeek = requirePreference(R.string.pref_key_close_tabs_after_one_week)
        radioOneMonth = requirePreference(R.string.pref_key_close_tabs_after_one_month)

        setupRadioGroups()

        requirePreference<PreferenceCategory>(R.string.pref_key_tab_view_category).apply {
            isVisible = context.settings().showGridViewInTabsSettings
        }
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
    }
}
