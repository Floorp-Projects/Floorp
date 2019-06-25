/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

package mozilla.components.browser.state.reducer

import mozilla.components.browser.state.action.TabListAction
import mozilla.components.browser.state.selector.findTab
import mozilla.components.browser.state.state.BrowserState
import mozilla.components.browser.state.state.TabSessionState

internal object TabListReducer {
    /**
     * [TabListAction] Reducer function for modifying the list of [TabSessionState] objects in [BrowserState.tabs].
     */
    fun reduce(state: BrowserState, action: TabListAction): BrowserState {
        return when (action) {
            is TabListAction.AddTabAction -> state.copy(
                tabs = state.tabs + action.tab,
                selectedTabId = if (action.select) action.tab.id else state.selectedTabId)

            is TabListAction.SelectTabAction -> state.copy(selectedTabId = action.tabId)

            is TabListAction.RemoveTabAction -> {
                val tab = state.findTab(action.tabId)
                if (tab == null) {
                    state
                } else {
                    state.copy(tabs = state.tabs - tab)
                }
            }
        }
    }
}
