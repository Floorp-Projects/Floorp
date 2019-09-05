/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

package mozilla.components.browser.state.reducer

import mozilla.components.browser.state.action.EngineAction
import mozilla.components.browser.state.state.BrowserState
import mozilla.components.browser.state.state.EngineState
import mozilla.components.browser.state.state.SessionState

internal object EngineStateReducer {

    /**
     * [EngineAction] Reducer function for modifying a specific [EngineState]
     * of a [SessionState].
     */
    fun reduce(state: BrowserState, action: EngineAction): BrowserState = when (action) {
        is EngineAction.LinkEngineSessionAction -> state.copyWithEngineState(action.sessionId) {
            it.copy(engineSession = action.engineSession)
        }
        is EngineAction.UnlinkEngineSessionAction -> state.copyWithEngineState(action.sessionId) {
            it.copy(engineSession = null, engineSessionState = null)
        }
        is EngineAction.UpdateEngineSessionStateAction -> state.copyWithEngineState(action.sessionId) {
            it.copy(engineSessionState = action.engineSessionState)
        }
    }
}

private fun BrowserState.copyWithEngineState(
    tabId: String,
    update: (EngineState) -> EngineState
): BrowserState {
    // Currently we map over both lists (tabs and customTabs). We could optimize this away later on if we know what
    // type we want to modify.
    return copy(
        tabs = tabs.map { current ->
            if (current.id == tabId) {
                current.copy(engineState = update.invoke(current.engineState))
            } else {
                current
            }
        },
        customTabs = customTabs.map { current ->
            if (current.id == tabId) {
                current.copy(engineState = update.invoke(current.engineState))
            } else {
                current
            }
        }
    )
}
