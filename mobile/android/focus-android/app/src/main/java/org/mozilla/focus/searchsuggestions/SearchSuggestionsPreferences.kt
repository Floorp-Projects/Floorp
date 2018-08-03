package org.mozilla.focus.searchsuggestions

import android.content.Context
import android.preference.PreferenceManager
import mozilla.components.browser.search.SearchEngine
import org.mozilla.focus.Components
import org.mozilla.focus.utils.Settings

class SearchSuggestionsPreferences(private val context: Context) {
    private val settings = Settings.getInstance(context)
    private val preferences = PreferenceManager.getDefaultSharedPreferences(context)

    fun searchSuggestionsEnabled(): Boolean = settings.shouldShowSearchSuggestions()

    fun getSearchEngine(): SearchEngine {
        return Components.searchEngineManager.getDefaultSearchEngine(
                context, settings.defaultSearchEngineName)
    }
}
