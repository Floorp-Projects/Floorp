/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

package org.mozilla.fenix.settings

import android.os.Bundle
import androidx.preference.PreferenceFragmentCompat
import androidx.preference.SwitchPreference
import org.mozilla.fenix.R
import org.mozilla.fenix.ext.getPreferenceKey
import org.mozilla.fenix.ext.settings
import org.mozilla.fenix.ext.showToolbar

/**
 * Lets the user customize Private browsing options.
 */
class SecretSettingsPreference : PreferenceFragmentCompat() {
    override fun onResume() {
        super.onResume()
        showToolbar(getString(R.string.preferences_debug_settings))
    }

    override fun onCreatePreferences(savedInstanceState: Bundle?, rootKey: String?) {
        setPreferencesFromResource(R.xml.secret_settings_preferences, rootKey)
        updatePreferences()
    }

    private fun updatePreferences() {
        findPreference<SwitchPreference>(getPreferenceKey(R.string.pref_key_enable_new_tab_tray))?.apply {
            onPreferenceChangeListener = SharedPreferenceUpdater()
            isChecked = context.settings().useNewTabTray
        }
    }
}
