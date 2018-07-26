package org.mozilla.focus.searchsuggestions

import android.arch.lifecycle.LiveData
import android.arch.lifecycle.MutableLiveData
import android.content.Context
import kotlinx.coroutines.experimental.CommonPool
import kotlinx.coroutines.experimental.android.UI
import mozilla.components.browser.search.suggestions.SearchSuggestionClient
import org.mozilla.focus.Components
import org.mozilla.focus.utils.Settings
import kotlinx.coroutines.experimental.launch
import mozilla.components.browser.search.SearchEngine

class SearchSuggestionsRepository(private val context: Context) {

    private var searchEngine: SearchEngine? = null
    private var client: SearchSuggestionClient? = null

    private val _suggestions = MutableLiveData<List<String>>()
    val suggestions: LiveData<List<String>>
            get() = _suggestions

    private fun fetch(): String? {
        return "[\"firefox\",[\"firefox\",\"firefox for mac\",\"firefox quantum\",\"firefox update\",\"firefox esr\",\"firefox focus\",\"firefox addons\",\"firefox extensions\",\"firefox nightly\",\"firefox clear cache\"]]"
    }

    fun getSuggestions(query: String) {
        if (shouldUpdateclient()) { updateClient() }

        launch(CommonPool) {
            val suggestions = client?.getSuggestions(query) ?: listOf()

            launch(UI) {
                _suggestions.value = suggestions
            }
        }
    }

    private fun shouldUpdateclient(): Boolean {
        val defaultSearchEngineName= Settings.getInstance(context).defaultSearchEngineName
        return searchEngine?.let { it.name != defaultSearchEngineName } ?: true
    }

    private fun updateClient() {
        val defaultIdentifier = Settings.getInstance(context).defaultSearchEngineName

        searchEngine = Components.searchEngineManager
                .getDefaultSearchEngine(context, defaultIdentifier)


        client = SearchSuggestionClient(searchEngine!!, { fetch() })
    }
}