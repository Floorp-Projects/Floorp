/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

package org.mozilla.fenix.settings

import android.os.Bundle
import androidx.appcompat.app.AppCompatActivity
import androidx.preference.CheckBoxPreference
import androidx.preference.Preference
import androidx.preference.PreferenceFragmentCompat
import androidx.preference.SwitchPreference
import org.mozilla.fenix.R
import org.mozilla.fenix.ext.getPreferenceKey
import org.mozilla.fenix.utils.Settings

class DeleteBrowsingDataOnQuitFragment : PreferenceFragmentCompat() {
    override fun onCreatePreferences(savedInstanceState: Bundle?, rootKey: String?) {
        setPreferencesFromResource(R.xml.delete_browsing_data_quit_preferences, rootKey)
    }

    @Suppress("ComplexMethod")
    override fun onResume() {
        super.onResume()
        activity?.title = getString(R.string.preferences_delete_browsing_data_on_quit)
        (activity as AppCompatActivity).supportActionBar?.show()

        val checkboxUpdater = object : SharedPreferenceUpdater() {
            override fun onPreferenceChange(preference: Preference, newValue: Any?): Boolean {
                super.onPreferenceChange(preference, newValue)
                if (!Settings.getInstance(preference.context).shouldDeleteAnyDataOnQuit()) {
                    findPreference<SwitchPreference>(
                        getPreferenceKey(R.string.pref_key_delete_browsing_data_on_quit)
                    )?.apply {
                        isChecked = false
                    }
                    Settings.getInstance(preference.context).preferences.edit().putBoolean(
                        getString(R.string.pref_key_delete_browsing_data_on_quit),
                        false
                    ).apply()
                }
                return true
            }
        }

        val switchUpdater = object : SharedPreferenceUpdater() {
            override fun onPreferenceChange(preference: Preference, newValue: Any?): Boolean {
                setAllCheckboxes(newValue as Boolean)
                return super.onPreferenceChange(preference, newValue)
            }
        }

        findPreference<CheckBoxPreference>(getPreferenceKey(R.string.pref_key_delete_open_tabs_on_quit))?.apply {
            onPreferenceChangeListener = checkboxUpdater
        }
        findPreference<CheckBoxPreference>(getPreferenceKey(R.string.pref_key_delete_browsing_history_on_quit))?.apply {
            onPreferenceChangeListener = checkboxUpdater
        }
        findPreference<CheckBoxPreference>(getPreferenceKey(R.string.pref_key_delete_caches_on_quit))?.apply {
            onPreferenceChangeListener = checkboxUpdater
        }
        findPreference<CheckBoxPreference>(getPreferenceKey(R.string.pref_key_delete_permissions_on_quit))?.apply {
            onPreferenceChangeListener = checkboxUpdater
        }
        findPreference<CheckBoxPreference>(getPreferenceKey(R.string.pref_key_delete_cookies_on_quit))?.apply {
            onPreferenceChangeListener = checkboxUpdater
        }

        // Delete Browsing Data on Quit Switch
        val deleteOnQuitKey = getPreferenceKey(R.string.pref_key_delete_browsing_data_on_quit)
        findPreference<SwitchPreference>(deleteOnQuitKey)?.apply {
            onPreferenceChangeListener = switchUpdater
            isChecked = Settings.getInstance(context!!).shouldDeleteBrowsingDataOnQuit
        }
    }

    private fun setAllCheckboxes(newValue: Boolean) {
        val openTabs =
            findPreference<CheckBoxPreference>(getPreferenceKey(R.string.pref_key_delete_open_tabs_on_quit))
        val history =
            findPreference<CheckBoxPreference>(getPreferenceKey(R.string.pref_key_delete_browsing_history_on_quit))
        val cache =
            findPreference<CheckBoxPreference>(getPreferenceKey(R.string.pref_key_delete_caches_on_quit))
        val permissions =
            findPreference<CheckBoxPreference>(getPreferenceKey(R.string.pref_key_delete_permissions_on_quit))
        val cookies =
            findPreference<CheckBoxPreference>(getPreferenceKey(R.string.pref_key_delete_cookies_on_quit))

        openTabs?.isChecked = newValue
        history?.isChecked = newValue
        cache?.isChecked = newValue
        permissions?.isChecked = newValue
        cookies?.isChecked = newValue

        Settings.getInstance(context!!).preferences.edit().putBoolean(openTabs?.key, newValue)
            .apply()
        Settings.getInstance(context!!).preferences.edit().putBoolean(history?.key, newValue)
            .apply()
        Settings.getInstance(context!!).preferences.edit().putBoolean(cache?.key, newValue).apply()
        Settings.getInstance(context!!).preferences.edit().putBoolean(permissions?.key, newValue)
            .apply()
        Settings.getInstance(context!!).preferences.edit().putBoolean(cookies?.key, newValue)
            .apply()
    }
}
