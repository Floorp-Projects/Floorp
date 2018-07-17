/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

package mozilla.components.browser.search.suggestions

import mozilla.components.browser.search.SearchEngine

/**
 * Async function responsible for taking a URL and returning the results
 */
typealias SearchSuggestionFetcher = suspend (url: String) -> String?

/**
 *  Provides an interface to get search suggestions from a given SearchEngine.
 */
class SearchSuggestionClient private constructor(
    private val searchEngine: SearchEngine,
    private val fetcher: SearchSuggestionFetcher
) {
    companion object {
        fun create(searchEngine: SearchEngine, fetcher: SearchSuggestionFetcher): SearchSuggestionClient? {
            if (!searchEngine.canProvideSearchSuggestions) { return null }

            return SearchSuggestionClient(searchEngine, fetcher)
        }
    }

    /**
     * Gets search suggestions for a given query
     */
    suspend fun getSuggestions(query: String): List<String>? {
        val suggestionsURL = searchEngine.buildSuggestionsURL(query) ?: return null
        val parser = selectResponseParser(searchEngine)

        return fetcher(suggestionsURL)?.let { parser(it) }
    }
}
