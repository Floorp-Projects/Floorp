package org.mozilla.focus.searchsuggestions

import android.arch.lifecycle.LiveData
import android.arch.lifecycle.MutableLiveData
import android.content.Context
import android.preference.PreferenceManager
import mozilla.components.browser.search.SearchEngine
import org.mozilla.focus.Components
import org.mozilla.focus.utils.Settings

class SearchSuggestionsPreferences(private val context: Context) {
    private val settings = Settings.getInstance(context)
    private val preferences = PreferenceManager.getDefaultSharedPreferences(context)

    private val _searchSuggestionsEnabled = MutableLiveData<Boolean>()
    val searchSuggestionsEnabled: LiveData<Boolean> = _searchSuggestionsEnabled

    init {
        refresh()
    }

    fun hasUserToggledSearchSuggestions(): Boolean = settings.userHasToggledSearchSuggestions()

    fun getSearchEngine(): SearchEngine {
        return Components.searchEngineManager.getDefaultSearchEngine(
                context, settings.defaultSearchEngineName)
    }

    fun refresh() {
        _searchSuggestionsEnabled.value = settings.shouldShowSearchSuggestions()
    }
}
