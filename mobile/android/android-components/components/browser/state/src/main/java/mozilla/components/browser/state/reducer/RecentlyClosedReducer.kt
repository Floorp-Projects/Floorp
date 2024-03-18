/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

package mozilla.components.browser.state.reducer

import mozilla.components.browser.state.action.RecentlyClosedAction
import mozilla.components.browser.state.state.BrowserState

internal object RecentlyClosedReducer {
    fun reduce(state: BrowserState, action: RecentlyClosedAction): BrowserState {
        return when (action) {
            is RecentlyClosedAction.AddClosedTabsAction -> {
                state.copy(
                    closedTabs = state.closedTabs + action.tabs.map { it.state },
                )
            }
            is RecentlyClosedAction.PruneClosedTabsAction -> {
                state.copy(
                    closedTabs = state.closedTabs.sortedByDescending { it.lastAccess }
                        .take(action.maxTabs),
                )
            }
            is RecentlyClosedAction.ReplaceTabsAction -> state.copy(closedTabs = action.tabs)
            is RecentlyClosedAction.RemoveClosedTabAction -> {
                state.copy(
                    closedTabs = state.closedTabs - action.tab,
                )
            }
            is RecentlyClosedAction.RemoveAllClosedTabAction -> {
                state.copy(closedTabs = listOf())
            }
        }
    }
}
