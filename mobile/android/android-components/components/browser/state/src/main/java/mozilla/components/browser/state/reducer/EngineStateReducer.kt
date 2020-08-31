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
            it.copy(
                engineSession = action.engineSession
            )
        }
        is EngineAction.UnlinkEngineSessionAction -> state.copyWithEngineState(action.sessionId) {
            it.copy(
                engineSession = null,
                engineObserver = null
            )
        }
        is EngineAction.UpdateEngineSessionObserverAction -> state.copyWithEngineState(action.sessionId) {
            it.copy(engineObserver = action.engineSessionObserver)
        }
        is EngineAction.UpdateEngineSessionStateAction -> state.copyWithEngineState(action.sessionId) { engineState ->
            if (engineState.crashed) {
                // We ignore state updates for a crashed engine session. We want to keep the last state until
                // this tab gets restored (or closed).
                engineState
            } else {
                engineState.copy(engineSessionState = action.engineSessionState)
            }
        }
        is EngineAction.SuspendEngineSessionAction,
        is EngineAction.CreateEngineSessionAction,
        is EngineAction.LoadDataAction,
        is EngineAction.LoadUrlAction,
        is EngineAction.ReloadAction,
        is EngineAction.GoBackAction,
        is EngineAction.GoForwardAction,
        is EngineAction.GoToHistoryIndexAction,
        is EngineAction.ToggleDesktopModeAction,
        is EngineAction.ExitFullScreenModeAction,
        is EngineAction.ClearDataAction -> {
            throw IllegalStateException("You need to add EngineMiddleware to your BrowserStore. ($action)")
        }
    }
}

private inline fun BrowserState.copyWithEngineState(
    tabId: String,
    crossinline update: (EngineState) -> EngineState
): BrowserState {
    return updateTabState(tabId) { current ->
        current.createCopy(engineState = update(current.engineState))
    }
}
