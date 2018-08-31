/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

package org.mozilla.focus.searchsuggestions

import android.app.Application
import android.arch.lifecycle.AndroidViewModel
import android.arch.lifecycle.LiveData
import android.arch.lifecycle.MutableLiveData
import android.arch.lifecycle.Transformations.map
import android.graphics.Typeface
import android.text.Spannable
import android.text.SpannableStringBuilder
import android.text.style.StyleSpan

sealed class State {
    data class Disabled(val givePrompt: Boolean) : State()
    data class NoSuggestionsAPI(val givePrompt: Boolean) : State()
    object ReadyForSuggestions : State()
}

class SearchSuggestionsViewModel(application: Application) : AndroidViewModel(application) {
    private val fetcher: SearchSuggestionsFetcher
    private val preferences: SearchSuggestionsPreferences = SearchSuggestionsPreferences(application)

    private val _selectedSearchSuggestion = MutableLiveData<String>()
    val selectedSearchSuggestion: LiveData<String> = _selectedSearchSuggestion

    private val _searchQuery = MutableLiveData<String>()
    val searchQuery: LiveData<String> = _searchQuery

    private val _state = MutableLiveData<State>()
    val state: LiveData<State> = _state

    val suggestions: LiveData<List<SpannableStringBuilder>>
    var alwaysSearch = false
        private set

    init {
        fetcher = SearchSuggestionsFetcher(preferences.getSearchEngine())

        suggestions = map(fetcher.results) { result ->
            val style = StyleSpan(Typeface.BOLD)
            val endIndex = result.query.length

            result.suggestions.map { suggestion ->
                SpannableStringBuilder(suggestion).apply {
                    setSpan(style, 0, minOf(endIndex, suggestion.length), Spannable.SPAN_EXCLUSIVE_EXCLUSIVE)
                }
            }
        }
    }

    fun selectSearchSuggestion(suggestion: String, alwaysSearch: Boolean = false) {
        this.alwaysSearch = alwaysSearch
        _selectedSearchSuggestion.postValue(suggestion)
    }

    fun setSearchQuery(query: String) {
        _searchQuery.value = query

        if (state.value is State.ReadyForSuggestions) {
            fetcher.requestSuggestions(query)
        }
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
        updateState()
    }

    private fun updateState() {
        val enabled = preferences.searchSuggestionsEnabled()

        val state = if (enabled) {
            if (fetcher.canProvideSearchSuggestions) {
                State.ReadyForSuggestions
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
}
