/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

package mozilla.components.feature.search.ext

import android.graphics.Bitmap
import android.net.Uri
import androidx.annotation.VisibleForTesting
import mozilla.components.browser.state.search.OS_SEARCH_ENGINE_TERMS_PARAM
import mozilla.components.browser.state.search.SearchEngine
import mozilla.components.browser.state.state.SearchState
import mozilla.components.browser.state.state.searchEngines
import mozilla.components.browser.state.state.selectedOrDefaultSearchEngine
import mozilla.components.feature.search.internal.SearchUrlBuilder
import mozilla.components.feature.search.storage.SearchEngineReader
import java.io.InputStream
import java.lang.IllegalArgumentException
import java.util.UUID

/**
 * Creates a custom [SearchEngine].
 */
fun createSearchEngine(
    name: String,
    url: String,
    icon: Bitmap,
    suggestUrl: String? = null,
): SearchEngine {
    if (!url.contains(OS_SEARCH_ENGINE_TERMS_PARAM)) {
        throw IllegalArgumentException("URL does not contain search terms placeholder")
    }

    return SearchEngine(
        id = UUID.randomUUID().toString(),
        name = name,
        icon = icon,
        type = SearchEngine.Type.CUSTOM,
        resultUrls = listOf(url),
        suggestUrl = suggestUrl,
    )
}

/**
 * Creates an application [SearchEngine].
 */
fun createApplicationSearchEngine(
    id: String? = null,
    name: String,
    url: String,
    icon: Bitmap,
    suggestUrl: String? = null,
): SearchEngine {
    return SearchEngine(
        id = id ?: UUID.randomUUID().toString(),
        name = name,
        icon = icon,
        type = SearchEngine.Type.APPLICATION,
        resultUrls = listOf(url),
        suggestUrl = suggestUrl,
    )
}

/**
 * Whether this [SearchEngine] has a [SearchEngine.suggestUrl] set and can provide search
 * suggestions.
 */
val SearchEngine.canProvideSearchSuggestions: Boolean
    get() = suggestUrl != null

/**
 * Creates an URL to retrieve search suggestions for the provided [query].
 */
fun SearchEngine.buildSuggestionsURL(query: String): String? {
    val builder = SearchUrlBuilder(this)
    return builder.buildSuggestionUrl(query)
}

/**
 * Builds a URL to search for the given search terms with this search engine.
 */
fun SearchEngine.buildSearchUrl(searchTerm: String): String {
    val builder = SearchUrlBuilder(this)
    return builder.buildSearchUrl(searchTerm)
}

/**
 * Parses a [SearchEngine] from the given [stream].
 */
@Deprecated("Only for migrating legacy search engines. Will eventually be removed.")
fun parseLegacySearchEngine(id: String, stream: InputStream): SearchEngine {
    val reader = SearchEngineReader(SearchEngine.Type.CUSTOM)
    return reader.loadStream(id, stream)
}

/**
 * Given a [SearchState], determine if the passed-in [url] is a known search results page url
 * and what are the associated search terms.
 * @return Search terms if [url] is a known search results page, `null` otherwise.
 */
fun SearchState.parseSearchTerms(url: String): String? {
    val parsedUrl = Uri.parse(url)
    // Default/selected engine is the most likely to match, check it first.
    val currentEngine = this.selectedOrDefaultSearchEngine
    // Or go through the rest of known engines.
    val fallback: () -> String? = fallback@{
        this.searchEngines.forEach { searchEngine ->
            searchEngine.parseSearchTerms(parsedUrl)?.let { return@fallback it }
        }
        return@fallback null
    }
    return currentEngine?.parseSearchTerms(parsedUrl) ?: fallback()
}

/**
 * Given a [SearchEngine], determine if the passed-in [url] matches its results template,
 * and what are the associated search terms.
 * @return Search terms if [url] matches the results page template, `null` otherwise.
 */
@VisibleForTesting
fun SearchEngine.parseSearchTerms(url: Uri): String? {
    // Basic approach:
    // - look at the "base" of the template url; if there's a match, continue
    // - see if the GET parameter for the search terms is present in the url
    // - if that param present, its value is our answer if it's non-empty
    val searchResultsRoot = this.resultsUrl.authority + this.resultsUrl.path
    val urlRoot = url.authority + url.path

    return if (searchResultsRoot == urlRoot) {
        val searchTerms = try {
            this.searchParameterName?.let {
                url.getQueryParameter(it)
            }
        } catch (e: UnsupportedOperationException) {
            // Non-hierarchical url.
            null
        }
        searchTerms.takeUnless { it.isNullOrEmpty() }
    } else {
        null
    }
}
