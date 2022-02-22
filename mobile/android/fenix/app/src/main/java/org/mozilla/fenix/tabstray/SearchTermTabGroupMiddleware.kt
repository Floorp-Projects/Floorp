/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

package org.mozilla.fenix.tabstray

import mozilla.components.browser.state.action.BrowserAction
import mozilla.components.browser.state.action.HistoryMetadataAction
import mozilla.components.browser.state.action.TabGroupAction
import mozilla.components.browser.state.action.TabListAction
import mozilla.components.browser.state.state.BrowserState
import mozilla.components.browser.state.state.getGroupByName
import mozilla.components.lib.state.Middleware
import mozilla.components.lib.state.MiddlewareContext

const val SEARCH_TERM_TAB_GROUPS = "searchTermTabGroups"
const val SEARCH_TERM_TAB_GROUPS_MIN_SIZE = 2

/**
 * This [Middleware] manages tab groups for search terms.
 */
class SearchTermTabGroupMiddleware : Middleware<BrowserState, BrowserAction> {

    override fun invoke(
        context: MiddlewareContext<BrowserState, BrowserAction>,
        next: (BrowserAction) -> Unit,
        action: BrowserAction
    ) {

        next(action)

        when (action) {
            is HistoryMetadataAction.SetHistoryMetadataKeyAction -> {
                action.historyMetadataKey.searchTerm?.let { searchTerm ->
                    context.dispatch(
                        TabGroupAction.AddTabAction(SEARCH_TERM_TAB_GROUPS, searchTerm, action.tabId)
                    )
                }
            }
            is HistoryMetadataAction.DisbandSearchGroupAction -> {
                val group = context.state.tabPartitions[SEARCH_TERM_TAB_GROUPS]?.getGroupByName(action.searchTerm)
                group?.let {
                    context.dispatch(TabGroupAction.RemoveTabGroupAction(SEARCH_TERM_TAB_GROUPS, it.id))
                }
            }
            is TabListAction.RestoreAction -> {
                action.tabs.forEach { tab ->
                    tab.state.historyMetadata?.searchTerm?.let { searchTerm ->
                        context.dispatch(
                            TabGroupAction.AddTabAction(SEARCH_TERM_TAB_GROUPS, searchTerm, tab.state.id)
                        )
                    }
                }
            }
            else -> {
                // no-op
            }
        }
    }
}
