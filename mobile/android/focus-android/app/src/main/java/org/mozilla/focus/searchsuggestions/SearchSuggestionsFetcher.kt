/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

package org.mozilla.focus.searchsuggestions

import androidx.lifecycle.LiveData
import androidx.lifecycle.MutableLiveData
import kotlinx.coroutines.CoroutineScope
import kotlinx.coroutines.Dispatchers
import kotlinx.coroutines.Job
import kotlinx.coroutines.channels.Channel
import kotlinx.coroutines.channels.consumeEach
import kotlinx.coroutines.coroutineScope
import kotlinx.coroutines.launch
import mozilla.components.browser.state.search.SearchEngine
import mozilla.components.concept.fetch.Client
import mozilla.components.feature.search.suggestions.SearchSuggestionClient
import mozilla.components.support.ktx.kotlin.sanitizeURL
import org.mozilla.focus.utils.debounce
import java.util.concurrent.TimeUnit
import kotlin.coroutines.CoroutineContext
import mozilla.components.concept.fetch.Request as FetchRequest

class SearchSuggestionsFetcher(
    searchEngine: SearchEngine?,
    private val fetchClient: Client
) : CoroutineScope {
    private var job = Job()

    override val coroutineContext: CoroutineContext
        get() = job + Dispatchers.Main

    data class SuggestionResult(val query: String, val suggestions: List<String>)

    private var client: SearchSuggestionClient? = null

    private val fetchChannel = Channel<String>(capacity = Channel.UNLIMITED)

    private val _results = MutableLiveData<SuggestionResult>()
    val results: LiveData<SuggestionResult> = _results

    var canProvideSearchSuggestions = false
        private set

    init {
        updateSearchEngine(searchEngine)
        launch(Dispatchers.IO) {
            debounce(THROTTLE_AMOUNT, fetchChannel)
                .consumeEach { getSuggestions(it) }
        }
    }

    fun requestSuggestions(query: String) {
        if (query.isBlank()) { _results.value = SuggestionResult(query, listOf()); return }
        fetchChannel.trySend(query)
    }

    fun updateSearchEngine(searchEngine: SearchEngine?) {
        canProvideSearchSuggestions = searchEngine?.suggestUrl != null
        client = if (canProvideSearchSuggestions) {
            SearchSuggestionClient(searchEngine!!) { fetch(it) }
        } else {
            null
        }
    }

    private suspend fun getSuggestions(query: String) = coroutineScope {
        val suggestions = try {
            client?.getSuggestions(query) ?: listOf()
        } catch (ex: SearchSuggestionClient.ResponseParserException) {
            listOf<String>()
        } catch (ex: SearchSuggestionClient.FetchException) {
            listOf<String>()
        }

        launch(Dispatchers.Main) {
            _results.value = SuggestionResult(query, suggestions)
        }
    }

    private fun fetch(url: String): String {
        val request = FetchRequest(
            url = url.sanitizeURL(),
            readTimeout = Pair(READ_TIMEOUT_IN_MS, TimeUnit.MILLISECONDS),
            connectTimeout = Pair(CONNECT_TIMEOUT_IN_MS, TimeUnit.MILLISECONDS),
            private = true
        )
        return fetchClient.fetch(request).body.string()
    }

    companion object {
        private const val THROTTLE_AMOUNT: Long = 100

        private const val READ_TIMEOUT_IN_MS = 2000L
        private const val CONNECT_TIMEOUT_IN_MS = 1000L
    }
}
