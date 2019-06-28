/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

package mozilla.components.browser.state.reducer

import mozilla.components.browser.state.action.CustomTabListAction
import mozilla.components.browser.state.selector.findCustomTab
import mozilla.components.browser.state.state.BrowserState

internal object CustomTabListReducer {
    /**
     * [CustomTabListAction] Reducer function for modifying [BrowserState.customTabs].
     */
    fun reduce(state: BrowserState, action: CustomTabListAction): BrowserState {
        return when (action) {
            is CustomTabListAction.AddCustomTabAction -> state.copy(customTabs = state.customTabs + action.tab)

            is CustomTabListAction.RemoveCustomTabAction -> {
                val tab = state.findCustomTab(action.tabId) ?: return state
                state.copy(customTabs = state.customTabs - tab)
            }

            is CustomTabListAction.RemoveAllCustomTabsAction -> {
                state.copy(customTabs = emptyList())
            }
        }
    }
}
