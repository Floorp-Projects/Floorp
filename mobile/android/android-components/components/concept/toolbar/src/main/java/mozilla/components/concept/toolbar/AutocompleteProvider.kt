/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

package mozilla.components.concept.toolbar

/**
 * Object providing autocomplete suggestions for the toolbar.
 * More such objects can be set for the same toolbar with each getting results from a different source.
 * If more providers are used the [autocompletePriority] property allows to easily set an order
 * for the results and the suggestion of which provider should be tried to be applied first.
 */
interface AutocompleteProvider : Comparable<AutocompleteProvider> {
    /**
     * Retrieves an autocomplete suggestion which best matches [query].
     *
     * @param query Segment of text to be autocompleted.
     *
     * @return Optional domain URL which best matches the query.
     */
    suspend fun getAutocompleteSuggestion(query: String): AutocompleteResult?

    /**
     * Order in which this provider will be queried for autocomplete suggestions in relation ot others.
     *  - a lower priority means that this provider must be called before others with a higher priority.
     *  - an equal priority offers no ordering guarantees.
     *
     * Defaults to `0`.
     */
    val autocompletePriority: Int
        get() = 0

    override fun compareTo(other: AutocompleteProvider): Int {
        return autocompletePriority - other.autocompletePriority
    }
}
