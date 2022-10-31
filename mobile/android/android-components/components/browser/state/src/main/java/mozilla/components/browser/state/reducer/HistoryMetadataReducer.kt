/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

package mozilla.components.browser.state.reducer

import mozilla.components.browser.state.action.HistoryMetadataAction
import mozilla.components.browser.state.state.BrowserState
import mozilla.components.browser.state.state.TabSessionState
import mozilla.components.concept.storage.HistoryMetadataKey

internal object HistoryMetadataReducer {

    /**
     * [HistoryMetadataAction] reducer function for modifying [TabSessionState.historyMetadata].
     */
    fun reduce(state: BrowserState, action: HistoryMetadataAction): BrowserState {
        return when (action) {
            is HistoryMetadataAction.SetHistoryMetadataKeyAction ->
                state.updateHistoryMetadataKey(action.tabId, action.historyMetadataKey)

            is HistoryMetadataAction.DisbandSearchGroupAction ->
                state.disbandSearchGroup(action.searchTerm)
        }
    }
}

private fun BrowserState.updateHistoryMetadataKey(
    tabId: String,
    key: HistoryMetadataKey,
): BrowserState {
    return copy(
        tabs = tabs.updateTabs(tabId) { current ->
            current.copy(historyMetadata = key)
        } ?: tabs,
    )
}

private fun BrowserState.disbandSearchGroup(searchTerm: String): BrowserState {
    val searchTermLowerCase = searchTerm.lowercase()
    return copy(
        tabs = tabs.map { tab ->
            if (tab.historyMetadata?.searchTerm?.lowercase() == searchTermLowerCase) {
                tab.copy(historyMetadata = HistoryMetadataKey(url = tab.historyMetadata.url))
            } else {
                tab
            }
        },
    )
}
