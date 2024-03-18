/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

package mozilla.components.browser.state.reducer

import mozilla.components.browser.state.action.AwesomeBarAction
import mozilla.components.browser.state.state.AwesomeBarState
import mozilla.components.browser.state.state.BrowserState

/**
 * An [AwesomeBarAction] reducer that updates [BrowserState.awesomeBarState].
 */
internal object AwesomeBarStateReducer {
    fun reduce(state: BrowserState, action: AwesomeBarAction): BrowserState {
        return when (action) {
            is AwesomeBarAction.VisibilityStateUpdated -> {
                val awesomeBarState = state.awesomeBarState.copy(visibilityState = action.visibilityState)
                state.copy(awesomeBarState = awesomeBarState)
            }
            is AwesomeBarAction.SuggestionClicked -> {
                val awesomeBarState = state.awesomeBarState.copy(clickedSuggestion = action.suggestion)
                state.copy(awesomeBarState = awesomeBarState)
            }
            is AwesomeBarAction.EngagementFinished -> {
                state.copy(awesomeBarState = AwesomeBarState())
            }
        }
    }
}
