/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

package mozilla.components.browser.state.reducer

import mozilla.components.browser.state.action.CustomTabListAction
import mozilla.components.browser.state.ext.toTab
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
                state.copy(customTabs = state.customTabs.filter { it.id != action.tabId })
            }

            is CustomTabListAction.RemoveAllCustomTabsAction -> {
                state.copy(customTabs = emptyList())
            }

            is CustomTabListAction.TurnCustomTabIntoNormalTabAction -> {
                val customTab = state.findCustomTab(action.tabId)
                if (customTab == null) {
                    state
                } else {
                    val tab = customTab.toTab()
                    state.copy(
                        customTabs = state.customTabs - customTab,
                        tabs = state.tabs + tab,
                    )
                }
            }
        }
    }
}
