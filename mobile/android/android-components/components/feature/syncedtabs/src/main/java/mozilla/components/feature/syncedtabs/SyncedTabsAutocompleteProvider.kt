/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

package mozilla.components.feature.syncedtabs

import androidx.annotation.VisibleForTesting
import mozilla.components.concept.toolbar.AutocompleteProvider
import mozilla.components.concept.toolbar.AutocompleteResult
import mozilla.components.feature.syncedtabs.ext.getActiveDeviceTabs
import mozilla.components.feature.syncedtabs.storage.SyncedTabsStorage
import mozilla.components.support.utils.doesUrlStartsWithText
import mozilla.components.support.utils.segmentAwareDomainMatch

@VisibleForTesting
internal const val SYNCED_TABS_AUTOCOMPLETE_SOURCE_NAME = "syncedTabs"

/**
 * Provide autocomplete suggestions from synced tabs.
 *
 * @param syncedTabs [SyncedTabsStorage] containing the information about the available synced tabs.
 * @param autocompletePriority Order in which this provider will be queried for autocomplete suggestions
 * in relation ot others.
 *  - a lower priority means that this provider must be called before others with a higher priority.
 *  - an equal priority offers no ordering guarantees.
 *
 * Defaults to `0`.
 */
class SyncedTabsAutocompleteProvider(
    private val syncedTabs: SyncedTabsStorage,
    override val autocompletePriority: Int = 0,
) : AutocompleteProvider {
    override suspend fun getAutocompleteSuggestion(query: String): AutocompleteResult? {
        val tabUrl = syncedTabs
            .getActiveDeviceTabs { doesUrlStartsWithText(it.url, query) }
            .firstOrNull()
            ?.tab?.url
            ?: return null

        val resultText = segmentAwareDomainMatch(query, arrayListOf(tabUrl))
        return resultText?.let {
            AutocompleteResult(
                input = query,
                text = it.matchedSegment,
                url = it.url,
                source = SYNCED_TABS_AUTOCOMPLETE_SOURCE_NAME,
                totalItems = 1,
            )
        }
    }
}
