/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

package org.mozilla.fenix.settings.search

import android.os.Bundle
import androidx.appcompat.app.AppCompatActivity
import androidx.preference.PreferenceFragmentCompat
import androidx.preference.SwitchPreference
import org.mozilla.fenix.R
import org.mozilla.fenix.ext.getPreferenceKey
import org.mozilla.fenix.ext.settings
import org.mozilla.fenix.settings.SharedPreferenceUpdater

class SearchEngineFragment : PreferenceFragmentCompat() {

    override fun onCreatePreferences(savedInstanceState: Bundle?, rootKey: String?) {
        setPreferencesFromResource(R.xml.search_preferences, rootKey)
    }

    override fun onResume() {
        super.onResume()
        (activity as AppCompatActivity).title = getString(R.string.preferences_search)
        (activity as AppCompatActivity).supportActionBar?.show()

        val searchSuggestionsPreference =
            findPreference<SwitchPreference>(getPreferenceKey(R.string.pref_key_show_search_suggestions))?.apply {
                isChecked = context.settings().shouldShowSearchSuggestions
            }

        val showSearchShortcuts =
            findPreference<SwitchPreference>(getPreferenceKey(R.string.pref_key_show_search_shortcuts))?.apply {
                isChecked = context.settings().shouldShowSearchShortcuts
            }

        val showHistorySuggestions =
            findPreference<SwitchPreference>(getPreferenceKey(R.string.pref_key_search_browsing_history))?.apply {
                isChecked = context.settings().shouldShowHistorySuggestions
            }

        val showBookmarkSuggestions =
            findPreference<SwitchPreference>(getPreferenceKey(R.string.pref_key_search_bookmarks))?.apply {
                isChecked = context.settings().shouldShowBookmarkSuggestions
            }

        val showClipboardSuggestions =
            findPreference<SwitchPreference>(getPreferenceKey(R.string.pref_key_show_clipboard_suggestions))?.apply {
                isChecked = context.settings().shouldShowClipboardSuggestions
            }

        searchSuggestionsPreference?.onPreferenceChangeListener = SharedPreferenceUpdater()
        showSearchShortcuts?.onPreferenceChangeListener = SharedPreferenceUpdater()
        showHistorySuggestions?.onPreferenceChangeListener = SharedPreferenceUpdater()
        showBookmarkSuggestions?.onPreferenceChangeListener = SharedPreferenceUpdater()
        showClipboardSuggestions?.onPreferenceChangeListener = SharedPreferenceUpdater()
    }
}
