package mozilla.components.browser.search

import kotlinx.coroutines.experimental.async
import kotlinx.coroutines.experimental.Deferred
import mozilla.components.browser.search.parser.googleParser

typealias SearchSuggestionFetcher = suspend (url: String) -> String?
class SearchSuggestionClient(private val searchEngine: SearchEngine, private val fetcher: SearchSuggestionFetcher ) {
    suspend fun getSuggestions(query: String): List<String>? {
        val suggestionsURL = searchEngine.buildSuggestionsURL(query) ?: return null

        return fetcher(suggestionsURL)?.let { googleParser(it) }
    }
}