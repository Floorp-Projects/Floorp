/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

package org.mozilla.fenix.gleanplumb.state

import org.mozilla.fenix.components.appstate.AppAction
import org.mozilla.fenix.components.appstate.AppAction.MessagingAction.ConsumeMessageToShow
import org.mozilla.fenix.components.appstate.AppAction.MessagingAction.UpdateMessageToShow
import org.mozilla.fenix.components.appstate.AppAction.MessagingAction.UpdateMessages
import org.mozilla.fenix.components.appstate.AppState
import org.mozilla.fenix.gleanplumb.MessagingState

/**
 * Reducer for [MessagingState].
 */
internal object MessagingReducer {
    fun reduce(state: AppState, action: AppAction.MessagingAction): AppState = when (action) {
        is UpdateMessageToShow -> {
            val messageToShow = state.messaging.messageToShow.toMutableMap()
            messageToShow[action.message.surface] = action.message
            state.copy(
                messaging = state.messaging.copy(
                    messageToShow = messageToShow,
                ),
            )
        }
        is UpdateMessages -> {
            state.copy(
                messaging = state.messaging.copy(
                    messages = action.messages,
                ),
            )
        }
        is ConsumeMessageToShow -> {
            val messageToShow = state.messaging.messageToShow.toMutableMap()
            messageToShow.remove(action.surface)
            state.copy(
                messaging = state.messaging.copy(
                    messageToShow = messageToShow,
                ),
            )
        }
        else -> state
    }
}
