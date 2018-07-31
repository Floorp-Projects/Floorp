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
import okhttp3.OkHttpClient
import okhttp3.Request

class SearchSuggestionsService(private val context: Context) {

    private var searchEngine: SearchEngine? = null
    private var client: SearchSuggestionClient? = null
    private var httpClient = OkHttpClient()

    private val _suggestions = MutableLiveData<Pair<String, List<String>>>()
    val suggestions: LiveData<Pair<String, List<String>>>
            get() = _suggestions

    private fun fetch(url: String): String? {
        httpClient.dispatcher().queuedCalls().forEach {
            if (it.request().tag() == REQUEST_TAG) {
                it.cancel()
            }
        }

        val request = Request.Builder()
                .tag(REQUEST_TAG)
                .url(url)
                .build()

        return httpClient.newCall(request).execute().body()?.string() ?: ""
    }

    fun getSuggestions(query: String) {
        if (shouldUpdateclient()) { updateClient() }
        if (query.isBlank()) { _suggestions.value = Pair(query, listOf()); return }

        launch(CommonPool) {
            val suggestions = client?.getSuggestions(query) ?: listOf()

            launch(UI) {
                _suggestions.value = Pair(query, suggestions)
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


        client = SearchSuggestionClient(searchEngine!!, { fetch(it) })
    }

    companion object {
        private val REQUEST_TAG = "searchSuggestionFetch"
    }
}