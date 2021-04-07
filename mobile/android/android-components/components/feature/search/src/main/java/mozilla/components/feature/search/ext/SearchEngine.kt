/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

package mozilla.components.feature.search.ext

import android.graphics.Bitmap
import mozilla.components.browser.state.search.SearchEngine
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
    suggestUrl: String? = null
): SearchEngine {
    if (!url.contains("{searchTerms}")) {
        throw IllegalArgumentException("URL does not contain search terms placeholder")
    }

    return SearchEngine(
        id = UUID.randomUUID().toString(),
        name = name,
        icon = icon,
        type = SearchEngine.Type.CUSTOM,
        resultUrls = listOf(url),
        suggestUrl = suggestUrl
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
