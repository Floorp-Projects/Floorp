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
    private val service: SearchSuggestionsService,
    private val searchSuggestionsPreferences: SearchSuggestionsPreferences
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

        val data = service.getSuggestions(it)

        map(data) {
            val query = it.first
            val suggestions = it.second

            val style = StyleSpan(Typeface.BOLD)
            val endIndex = query.length

            suggestions.map {
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
        searchSuggestionsPreferences.enableSearchSuggestions()
        updateState()
    }

    fun disableSearchSuggestions() {
        searchSuggestionsPreferences.disableSearchSuggestions()
        updateState()
    }

    fun dismissNoSuggestionsMessage() {
        searchSuggestionsPreferences.dismissNoSuggestionsMessage()
        updateState()
    }

    fun refresh() {
        val engine = searchSuggestionsPreferences.getSearchEngine()
        service.updateSearchEngine(engine)
        _searchEngine.value = engine
        updateState()
    }

    private fun updateState() {
        val enabled = searchSuggestionsPreferences.searchSuggestionsEnabled()

        val state = if (enabled) {
            if (service.canProvideSearchSuggestions) {
                State.ReadyForSuggestions()
            } else {
                val givePrompt = !searchSuggestionsPreferences.userHasDismissedNoSuggestionsMessage()
                State.NoSuggestionsAPI(givePrompt)
            }
        } else {
            val givePrompt = !searchSuggestionsPreferences.hasUserToggledSearchSuggestions()
            State.Disabled(givePrompt)
        }

        _state.value = state
    }

    class Factory(private val context: Context) : ViewModelProvider.NewInstanceFactory() {
        override fun <T : ViewModel?> create(modelClass: Class<T>): T {
            val preferences = SearchSuggestionsPreferences(context)
            val service = SearchSuggestionsService(preferences.getSearchEngine())

            @Suppress("UNCHECKED_CAST")
            return SearchSuggestionsViewModel(service, preferences) as T
        }
    }
}
