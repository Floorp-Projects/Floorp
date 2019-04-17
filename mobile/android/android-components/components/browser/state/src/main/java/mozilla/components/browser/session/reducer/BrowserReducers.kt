/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

package mozilla.components.browser.session.reducer

import mozilla.components.browser.session.action.Action
import mozilla.components.browser.session.action.BrowserAction
import mozilla.components.browser.session.action.SessionAction
import mozilla.components.browser.session.action.SessionListAction
import mozilla.components.browser.session.selector.findSession
import mozilla.components.browser.session.state.BrowserState
import mozilla.components.browser.session.state.SessionState
import mozilla.components.browser.session.store.BrowserStore
import mozilla.components.browser.session.store.Reducer

/**
 * Reducers for [BrowserStore].
 *
 * A reducer is a function that receives the current [BrowserState] and an [Action] and then returns a new
 * [BrowserState].
 */
internal object BrowserReducers {
    fun get(): List<Reducer> = listOf(
        ::reduce
    )

    private fun reduce(state: BrowserState, action: Action): BrowserState {
        if (action !is BrowserAction) {
            return state
        }

        return when (action) {
            is SessionListAction -> reduceSessionListAction(state, action)
            is SessionAction -> reduceSessionAction(state, action)
        }
    }

    /**
     * (SessionListAction) Reducer function for modifying the list of [SessionState] objects in [BrowserState.sessions].
     */
    private fun reduceSessionListAction(state: BrowserState, action: SessionListAction): BrowserState {
        return when (action) {
            is SessionListAction.AddSessionAction -> state.copy(
                    sessions = state.sessions + action.session,
                    selectedSessionId = if (action.select) action.session.id else state.selectedSessionId)
            is SessionListAction.SelectSessionAction -> state.copy(selectedSessionId = action.sessionId)
            is SessionListAction.RemoveSessionAction -> {
                val session = state.findSession(action.sessionId)
                if (session == null) {
                    state
                } else {
                    state.copy(sessions = state.sessions - session)
                }
            }
        }
    }

    /**
     * (SessionAction) Reducer function for modifying a specific [SessionState].
     */
    private fun reduceSessionAction(state: BrowserState, action: SessionAction): BrowserState {
        return when (action) {
            is SessionAction.UpdateUrlAction -> updateSessionInState(state, action.sessionId) {
                it.copy(url = action.url)
            }
            is SessionAction.UpdateProgressAction -> updateSessionInState(state, action.sessionId) {
                it.copy(progress = action.progress)
            }
            is SessionAction.CanGoBackAction -> updateSessionInState(state, action.sessionId) {
                it.copy(canGoBack = action.canGoBack)
            }
            is SessionAction.CanGoForward -> updateSessionInState(state, action.sessionId) {
                it.copy(canGoForward = action.canGoForward)
            }
            is SessionAction.AddHitResultAction -> updateSessionInState(state, action.sessionId) {
                it.copy(hitResult = action.hitResult)
            }
            is SessionAction.ConsumeHitResultAction -> updateSessionInState(state, action.sessionId) {
                it.copy(hitResult = null)
            }
        }
    }
}

/**
 * Helper for updating a specific [SessionState] inside a [BrowserState]
 */
private fun updateSessionInState(
    state: BrowserState,
    sessionId: String,
    update: (SessionState) -> SessionState
): BrowserState {
    return state.copy(sessions = state.sessions.map { current ->
        if (current.id == sessionId) {
            update.invoke(current)
        } else {
            current
        }
    })
}
