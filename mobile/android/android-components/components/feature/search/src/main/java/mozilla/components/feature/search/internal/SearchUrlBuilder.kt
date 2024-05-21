/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

package mozilla.components.feature.search.internal

import android.net.Uri
import android.text.TextUtils
import mozilla.components.browser.state.search.OS_SEARCH_ENGINE_TERMS_PARAM
import mozilla.components.browser.state.search.SearchEngine
import java.io.UnsupportedEncodingException
import java.net.URLEncoder
import java.util.Locale

// We are using string concatenation here to avoid the Kotlin compiler interpreting this
// as string templates. It is possible to escape the string accordingly. But this seems to
// be inconsistent between Kotlin versions. So to be safe we avoid this completely by
// constructing the strings manually.

// Supported OpenSearch parameters
// See http://opensearch.a9.com/spec/1.1/querysyntax/#core
private const val OS_PARAM_USER_DEFINED = OS_SEARCH_ENGINE_TERMS_PARAM
private const val OS_PARAM_INPUT_ENCODING = "{" + "inputEncoding" + "}"
private const val OS_PARAM_LANGUAGE = "{" + "language" + "}"
private const val OS_PARAM_OUTPUT_ENCODING = "{" + "outputEncoding" + "}"
private const val OS_PARAM_OPTIONAL = "\\{" + "(?:\\w+:)?\\w+?" + "\\}"

internal class SearchUrlBuilder(
    private val searchEngine: SearchEngine,
) {
    fun buildSearchUrl(searchTerms: String): String {
        // The parser should have put the best URL for this device at the beginning of the list.
        val template = searchEngine.resultUrls[0]
        return buildUrl(template, searchTerms)
    }

    fun buildSuggestionUrl(searchTerms: String): String? {
        val template = searchEngine.suggestUrl ?: return null
        return buildUrl(template, searchTerms)
    }

    private fun buildUrl(template: String, searchTerms: String): String {
        val templateUri = Uri.decode(template)
        val inputEncoding = searchEngine.inputEncoding ?: "UTF-8"
        val query = try {
            // Although android.net.Uri.encode convert space (U+0x20) to "%20", java.net.URLEncoder convert it to "+".
            URLEncoder.encode(searchTerms, inputEncoding).replace("+", "%20")
        } catch (e: UnsupportedEncodingException) {
            Uri.encode(searchTerms)
        }
        val urlWithSubstitutions = paramSubstitution(templateUri, query, inputEncoding)
        return normalize(urlWithSubstitutions) // User-entered search engines may need normalization.
    }
}

/**
 * Formats template string with proper parameters. Modeled after ParamSubstitution in nsSearchService.js
 */
private fun paramSubstitution(template: String, query: String, inputEncoding: String): String {
    var result = template
    val locale = Locale.getDefault().toString()

    result = result.replace(OS_PARAM_USER_DEFINED, query)
    result = result.replace(OS_PARAM_INPUT_ENCODING, inputEncoding)

    result = result.replace(OS_PARAM_LANGUAGE, locale)
    result = result.replace(OS_PARAM_OUTPUT_ENCODING, "UTF-8")

    // Replace any optional parameters
    result = result.replace(OS_PARAM_OPTIONAL.toRegex(), "")

    return result
}

private fun normalize(input: String): String {
    val trimmedInput = input.trim { it <= ' ' }
    var uri = Uri.parse(trimmedInput)

    if (TextUtils.isEmpty(uri.scheme)) {
        uri = Uri.parse("http://$trimmedInput")
    }

    return uri.toString()
}
