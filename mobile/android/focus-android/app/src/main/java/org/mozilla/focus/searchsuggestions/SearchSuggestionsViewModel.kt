/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

package org.mozilla.focus.searchsuggestions

import android.arch.lifecycle.LiveData
import android.arch.lifecycle.ViewModelProvider
import android.arch.lifecycle.MutableLiveData
import android.arch.lifecycle.Transformations.map
import android.arch.lifecycle.Transformations.switchMap
import android.arch.lifecycle.ViewModel
import android.content.Context
import android.graphics.Typeface
import android.text.Spannable
import android.text.SpannableStringBuilder
import android.text.style.StyleSpan
import mozilla.components.browser.search.SearchEngine

sealed class State {
    data class Disabled(val givePrompt: Boolean) : State()
    data class NoSuggestionsAPI(val givePrompt: Boolean) : State()
    class ReadyForSuggestions : State()
}

class SearchSuggestionsViewModel(
        private val fetcher: SearchSuggestionsFetcher,
        private val preferences: SearchSuggestionsPreferences
) : ViewModel() {
    private val _selectedSearchSuggestion = MutableLiveData<String>()
    val selectedSearchSuggestion: LiveData<String> = _selectedSearchSuggestion

    private val _searchQuery = MutableLiveData<String>()
    val searchQuery: LiveData<String> = _searchQuery

    private val _state = MutableLiveData<State>()
    val state: LiveData<State> = _state

    private val _searchEngine = MutableLiveData<SearchEngine>()
    val searchEngine: LiveData<SearchEngine> = _searchEngine

    val suggestions = switchMap(searchQuery) {
        if (state.value !is State.ReadyForSuggestions) { return@switchMap null }

        val data = fetcher.getSuggestions(it)

        map(data) {
            val style = StyleSpan(Typeface.BOLD)
            val endIndex = it.query.length

            it.suggestions.map {
                val ssb = SpannableStringBuilder(it)
                ssb.setSpan(style, 0, minOf(endIndex, it.length), Spannable.SPAN_EXCLUSIVE_EXCLUSIVE)

                ssb
            }
        }
    }!!

    fun selectSearchSuggestion(suggestion: String) {
        _selectedSearchSuggestion.value = suggestion
    }

    fun setSearchQuery(query: String) {
        _searchQuery.value = query
    }

    fun enableSearchSuggestions() {
        preferences.enableSearchSuggestions()
        updateState()
    }

    fun disableSearchSuggestions() {
        preferences.disableSearchSuggestions()
        updateState()
    }

    fun dismissNoSuggestionsMessage() {
        preferences.dismissNoSuggestionsMessage()
        updateState()
    }

    fun refresh() {
        val engine = preferences.getSearchEngine()
        fetcher.updateSearchEngine(engine)
        _searchEngine.value = engine
        updateState()
    }

    private fun updateState() {
        val enabled = preferences.searchSuggestionsEnabled()

        val state = if (enabled) {
            if (fetcher.canProvideSearchSuggestions) {
                State.ReadyForSuggestions()
            } else {
                val givePrompt = !preferences.userHasDismissedNoSuggestionsMessage()
                State.NoSuggestionsAPI(givePrompt)
            }
        } else {
            val givePrompt = !preferences.hasUserToggledSearchSuggestions()
            State.Disabled(givePrompt)
        }

        _state.value = state
    }

    class Factory(private val context: Context) : ViewModelProvider.NewInstanceFactory() {
        override fun <T : ViewModel?> create(modelClass: Class<T>): T {
            val preferences = SearchSuggestionsPreferences(context)
            val service = SearchSuggestionsFetcher(preferences.getSearchEngine())

            @Suppress("UNCHECKED_CAST")
            return SearchSuggestionsViewModel(service, preferences) as T
        }
    }
}
