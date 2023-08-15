/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

package org.mozilla.fenix.settings.search

import android.os.Bundle
import androidx.core.content.edit
import androidx.navigation.fragment.findNavController
import androidx.preference.CheckBoxPreference
import androidx.preference.Preference
import androidx.preference.PreferenceFragmentCompat
import androidx.preference.SwitchPreference
import mozilla.components.browser.state.state.selectedOrDefaultSearchEngine
import mozilla.components.support.ktx.android.view.hideKeyboard
import org.mozilla.fenix.R
import org.mozilla.fenix.ext.components
import org.mozilla.fenix.ext.getPreferenceKey
import org.mozilla.fenix.ext.navigateWithBreadcrumb
import org.mozilla.fenix.ext.settings
import org.mozilla.fenix.ext.showToolbar
import org.mozilla.fenix.settings.SharedPreferenceUpdater
import org.mozilla.fenix.settings.requirePreference
import org.mozilla.gecko.search.SearchWidgetProvider

class SearchEngineFragment : PreferenceFragmentCompat() {

    private var unifiedSearchUI = false

    override fun onCreatePreferences(savedInstanceState: Bundle?, rootKey: String?) {
        unifiedSearchUI = requireContext().settings().enableUnifiedSearchSettingsUI
        setPreferencesFromResource(
            if (unifiedSearchUI) R.xml.search_settings_preferences else R.xml.search_preferences,
            rootKey,
        )

        // Visibility should be set before the view has been created, to avoid visual glitches.
        requirePreference<SwitchPreference>(R.string.pref_key_show_search_engine_shortcuts).apply {
            isVisible = !context.settings().showUnifiedSearchFeature
        }

        view?.hideKeyboard()
    }

    override fun onResume() {
        super.onResume()
        view?.hideKeyboard()
        showToolbar(getString(R.string.preferences_search))

        if (unifiedSearchUI) {
            with(requirePreference<Preference>(R.string.pref_key_default_search_engine)) {
                summary =
                    requireContext().components.core.store.state.search.selectedOrDefaultSearchEngine?.name
            }
        }

        val searchSuggestionsPreference =
            requirePreference<SwitchPreference>(R.string.pref_key_show_search_suggestions).apply {
                isChecked = context.settings().shouldShowSearchSuggestions
            }

        val autocompleteURLsPreference =
            requirePreference<SwitchPreference>(R.string.pref_key_enable_autocomplete_urls).apply {
                isChecked = context.settings().shouldAutocompleteInAwesomebar
            }

        val searchSuggestionsInPrivatePreference =
            requirePreference<CheckBoxPreference>(R.string.pref_key_show_search_suggestions_in_private).apply {
                isChecked = context.settings().shouldShowSearchSuggestionsInPrivate
            }

        val showSearchShortcuts =
            requirePreference<SwitchPreference>(R.string.pref_key_show_search_engine_shortcuts).apply {
                isChecked = context.settings().shouldShowSearchShortcuts
            }

        val showHistorySuggestions =
            requirePreference<SwitchPreference>(R.string.pref_key_search_browsing_history).apply {
                isChecked = context.settings().shouldShowHistorySuggestions
            }

        val showBookmarkSuggestions =
            requirePreference<SwitchPreference>(R.string.pref_key_search_bookmarks).apply {
                isChecked = context.settings().shouldShowBookmarkSuggestions
            }

        val showSyncedTabsSuggestions =
            requirePreference<SwitchPreference>(R.string.pref_key_search_synced_tabs).apply {
                isChecked = context.settings().shouldShowSyncedTabsSuggestions
            }

        val showClipboardSuggestions =
            requirePreference<SwitchPreference>(R.string.pref_key_show_clipboard_suggestions).apply {
                isChecked = context.settings().shouldShowClipboardSuggestions
            }

        val showVoiceSearchPreference =
            requirePreference<SwitchPreference>(R.string.pref_key_show_voice_search).apply {
                isChecked = context.settings().shouldShowVoiceSearch
            }

        searchSuggestionsPreference.onPreferenceChangeListener = SharedPreferenceUpdater()
        showSearchShortcuts.onPreferenceChangeListener = SharedPreferenceUpdater()
        showHistorySuggestions.onPreferenceChangeListener = SharedPreferenceUpdater()
        showBookmarkSuggestions.onPreferenceChangeListener = SharedPreferenceUpdater()
        showSyncedTabsSuggestions.onPreferenceChangeListener = SharedPreferenceUpdater()
        showClipboardSuggestions.onPreferenceChangeListener = SharedPreferenceUpdater()
        searchSuggestionsInPrivatePreference.onPreferenceChangeListener = SharedPreferenceUpdater()
        showVoiceSearchPreference.onPreferenceChangeListener = object : Preference.OnPreferenceChangeListener {
            override fun onPreferenceChange(preference: Preference, newValue: Any?): Boolean {
                val newBooleanValue = newValue as? Boolean ?: return false
                requireContext().settings().preferences.edit {
                    putBoolean(preference.key, newBooleanValue)
                }
                SearchWidgetProvider.updateAllWidgets(requireContext())
                return true
            }
        }
        autocompleteURLsPreference.onPreferenceChangeListener = SharedPreferenceUpdater()

        searchSuggestionsPreference.setOnPreferenceClickListener {
            if (!searchSuggestionsPreference.isChecked) {
                searchSuggestionsInPrivatePreference.isChecked = false
                searchSuggestionsInPrivatePreference.callChangeListener(false)
            }
            true
        }
    }

    override fun onPreferenceTreeClick(preference: Preference): Boolean {
        when (preference.key) {
            getPreferenceKey(R.string.pref_key_add_search_engine) -> {
                val directions = SearchEngineFragmentDirections
                    .actionSearchEngineFragmentToAddSearchEngineFragment()
                context?.let {
                    findNavController().navigateWithBreadcrumb(
                        directions = directions,
                        navigateFrom = "SearchEngineFragment",
                        navigateTo = "ActionSearchEngineFragmentToAddSearchEngineFragment",
                        it.components.analytics.crashReporter,
                    )
                }
            }
            getPreferenceKey(R.string.pref_key_default_search_engine) -> {
                val directions = SearchEngineFragmentDirections
                    .actionSearchEngineFragmentToDefaultEngineFragment()
                findNavController().navigate(directions)
            }
            getPreferenceKey(R.string.pref_key_manage_search_shortcuts) -> {
                val directions = SearchEngineFragmentDirections
                    .actionSearchEngineFragmentToSearchShortcutsFragment()
                context?.let {
                    findNavController().navigateWithBreadcrumb(
                        directions = directions,
                        navigateFrom = "SearchEngineFragment",
                        navigateTo = "ActionSearchEngineFragmentToSearchShortcutsFragment",
                        it.components.analytics.crashReporter,
                    )
                }
            }
        }

        return super.onPreferenceTreeClick(preference)
    }
}
