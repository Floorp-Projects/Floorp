/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

package org.mozilla.fenix.settings

import android.os.Bundle
import androidx.appcompat.app.AppCompatActivity
import androidx.preference.PreferenceFragmentCompat
import androidx.preference.SwitchPreference
import org.mozilla.fenix.R
import org.mozilla.fenix.ext.getPreferenceKey
import org.mozilla.fenix.ext.settings

class SearchEngineFragment : PreferenceFragmentCompat() {

    override fun onCreatePreferences(savedInstanceState: Bundle?, rootKey: String?) {
        setPreferencesFromResource(R.xml.search_engine_preferences, rootKey)
    }

    override fun onResume() {
        super.onResume()
        (activity as AppCompatActivity).title = getString(R.string.preferences_search_engine)
        (activity as AppCompatActivity).supportActionBar?.show()

        val searchSuggestionsPreference =
            findPreference<SwitchPreference>(getPreferenceKey(R.string.pref_key_show_search_suggestions))?.apply {
                isChecked = context.settings.shouldShowSearchSuggestions
            }

        searchSuggestionsPreference?.onPreferenceChangeListener = SharedPreferenceUpdater()

        val showHistorySuggestions =
            findPreference<SwitchPreference>(getPreferenceKey(R.string.pref_key_search_browsing_history))?.apply {
                isChecked = context.settings.shouldShowHistorySuggestions
            }

        val showBookmarkSuggestions =
            findPreference<SwitchPreference>(getPreferenceKey(R.string.pref_key_search_bookmarks))?.apply {
                isChecked = context.settings.shouldShowBookmarkSuggestions
            }

        val showClipboardSuggestions =
            findPreference<SwitchPreference>(getPreferenceKey(R.string.pref_key_show_clipboard_suggestions))?.apply {
                isChecked = context.settings.shouldShowClipboardSuggestions
            }

        showHistorySuggestions?.onPreferenceChangeListener = SharedPreferenceUpdater()
        showBookmarkSuggestions?.onPreferenceChangeListener = SharedPreferenceUpdater()
        showClipboardSuggestions?.onPreferenceChangeListener = SharedPreferenceUpdater()
    }
}
