package org.mozilla.focus.searchsuggestions

import android.arch.lifecycle.LiveData
import android.arch.lifecycle.MutableLiveData
import android.arch.lifecycle.Transformations.map
import android.content.Context
import android.preference.PreferenceManager
import mozilla.components.browser.search.SearchEngine
import org.mozilla.focus.Components
import org.mozilla.focus.utils.Settings

class SearchSuggestionsPreferences(private val context: Context) {
    private val settings = Settings.getInstance(context)
    private val preferences = PreferenceManager.getDefaultSharedPreferences(context)

    private val _searchEngineSupportsSuggestions = MutableLiveData<Boolean>()

    private val _userHasRespondedToSearchSuggestionPrompt = MutableLiveData<Boolean>()
    val userHasRespondedToSearchSuggestionPrompt: LiveData<Boolean>
        get() = _userHasRespondedToSearchSuggestionPrompt

    private val _shouldShowSearchSuggestions = MutableLiveData<Boolean>()
    val shouldShowSearchSuggestions: LiveData<Boolean>
        get() = _shouldShowSearchSuggestions

    private val _searchEngine = MutableLiveData<SearchEngine>()
    val searchEngine: LiveData<SearchEngine>
        get() = _searchEngine

    val searchEngineSupportsSuggestions = map(searchEngine) { it.canProvideSearchSuggestions }

    init {
        _shouldShowSearchSuggestions.value = settings.shouldShowSearchSuggestions()
        _userHasRespondedToSearchSuggestionPrompt.value = false
        _searchEngine.value = getSearchEngine()
    }


    private fun getSearchEngine(): SearchEngine {
        val searchEngine = Components.searchEngineManager.getDefaultSearchEngine(
                context, settings.defaultSearchEngineName)

        return searchEngine
    }
}
