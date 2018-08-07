/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

package org.mozilla.focus.searchsuggestions

import android.arch.lifecycle.LiveData
import android.arch.lifecycle.MutableLiveData
import kotlinx.coroutines.experimental.CommonPool
import kotlinx.coroutines.experimental.android.UI
import mozilla.components.browser.search.suggestions.SearchSuggestionClient
import kotlinx.coroutines.experimental.launch
import mozilla.components.browser.search.SearchEngine
import okhttp3.OkHttpClient
import okhttp3.Request

class SearchSuggestionsService(searchEngine: SearchEngine) {
    private var client: SearchSuggestionClient? = null
    private var httpClient = OkHttpClient()

    var canProvideSearchSuggestions = false
        private set

    init {
        updateSearchEngine(searchEngine)
    }

    fun getSuggestions(query: String): LiveData<Pair<String, List<String>>> {
        val result = MutableLiveData<Pair<String, List<String>>>()

        if (query.isBlank()) { result.value = Pair(query, listOf()) }

        launch(CommonPool) {
            val suggestions = client?.getSuggestions(query) ?: listOf()

            launch(UI) {
                result.value = Pair(query, suggestions)
            }
        }

        return result
    }

    fun updateSearchEngine(searchEngine: SearchEngine) {
        canProvideSearchSuggestions = searchEngine.canProvideSearchSuggestions
        client = try {
            SearchSuggestionClient(searchEngine, { fetch(it) })
        } catch (_: IllegalArgumentException) {
            null
        }
    }

    private fun fetch(url: String): String? {
        httpClient.dispatcher().queuedCalls()
                .filter { it.request().tag() == REQUEST_TAG }
                .forEach { it.cancel() }

        val request = Request.Builder()
                .tag(REQUEST_TAG)
                .url(url)
                .build()

        return httpClient.newCall(request).execute().body()?.string() ?: ""
    }

    companion object {
        private const val REQUEST_TAG = "searchSuggestionFetch"
    }
}
