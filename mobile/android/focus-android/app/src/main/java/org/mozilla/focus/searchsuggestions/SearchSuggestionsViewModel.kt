/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

package org.mozilla.focus.searchsuggestions

import android.app.Application
import androidx.lifecycle.AndroidViewModel
import androidx.lifecycle.LiveData
import androidx.lifecycle.MutableLiveData
import mozilla.components.browser.state.state.selectedOrDefaultSearchEngine
import mozilla.components.feature.search.ext.canProvideSearchSuggestions
import org.mozilla.focus.FocusApplication

sealed class State {
    data class Disabled(val givePrompt: Boolean) : State()
    data class NoSuggestionsAPI(val givePrompt: Boolean) : State()
    object ReadyForSuggestions : State()
}

class SearchSuggestionsViewModel(application: Application) : AndroidViewModel(application) {
    private val preferences: SearchSuggestionsPreferences = SearchSuggestionsPreferences(application)

    private val _selectedSearchSuggestion = MutableLiveData<String>()
    val selectedSearchSuggestion: LiveData<String> = _selectedSearchSuggestion

    private val _searchQuery = MutableLiveData<String>()
    val searchQuery: LiveData<String> = _searchQuery

    private val _state = MutableLiveData<State>()
    val state: LiveData<State> = _state

    private val _autocompleteSuggestion = MutableLiveData<String>()
    val autocompleteSuggestion: LiveData<String> = _autocompleteSuggestion

    var alwaysSearch = false
        private set

    fun selectSearchSuggestion(suggestion: String, alwaysSearch: Boolean = false) {
        this.alwaysSearch = alwaysSearch
        _selectedSearchSuggestion.postValue(suggestion)
    }

    fun clearSearchSuggestion() {
        _selectedSearchSuggestion.postValue(null)
    }

    fun setAutocompleteSuggestion(text: String) {
        _autocompleteSuggestion.postValue(text)
    }

    fun clearAutocompleteSuggestion() {
        _autocompleteSuggestion.postValue(null)
    }

    fun setSearchQuery(query: String) {
        _searchQuery.value = query
    }

    fun enableSearchSuggestions() {
        preferences.enableSearchSuggestions()
        updateState()
        setSearchQuery(searchQuery.value ?: "")
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
        updateState()
    }

    private fun updateState() {
        val enabled = preferences.searchSuggestionsEnabled()

        val store = getApplication<FocusApplication>().components.store

        val state = if (enabled) {
            if (store.state.search.selectedOrDefaultSearchEngine?.canProvideSearchSuggestions == true) {
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
