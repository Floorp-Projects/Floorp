/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

package mozilla.components.browser.search.suggestions

import android.content.Context
import mozilla.components.browser.search.SearchEngine
import mozilla.components.browser.search.SearchEngineManager
import org.json.JSONException
import java.io.IOException

/**
 * Async function responsible for taking a URL and returning the results
 */
typealias SearchSuggestionFetcher = suspend (url: String) -> String?

/**
 *  Provides an interface to get search suggestions from a given SearchEngine.
 */
class SearchSuggestionClient {

    private val context: Context?
    private val fetcher: SearchSuggestionFetcher

    val searchEngineManager: SearchEngineManager?
    var searchEngine: SearchEngine? = null
        private set

    internal constructor(
        context: Context?,
        searchEngineManager: SearchEngineManager?,
        searchEngine: SearchEngine?,
        fetcher: SearchSuggestionFetcher
    ) {
        this.context = context
        this.searchEngineManager = searchEngineManager
        this.searchEngine = searchEngine
        this.fetcher = fetcher
    }

    constructor(searchEngine: SearchEngine, fetcher: SearchSuggestionFetcher) :
        this (null, null, searchEngine, fetcher)

    constructor(context: Context, searchEngineManager: SearchEngineManager, fetcher: SearchSuggestionFetcher) :
        this (context, searchEngineManager, null, fetcher)

    /**
     * Exception types for errors caught while getting a list of suggestions
     */
    class FetchException : Exception("There was a problem fetching suggestions")
    class ResponseParserException : Exception("There was a problem parsing the suggestion response")

    /**
     * Gets search suggestions for a given query
     */
    suspend fun getSuggestions(query: String): List<String>? {
        val searchEngine = searchEngine ?: run {
            requireNotNull(searchEngineManager)
            requireNotNull(context)
            searchEngineManager.getDefaultSearchEngineAsync(context).also {
                searchEngine = it
            }
        }

        if (!searchEngine.canProvideSearchSuggestions) {
            // This search engine doesn't support suggestions. Let's only return a default suggestion
            // for the entered text.
            return emptyList()
        }

        val suggestionsURL = searchEngine.buildSuggestionsURL(query)

        val parser = selectResponseParser(searchEngine)

        val suggestionResults = try {
            suggestionsURL?.let { fetcher(it) }
        } catch (_: IOException) {
            throw FetchException()
        }

        return try {
            suggestionResults?.let(parser)
        } catch (_: JSONException) {
            throw ResponseParserException()
        }
    }
}
