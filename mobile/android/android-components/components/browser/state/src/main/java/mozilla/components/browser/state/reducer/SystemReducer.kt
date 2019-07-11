/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

package mozilla.components.browser.state.reducer

import mozilla.components.browser.state.action.SystemAction
import mozilla.components.browser.state.state.BrowserState

internal object SystemReducer {
    /**
     * [SystemAction] Reducer function for modifying [BrowserState].
     */
    fun reduce(state: BrowserState, action: SystemAction): BrowserState {
        return when (action) {
            is SystemAction.LowMemoryAction -> {
                val updatedTabs = state.tabs.map {
                    if (state.selectedTabId != it.id) {
                        it.copy(content = it.content.copy(thumbnail = null))
                    } else {
                        it
                    }
                }
                state.copy(tabs = updatedTabs)
            }
        }
    }
}
