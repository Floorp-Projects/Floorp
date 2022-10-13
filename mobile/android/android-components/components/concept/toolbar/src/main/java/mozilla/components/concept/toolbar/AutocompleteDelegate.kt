/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

package mozilla.components.concept.toolbar

/**
 * Describes an object to which a [AutocompleteResult] may be applied.
 * Usually, this will delegate to a specific text view.
 */
interface AutocompleteDelegate {
    /**
     * @param result Apply result of autocompletion.
     * @param onApplied a lambda/callback invoked if (and only if) the result has been
     * applied. A result may be discarded by implementations because it is stale or
     * the autocomplete request has been cancelled.
     */
    fun applyAutocompleteResult(result: AutocompleteResult, onApplied: () -> Unit = { })

    /**
     * Autocompletion was invoked and no match was returned.
     */
    fun noAutocompleteResult(input: String)
}

/**
 * Describes an autocompletion result.
 * @property input Input for which this AutocompleteResult is being provided.
 * @property text AutocompleteResult of autocompletion, text to be displayed.
 * @property url AutocompleteResult of autocompletion, full matching url.
 * @property source Name of the autocompletion source.
 * @property totalItems A total number of results also available.
 */
data class AutocompleteResult(
    val input: String,
    val text: String,
    val url: String,
    val source: String,
    val totalItems: Int,
)
