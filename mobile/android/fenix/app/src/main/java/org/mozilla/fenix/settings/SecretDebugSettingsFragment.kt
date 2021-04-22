/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

package org.mozilla.fenix.settings

import android.os.Bundle
import androidx.preference.Preference
import androidx.preference.PreferenceFragmentCompat
import org.mozilla.fenix.R
import org.mozilla.fenix.ext.requireComponents
import org.mozilla.fenix.ext.showToolbar

class SecretDebugSettingsFragment : PreferenceFragmentCompat() {

    override fun onResume() {
        super.onResume()
        showToolbar(getString(R.string.preferences_debug_info))
    }

    override fun onCreatePreferences(savedInstanceState: Bundle?, rootKey: String?) {
        setPreferencesFromResource(R.xml.secret_info_settings_preferences, rootKey)

        val store = requireComponents.core.store

        requirePreference<Preference>(R.string.pref_key_search_region_home).apply {
            summary = store.state.search.region?.home ?: "Unknown"
        }

        requirePreference<Preference>(R.string.pref_key_search_region_current).apply {
            summary = store.state.search.region?.current ?: "Unknown"
        }
    }
}
