/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

package org.mozilla.focus.autocomplete

import android.os.Bundle
import android.preference.Preference
import android.preference.PreferenceFragment
import android.preference.PreferenceScreen
import org.mozilla.focus.R
import org.mozilla.focus.settings.SettingsFragment

/**
 * Settings UI for configuring autocomplete.
 */
class AutocompleteSettingsFragment : PreferenceFragment() {
    override fun onCreate(savedInstanceState: Bundle?) {
        super.onCreate(savedInstanceState)

        addPreferencesFromResource(R.xml.autocomplete)
    }

    override fun onResume() {
        super.onResume()

        val updater = activity as SettingsFragment.ActionBarUpdater
        updater.updateTitle(R.string.preference_subitem_autocomplete)
        updater.updateIcon(R.drawable.ic_back)
    }

    override fun onPreferenceTreeClick(preferenceScreen: PreferenceScreen?, preference: Preference?): Boolean {
        preference?.let {
            if (it.key == getString(R.string.pref_key_screen_custom_domains)) {
                fragmentManager.beginTransaction()
                        .replace(R.id.container, AutocompleteListFragment())
                        .addToBackStack(null)
                        .commit()
            }
        }

        return super.onPreferenceTreeClick(preferenceScreen, preference)
    }
}
