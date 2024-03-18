/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

package mozilla.components.feature.search.suggestions

import android.content.Context
import mozilla.components.browser.state.search.SearchEngine
import mozilla.components.browser.state.state.selectedOrDefaultSearchEngine
import mozilla.components.browser.state.store.BrowserStore
import mozilla.components.feature.search.ext.buildSuggestionsURL
import mozilla.components.feature.search.ext.canProvideSearchSuggestions
import mozilla.components.support.base.log.logger.Logger
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
    private val logger = Logger("SearchSuggestionClient")

    val store: BrowserStore?
    var searchEngine: SearchEngine? = null
        private set

    internal constructor(
        context: Context?,
        store: BrowserStore?,
        searchEngine: SearchEngine?,
        fetcher: SearchSuggestionFetcher,
    ) {
        this.context = context
        this.store = store
        this.searchEngine = searchEngine
        this.fetcher = fetcher
    }

    constructor(searchEngine: SearchEngine, fetcher: SearchSuggestionFetcher) :
        this (null, null, searchEngine, fetcher)

    constructor(
        context: Context,
        store: BrowserStore,
        fetcher: SearchSuggestionFetcher,
    ) : this (context, store, null, fetcher)

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
            requireNotNull(store)
            requireNotNull(context)

            val searchEngine = store.state.search.selectedOrDefaultSearchEngine
            if (searchEngine == null) {
                logger.warn("No default search engine for fetching suggestions")
                return emptyList()
            } else {
                this.searchEngine = searchEngine
                searchEngine
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
