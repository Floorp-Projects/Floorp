/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

package org.mozilla.fenix.gleanplumb.state

import io.mockk.mockk
import org.junit.Assert.assertFalse
import org.junit.Assert.assertNotNull
import org.junit.Assert.assertNull
import org.junit.Assert.assertTrue
import org.junit.Test
import org.mozilla.fenix.components.appstate.AppAction.MessagingAction.ConsumeMessageToShow
import org.mozilla.fenix.components.appstate.AppAction.MessagingAction.UpdateMessageToShow
import org.mozilla.fenix.components.appstate.AppAction.MessagingAction.UpdateMessages
import org.mozilla.fenix.components.appstate.AppState
import org.mozilla.fenix.components.appstate.AppStoreReducer
import org.mozilla.fenix.gleanplumb.MessagingState

class MessagingReducerTest {

    @Test
    fun `GIVEN a new value for messageToShow WHEN UpdateMessageToShow is called THEN update the current value`() {
        val initialState = AppState(
            messaging = MessagingState(
                messageToShow = null
            )
        )

        var updatedState = MessagingReducer.reduce(
            initialState,
            UpdateMessageToShow(mockk())
        )

        assertNotNull(updatedState.messaging.messageToShow)

        updatedState = AppStoreReducer.reduce(updatedState, ConsumeMessageToShow)

        assertNull(updatedState.messaging.messageToShow)
    }

    @Test
    fun `GIVEN a new value for messages WHEN UpdateMessages is called THEN update the current value`() {
        val initialState = AppState(
            messaging = MessagingState(
                messages = emptyList()
            )
        )

        var updatedState = MessagingReducer.reduce(
            initialState,
            UpdateMessages(listOf(mockk()))
        )

        assertFalse(updatedState.messaging.messages.isEmpty())

        updatedState = AppStoreReducer.reduce(updatedState, UpdateMessages(emptyList()))

        assertTrue(updatedState.messaging.messages.isEmpty())
    }
}
