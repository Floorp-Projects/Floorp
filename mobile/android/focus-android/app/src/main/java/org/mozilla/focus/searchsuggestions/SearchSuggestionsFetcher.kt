/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

package org.mozilla.focus.searchsuggestions

import android.arch.lifecycle.LiveData
import android.arch.lifecycle.MutableLiveData
import kotlinx.coroutines.CoroutineScope
import kotlinx.coroutines.Dispatchers
import kotlinx.coroutines.Dispatchers.IO
import kotlinx.coroutines.Dispatchers.Main
import kotlinx.coroutines.Job
import kotlinx.coroutines.channels.Channel
import kotlinx.coroutines.channels.consumeEach
import kotlinx.coroutines.coroutineScope
import kotlinx.coroutines.launch
import mozilla.components.browser.search.SearchEngine
import mozilla.components.browser.search.suggestions.SearchSuggestionClient
import okhttp3.OkHttpClient
import okhttp3.Request
import org.mozilla.focus.utils.debounce
import kotlin.coroutines.CoroutineContext

class SearchSuggestionsFetcher(searchEngine: SearchEngine) : CoroutineScope {
    private val job = Job()
    override val coroutineContext: CoroutineContext
        get() = job + Dispatchers.Main
    // TODO When should we cancel the job?

    data class SuggestionResult(val query: String, val suggestions: List<String>)

    private var client: SearchSuggestionClient? = null
    private var httpClient = OkHttpClient()

    private val fetchChannel = Channel<String>(capacity = Channel.UNLIMITED)

    private val _results = MutableLiveData<SuggestionResult>()
    val results: LiveData<SuggestionResult> = _results

    var canProvideSearchSuggestions = false
        private set

    init {
        updateSearchEngine(searchEngine)
        launch(IO) {
            debounce(THROTTLE_AMOUNT, fetchChannel)
                .consumeEach { getSuggestions(it) }
        }
    }

    fun requestSuggestions(query: String) {
        if (query.isBlank()) { _results.value = SuggestionResult(query, listOf()); return }
        fetchChannel.offer(query)
    }

    fun updateSearchEngine(searchEngine: SearchEngine) {
        canProvideSearchSuggestions = searchEngine.canProvideSearchSuggestions
        client = if (canProvideSearchSuggestions) SearchSuggestionClient(searchEngine, { fetch(it) }) else null
    }

    private suspend fun getSuggestions(query: String) = coroutineScope {
        val suggestions = try {
            client?.getSuggestions(query) ?: listOf()
        } catch (ex: SearchSuggestionClient.ResponseParserException) {
            listOf<String>()
        } catch (ex: SearchSuggestionClient.FetchException) {
            listOf<String>()
        }

        launch(Main) {
            _results.value = SuggestionResult(query, suggestions)
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

    fun cancelJobs() {
        job.cancel()
    }

    companion object {
        private const val REQUEST_TAG = "searchSuggestionFetch"
        private const val THROTTLE_AMOUNT: Long = 100
    }
}
