/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

package org.mozilla.fenix.settings

import android.os.Bundle
import androidx.preference.PreferenceFragmentCompat
import androidx.preference.SwitchPreference
import org.mozilla.fenix.FeatureFlags
import org.mozilla.fenix.R
import org.mozilla.fenix.ext.settings
import org.mozilla.fenix.ext.showToolbar

/**
 * Lets the user customize Private browsing options.
 */
class SecretSettingsFragment : PreferenceFragmentCompat() {

    override fun onResume() {
        super.onResume()
        showToolbar(getString(R.string.preferences_debug_settings))
    }

    override fun onCreatePreferences(savedInstanceState: Bundle?, rootKey: String?) {
        setPreferencesFromResource(R.xml.secret_settings_preferences, rootKey)

        requirePreference<SwitchPreference>(R.string.pref_key_use_new_search_experience).apply {
            isVisible = FeatureFlags.newSearchExperience
            isChecked = context.settings().useNewSearchExperience
            onPreferenceChangeListener = SharedPreferenceUpdater()
        }

        requirePreference<SwitchPreference>(R.string.pref_key_wait_first_paint).apply {
            isVisible = FeatureFlags.waitUntilPaintToDraw
            isChecked = context.settings().waitToShowPageUntilFirstPaint
            onPreferenceChangeListener = SharedPreferenceUpdater()
        }

        requirePreference<SwitchPreference>(R.string.pref_key_synced_tabs_tabs_tray).apply {
            isVisible = FeatureFlags.syncedTabsInTabsTray
            isChecked = context.settings().syncedTabsInTabsTray
            onPreferenceChangeListener = SharedPreferenceUpdater()
        }
    }
}
