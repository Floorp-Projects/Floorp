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

        val template = Uri.decode(searchUri.toString())
        val urlWithSubstitutions = paramSubstitution(template, Uri.encode(searchTerm))
        return normalize(urlWithSubstitutions) // User-entered search engines may need normalization.
    }

    /**
     * Formats template string with proper parameters. Modeled after ParamSubstitution in nsSearchService.js
     */
    private fun paramSubstitution(template: String, query: String): String {
        var result = template
        val locale = Locale.getDefault().toString()

        result = result.replace(MOZ_PARAM_LOCALE.toRegex(), locale)
        result = result.replace(MOZ_PARAM_DIST_ID.toRegex(), "")
        result = result.replace(MOZ_PARAM_OFFICIAL.toRegex(), "unofficial")

        result = result.replace(OS_PARAM_USER_DEFINED.toRegex(), query)
        result = result.replace(OS_PARAM_INPUT_ENCODING.toRegex(), "UTF-8")

        result = result.replace(OS_PARAM_LANGUAGE.toRegex(), locale)
        result = result.replace(OS_PARAM_OUTPUT_ENCODING.toRegex(), "UTF-8")

        // Replace any optional parameters
        result = result.replace(OS_PARAM_OPTIONAL.toRegex(), "")

        return result
    }

    companion object {
        // Parameters copied from nsSearchService.js
        private const val MOZ_PARAM_LOCALE = "\\{moz:locale}"
        private const val MOZ_PARAM_DIST_ID = "\\{moz:distributionID}"
        private const val MOZ_PARAM_OFFICIAL = "\\{moz:official}"

        // Supported OpenSearch parameters
        // See http://opensearch.a9.com/spec/1.1/querysyntax/#core
        private const val OS_PARAM_USER_DEFINED = "\\{searchTerms\\??}"
        private const val OS_PARAM_INPUT_ENCODING = "\\{inputEncoding\\??}"
        private const val OS_PARAM_LANGUAGE = "\\{language\\??}"
        private const val OS_PARAM_OUTPUT_ENCODING = "\\{outputEncoding\\??}"
        private const val OS_PARAM_OPTIONAL = "\\{(?:\\w+:)?\\w+?}"

        private fun normalize(input: String): String {
            val trimmedInput = input.trim { it <= ' ' }
            var uri = Uri.parse(trimmedInput)

            if (TextUtils.isEmpty(uri.scheme)) {
                uri = Uri.parse("http://$trimmedInput")
            }

            return uri.toString()
        }
    }
}
