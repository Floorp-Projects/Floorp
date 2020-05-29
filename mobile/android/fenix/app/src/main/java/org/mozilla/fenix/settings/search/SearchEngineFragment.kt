/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

package org.mozilla.fenix.settings.search

import android.os.Bundle
import androidx.navigation.fragment.findNavController
import androidx.preference.CheckBoxPreference
import androidx.preference.Preference
import androidx.preference.PreferenceFragmentCompat
import androidx.preference.SwitchPreference
import org.mozilla.fenix.R
import org.mozilla.fenix.ext.getPreferenceKey
import org.mozilla.fenix.ext.settings
import org.mozilla.fenix.ext.showToolbar
import org.mozilla.fenix.settings.SharedPreferenceUpdater

class SearchEngineFragment : PreferenceFragmentCompat() {

    override fun onCreatePreferences(savedInstanceState: Bundle?, rootKey: String?) {
        setPreferencesFromResource(R.xml.search_preferences, rootKey)
    }

    override fun onResume() {
        super.onResume()
        showToolbar(getString(R.string.preferences_search))

        val searchSuggestionsPreference =
            findPreference<SwitchPreference>(getPreferenceKey(R.string.pref_key_show_search_suggestions))?.apply {
                isChecked = context.settings().shouldShowSearchSuggestions
            }

        val searchSuggestionsInPrivatePreference =
            findPreference<CheckBoxPreference>(getPreferenceKey(R.string.pref_key_show_search_suggestions_in_private))
                ?.apply {
                isChecked = context.settings().shouldShowSearchSuggestionsInPrivate
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

        val searchEngineListPreference =
            findPreference<SearchEngineListPreference>(getPreferenceKey(R.string.pref_key_search_engine_list))

        val showVoiceSearchPreference =
            findPreference<SwitchPreference>(getPreferenceKey(R.string.pref_key_show_voice_search))?.apply {
                isChecked = context.settings().shouldShowVoiceSearch
            }

        searchEngineListPreference?.reload(requireContext())
        searchSuggestionsPreference?.onPreferenceChangeListener = SharedPreferenceUpdater()
        showSearchShortcuts?.onPreferenceChangeListener = SharedPreferenceUpdater()
        showHistorySuggestions?.onPreferenceChangeListener = SharedPreferenceUpdater()
        showBookmarkSuggestions?.onPreferenceChangeListener = SharedPreferenceUpdater()
        showClipboardSuggestions?.onPreferenceChangeListener = SharedPreferenceUpdater()
        searchSuggestionsInPrivatePreference?.onPreferenceChangeListener = SharedPreferenceUpdater()
        showVoiceSearchPreference?.onPreferenceChangeListener = SharedPreferenceUpdater()

        searchSuggestionsPreference?.setOnPreferenceClickListener {
            if (!searchSuggestionsPreference.isChecked) {
                searchSuggestionsInPrivatePreference?.isChecked = false
                searchSuggestionsInPrivatePreference?.callChangeListener(false)
            }
            true
        }
    }

    override fun onPreferenceTreeClick(preference: Preference): Boolean {
        when (preference.key) {
            getPreferenceKey(R.string.pref_key_add_search_engine) -> {
                val directions = SearchEngineFragmentDirections
                    .actionSearchEngineFragmentToAddSearchEngineFragment()
                findNavController().navigate(directions)
            }
        }

        return super.onPreferenceTreeClick(preference)
    }
}
