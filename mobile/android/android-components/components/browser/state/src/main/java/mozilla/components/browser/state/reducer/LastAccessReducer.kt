/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

package mozilla.components.browser.state.reducer

import mozilla.components.browser.state.action.LastAccessAction
import mozilla.components.browser.state.action.LastAccessAction.ResetLastMediaSessionAction
import mozilla.components.browser.state.action.LastAccessAction.UpdateLastAccessAction
import mozilla.components.browser.state.action.LastAccessAction.UpdateLastMediaAccessAction
import mozilla.components.browser.state.state.BrowserState
import mozilla.components.browser.state.state.TabSessionState

internal object LastAccessReducer {
    /**
     * [LastAccessAction] Reducer function for modifying [TabSessionState.lastAccess] state.
     */
    fun reduce(state: BrowserState, action: LastAccessAction): BrowserState = when (action) {
        is UpdateLastAccessAction -> {
            state.updateTabState(action.tabId) { sessionState ->
                sessionState.copy(lastAccess = action.lastAccess)
            }
        }
        is UpdateLastMediaAccessAction -> {
            state.updateTabState(action.tabId) { sessionState ->
                sessionState.copy(
                    lastMediaAccessState = sessionState.lastMediaAccessState.copy(
                        lastMediaUrl = sessionState.content.url,
                        lastMediaAccess = action.lastMediaAccess,
                        mediaSessionActive = true,
                    ),
                )
            }
        }
        is ResetLastMediaSessionAction -> {
            state.updateTabState(action.tabId) { sessionState ->
                sessionState.copy(
                    lastMediaAccessState = sessionState.lastMediaAccessState.copy(
                        mediaSessionActive = false,
                    ),
                )
            }
        }
    }
}
