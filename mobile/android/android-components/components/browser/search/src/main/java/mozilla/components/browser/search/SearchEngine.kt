/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

package mozilla.components.browser.search

import android.graphics.Bitmap
import android.net.Uri
import android.text.TextUtils
import java.util.Locale

/**
 * A data class representing a search engine.
 */
class SearchEngine internal constructor(
    val identifier: String,
    val name: String,
    val icon: Bitmap,
    private val resultsUris: List<Uri>,
    private val suggestUri: Uri? = null
) {
    val canProvideSearchSuggestions: Boolean = suggestUri != null

    init {
        if (resultsUris.isEmpty()) {
            throw IllegalArgumentException("Results uri list should not be empty!")
        }
    }

    /**
     * Builds a URL to search for the given search terms with this search engine.
     */
    fun buildSearchUrl(searchTerm: String): String {
        // The parse should have put the best URL for this device at the beginning of the list.
        val searchUri = resultsUris[0]

        return buildURL(searchUri, searchTerm)
    }

    /**
     * Builds a URL to get suggestions from this search engine.
     */
    fun buildSuggestionsURL(searchTerm: String): String? {
        val suggestUri = suggestUri ?: return null
        return buildURL(suggestUri, searchTerm)
    }

    private fun buildURL(uri: Uri, searchTerm: String): String {
        val template = Uri.decode(uri.toString())
        val urlWithSubstitutions = paramSubstitution(template, Uri.encode(searchTerm))
        return normalize(urlWithSubstitutions) // User-entered search engines may need normalization.
    }

    /**
     * Formats template string with proper parameters. Modeled after ParamSubstitution in nsSearchService.js
     */
    private fun paramSubstitution(template: String, query: String): String {
        var result = template
        val locale = Locale.getDefault().toString()

        result = result.replace(MOZ_PARAM_LOCALE, locale)
        result = result.replace(MOZ_PARAM_DIST_ID, "")
        result = result.replace(MOZ_PARAM_OFFICIAL, "unofficial")

        result = result.replace(OS_PARAM_USER_DEFINED, query)
        result = result.replace(OS_PARAM_INPUT_ENCODING, "UTF-8")

        result = result.replace(OS_PARAM_LANGUAGE, locale)
        result = result.replace(OS_PARAM_OUTPUT_ENCODING, "UTF-8")

        // Replace any optional parameters
        result = result.replace(OS_PARAM_OPTIONAL.toRegex(), "")

        return result
    }

    companion object {
        // We are using string concatenation here to avoid the Kotlin compiler interpreting this
        // as string templates. It is possible to escape the string accordingly. But this seems to
        // be inconsistent between Kotlin versions. So to be safe we avoid this completely by
        // constructing the strings manually.

        // Parameters copied from nsSearchService.js
        private const val MOZ_PARAM_LOCALE = "{" + "moz:locale" + "}"
        private const val MOZ_PARAM_DIST_ID = "{" + "moz:distributionID" + "}"
        private const val MOZ_PARAM_OFFICIAL = "{" + "moz:official" + "}"

        // Supported OpenSearch parameters
        // See http://opensearch.a9.com/spec/1.1/querysyntax/#core
        private const val OS_PARAM_USER_DEFINED = "{" + "searchTerms" + "}"
        private const val OS_PARAM_INPUT_ENCODING = "{" + "inputEncoding" + "}"
        private const val OS_PARAM_LANGUAGE = "{" + "language" + "}"
        private const val OS_PARAM_OUTPUT_ENCODING = "{" + "outputEncoding" + "}"
        private const val OS_PARAM_OPTIONAL = "\\{" + "(?:\\w+:)?\\w+?" + "\\}"

        private fun normalize(input: String): String {
            val trimmedInput = input.trim { it <= ' ' }
            var uri = Uri.parse(trimmedInput)

            if (TextUtils.isEmpty(uri.scheme)) {
                uri = Uri.parse("http://$trimmedInput")
            }

            return uri.toString()
        }
    }

    override fun toString(): String = "SearchEngine($identifier)"
}
