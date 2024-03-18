/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

package mozilla.components.concept.toolbar

/**
 * Describes an autocompletion result.
 *
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
