/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

package mozilla.components.feature.awesomebar.provider

import androidx.annotation.VisibleForTesting
import mozilla.components.browser.state.store.BrowserStore
import mozilla.components.concept.toolbar.AutocompleteProvider
import mozilla.components.concept.toolbar.AutocompleteResult
import mozilla.components.support.utils.doesUrlStartsWithText
import mozilla.components.support.utils.segmentAwareDomainMatch

@VisibleForTesting
internal const val LOCAL_TABS_AUTOCOMPLETE_SOURCE_NAME = "localTabs"

/**
 * Provide autocomplete suggestions from the currently opened tabs.
 *
 * @param store [BrowserStore] containing the information about the currently open tabs.
 * @param autocompletePriority Order in which this provider will be queried for autocomplete suggestions
 * in relation ot others.
 *  - a lower priority means that this provider must be called before others with a higher priority.
 *  - an equal priority offers no ordering guarantees.
 *
 * Defaults to `0`.
 */
class SessionAutocompleteProvider(
    private val store: BrowserStore,
    override val autocompletePriority: Int = 0,
) : AutocompleteProvider {
    override suspend fun getAutocompleteSuggestion(query: String): AutocompleteResult? {
        if (query.isEmpty()) {
            return null
        }

        val tabUrl = store.state.tabs
            .firstOrNull {
                !it.content.private && doesUrlStartsWithText(it.content.url, query)
            }
            ?.content?.url
            ?: return null

        val resultText = segmentAwareDomainMatch(query, arrayListOf(tabUrl))
        return resultText?.let {
            AutocompleteResult(
                input = query,
                text = it.matchedSegment,
                url = it.url,
                source = LOCAL_TABS_AUTOCOMPLETE_SOURCE_NAME,
                totalItems = 1,
            )
        }
    }
}
