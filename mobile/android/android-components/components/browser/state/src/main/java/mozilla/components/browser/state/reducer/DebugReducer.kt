/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

package mozilla.components.browser.state.reducer

import mozilla.components.browser.state.action.DebugAction
import mozilla.components.browser.state.state.BrowserState
import mozilla.components.lib.state.DelicateAction

internal object DebugReducer {
    /**
     * [DebugAction] Reducer function for modifying internal state for debugging purposes only.
     */
    @OptIn(DelicateAction::class)
    fun reduce(state: BrowserState, action: DebugAction): BrowserState {
        return when (action) {
            is DebugAction.UpdateCreatedAtAction ->
                state.updateTabState(action.tabId) { sessionState ->
                    sessionState.copy(createdAt = action.createdAt)
                }
        }
    }
}
