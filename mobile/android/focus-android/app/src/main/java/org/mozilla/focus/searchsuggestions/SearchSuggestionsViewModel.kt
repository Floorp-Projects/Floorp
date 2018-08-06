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
import android.util.Log

sealed class State {
    data class Disabled(val givePrompt: Boolean): State()
    class NoSuggestionsAPI: State()
    class ReadyForSuggestions: State()
}
class SearchSuggestionsViewModel(
        private val service: SearchSuggestionsService,
        private val searchSuggestionsPreferences: SearchSuggestionsPreferences) : ViewModel() {
    private val _selectedSearchSuggestion = MutableLiveData<String>()
    private val _searchQuery = MutableLiveData<String>()

    val selectedSearchSuggestion: LiveData<String> = _selectedSearchSuggestion
    val searchQuery: LiveData<String> = _searchQuery

    val state: LiveData<State> = map(searchSuggestionsPreferences.searchSuggestionsEnabled) { enabled ->
        return@map if (enabled) {
            if (service.canProvideSearchSuggestions) {
                State.ReadyForSuggestions()
            } else {
                State.NoSuggestionsAPI()
            }
        } else {
            val givePrompt = !searchSuggestionsPreferences.hasUserToggledSearchSuggestions()
            State.Disabled(givePrompt)
        }
    }

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
    }

    fun disableSearchSuggestions() {
        searchSuggestionsPreferences.disableSearchSuggestions()
    }

    fun refresh() {
        service.updateSearchEngine(searchSuggestionsPreferences.getSearchEngine())
        searchSuggestionsPreferences.refresh()
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

