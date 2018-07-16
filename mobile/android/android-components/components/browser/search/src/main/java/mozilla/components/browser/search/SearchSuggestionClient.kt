package mozilla.components.browser.search

import mozilla.components.browser.search.parser.defaultParser

typealias SearchSuggestionFetcher = suspend (url: String) -> String?
class SearchSuggestionClient(private val searchEngine: SearchEngine, private val fetcher: SearchSuggestionFetcher ) {
    suspend fun getSuggestions(query: String): List<String>? {
        val suggestionsURL = searchEngine.buildSuggestionsURL(query) ?: return null

        return fetcher(suggestionsURL)?.let { defaultParser(it) }
    }
}