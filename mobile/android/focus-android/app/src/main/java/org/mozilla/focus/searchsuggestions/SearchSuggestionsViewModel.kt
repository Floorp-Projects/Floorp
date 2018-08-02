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


class SearchSuggestionsViewModel(
        private val service: SearchSuggestionsService,
        private val searchSuggestionsPreferences: SearchSuggestionsPreferences) : ViewModel() {
    private val _selectedSearchSuggestion = MutableLiveData<String>()
    private val _searchQuery = MutableLiveData<String>()
    private val _promptUserToEnableSearchSuggestions = MutableLiveData<Boolean>()

    val canRequestSearchSuggestions = service.canProvideSearchSuggestions

    val selectedSearchSuggestion: LiveData<String>
        get() = _selectedSearchSuggestion

    val searchQuery: LiveData<String>
        get() = _searchQuery

    val promptUserToEnableSearchSuggestions: LiveData<Boolean>
        get() = _promptUserToEnableSearchSuggestions

    val suggestions = switchMap(searchQuery) {
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

    class Factory(private val context: Context) : ViewModelProvider.NewInstanceFactory() {
        override fun <T : ViewModel?> create(modelClass: Class<T>): T {
            val preferences = SearchSuggestionsPreferences(context)
            val service = SearchSuggestionsService(preferences.searchEngine.value!!)

            @Suppress("UNCHECKED_CAST")
            return SearchSuggestionsViewModel(service, preferences) as T
        }
    }
}

