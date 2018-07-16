package mozilla.components.browser.search

import mozilla.components.browser.search.parser.selectParser

typealias SearchSuggestionFetcher = suspend (url: String) -> String?
class SearchSuggestionClient(private val searchEngine: SearchEngine, private val fetcher: SearchSuggestionFetcher ) {
    suspend fun getSuggestions(query: String): List<String>? {
        val suggestionsURL = searchEngine.buildSuggestionsURL(query) ?: return null
        val parser = selectParser(searchEngine)

        return fetcher(suggestionsURL)?.let { parser(it) }
    }
}