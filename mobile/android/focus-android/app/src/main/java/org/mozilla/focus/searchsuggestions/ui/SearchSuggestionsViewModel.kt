package org.mozilla.focus.searchsuggestions.ui

import android.arch.lifecycle.LiveData
import android.arch.lifecycle.MutableLiveData
import android.arch.lifecycle.ViewModel

class SearchSuggestionsViewModel : ViewModel() {
    private val _selectedSearchSuggestion = MutableLiveData<String>()
    val selectedSearchSuggestion: LiveData<String>
        get() = _selectedSearchSuggestion

    private val _searchQuery = MutableLiveData<String>()
    val searchQuery: LiveData<String>
        get() = _searchQuery

    fun selectSearchSuggestion(suggestion: String) {
        _selectedSearchSuggestion.value = suggestion
    }

    fun search(query: String) {
        _searchQuery.value = query
    }
}