/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

package org.mozilla.fenix.settings

import android.os.Bundle
import androidx.preference.PreferenceFragmentCompat
import org.mozilla.fenix.R
import org.mozilla.fenix.ext.showToolbar
import org.mozilla.fenix.utils.view.addToRadioGroup

/**
 * Lets the user customize auto closing tabs.
 */
class CloseTabsSettingsFragment : PreferenceFragmentCompat() {
    private lateinit var radioManual: RadioButtonPreference
    private lateinit var radioOneDay: RadioButtonPreference
    private lateinit var radioOneWeek: RadioButtonPreference
    private lateinit var radioOneMonth: RadioButtonPreference

    override fun onCreatePreferences(savedInstanceState: Bundle?, rootKey: String?) {
        setPreferencesFromResource(R.xml.close_tabs_preferences, rootKey)
    }

    override fun onResume() {
        super.onResume()
        showToolbar(getString(R.string.preferences_close_tabs))
        setupPreferences()
    }

    private fun setupPreferences() {
        radioManual = requirePreference(R.string.pref_key_close_tabs_manually)
        radioOneDay = requirePreference(R.string.pref_key_close_tabs_after_one_day)
        radioOneWeek = requirePreference(R.string.pref_key_close_tabs_after_one_week)
        radioOneMonth = requirePreference(R.string.pref_key_close_tabs_after_one_month)
        setupRadioGroups()
    }

    private fun setupRadioGroups() {
        addToRadioGroup(
            radioManual,
            radioOneDay,
            radioOneMonth,
            radioOneWeek
        )
    }
}
