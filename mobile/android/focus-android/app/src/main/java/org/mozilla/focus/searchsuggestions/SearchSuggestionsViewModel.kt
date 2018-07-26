package org.mozilla.focus.searchsuggestions

import android.app.Application
import android.arch.lifecycle.ViewModelProvider
import android.arch.lifecycle.MutableLiveData
import android.arch.lifecycle.LiveData
import android.arch.lifecycle.ViewModel
import android.content.Context
import android.util.Log

class SearchSuggestionsViewModelFactory(application: Application): ViewModelProvider.AndroidViewModelFactory(application) {

}


class SearchSuggestionsViewModel(private val repository: SearchSuggestionsRepository) : ViewModel() {
    private val _selectedSearchSuggestion = MutableLiveData<String>()
    val selectedSearchSuggestion: LiveData<String>
        get() = _selectedSearchSuggestion

    val suggestions = repository.suggestions

    private val _searchQuery = MutableLiveData<String>()
    val searchQuery: LiveData<String>
        get() = _searchQuery

    fun selectSearchSuggestion(suggestion: String) {
        _selectedSearchSuggestion.value = suggestion
    }

    fun setSearchQuery(query: String) {
        _searchQuery.value = query
    }

    fun fetchSuggestions(query: String) {
        repository.getSuggestions(query)
    }

    class Factory(private val context: Context) : ViewModelProvider.NewInstanceFactory() {
        override fun <T : ViewModel?> create(modelClass: Class<T>): T {

            Log.e("WAT", "FOOBAR")
            val repository = SearchSuggestionsRepository(context)

            @Suppress("UNCHECKED_CAST")
            return SearchSuggestionsViewModel(repository) as T
        }
    }
}

