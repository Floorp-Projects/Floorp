/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

package org.mozilla.fenix.gleanplumb

import android.net.Uri
import io.mockk.every
import io.mockk.mockk
import io.mockk.spyk
import io.mockk.verify
import mozilla.components.support.test.robolectric.testContext
import mozilla.telemetry.glean.testing.GleanTestRule
import org.junit.Assert.assertEquals
import org.junit.Assert.assertFalse
import org.junit.Assert.assertTrue
import org.junit.Before
import org.junit.Rule
import org.junit.Test
import org.junit.runner.RunWith
import org.mozilla.fenix.BuildConfig
import org.mozilla.fenix.GleanMetrics.Messaging
import org.mozilla.fenix.HomeActivity
import org.mozilla.fenix.components.AppStore
import org.mozilla.fenix.components.appstate.AppAction
import org.mozilla.fenix.components.appstate.AppAction.MessagingAction.MessageClicked
import org.mozilla.fenix.components.appstate.AppAction.MessagingAction.MessageDisplayed
import org.mozilla.fenix.helpers.FenixRobolectricTestRunner
import org.mozilla.fenix.nimbus.MessageData

@RunWith(FenixRobolectricTestRunner::class)
class DefaultMessageControllerTest {

    @get:Rule
    val gleanTestRule = GleanTestRule(testContext)

    private val activity: HomeActivity = mockk(relaxed = true)
    private val storageNimbus: NimbusMessagingStorage = mockk(relaxed = true)
    private lateinit var controller: DefaultMessageController
    private val store: AppStore = mockk(relaxed = true)

    @Before
    fun setup() {
        controller = DefaultMessageController(
            messagingStorage = storageNimbus,
            appStore = store,
            homeActivity = activity
        )
    }

    @Test
    fun `WHEN calling onMessagePressed THEN update the store and handle the action`() {
        val customController = spyk(controller)
        val message = mockMessage()
        every { customController.handleAction(any()) } returns mockk()
        every { storageNimbus.getMessageAction(message) } returns Pair("uuid", message.id)
        assertFalse(Messaging.messageClicked.testHasValue())

        customController.onMessagePressed(message)

        assertTrue(Messaging.messageClicked.testHasValue())
        val event = Messaging.messageClicked.testGetValue()
        assertEquals(1, event.size)
        assertEquals(message.id, event.single().extra!!["message_key"])
        assertEquals("uuid", event.single().extra!!["action_uuid"])
        verify { customController.handleAction(any()) }
        verify { store.dispatch(MessageClicked(message)) }
    }

    @Test
    fun `GIVEN an URL WHEN calling handleAction THEN process the intent with an open uri`() {
        val intent = controller.handleAction("http://mozilla.org")

        verify { activity.processIntent(any()) }

        val encodedUrl = Uri.encode("http://mozilla.org")
        assertEquals(
            "${BuildConfig.DEEP_LINK_SCHEME}://open?url=$encodedUrl",
            intent.data.toString()
        )
    }

    @Test
    fun `GIVEN an deeplink WHEN calling handleAction THEN process the intent with an deeplink uri`() {
        val intent = controller.handleAction("://settings_privacy")

        verify { activity.processIntent(any()) }

        assertEquals("${BuildConfig.DEEP_LINK_SCHEME}://settings_privacy", intent.data.toString())
    }

    @Test
    fun `WHEN calling onMessageDismissed THEN report to the messageManager`() {
        val message = mockMessage()
        assertFalse(Messaging.messageDismissed.testHasValue())

        controller.onMessageDismissed(message)

        assertTrue(Messaging.messageDismissed.testHasValue())
        val event = Messaging.messageDismissed.testGetValue()
        assertEquals(1, event.size)
        assertEquals(message.id, event.single().extra!!["message_key"])
        verify { store.dispatch(AppAction.MessagingAction.MessageDismissed(message)) }
    }

    @Test
    fun `WHEN calling onMessageDisplayed THEN report to the messageManager`() {
        val data = MessageData()
        val message = mockMessage(data)
        assertFalse(Messaging.messageExpired.testHasValue())
        assertFalse(Messaging.messageShown.testHasValue())

        controller.onMessageDisplayed(message)

        assertTrue(Messaging.messageExpired.testHasValue())
        val messageExpiredEvent = Messaging.messageExpired.testGetValue()
        assertEquals(1, messageExpiredEvent.size)
        assertEquals(message.id, messageExpiredEvent.single().extra!!["message_key"])

        assertTrue(Messaging.messageShown.testHasValue())
        val event = Messaging.messageShown.testGetValue()
        assertEquals(1, event.size)
        assertEquals(message.id, event.single().extra!!["message_key"])

        verify { store.dispatch(MessageDisplayed(message)) }
    }

    private fun mockMessage(data: MessageData = MessageData()) = Message(
        id = "id",
        data = data,
        style = mockk(relaxed = true),
        action = "action",
        triggers = emptyList(),
        metadata = Message.Metadata(
            id = "id",
            displayCount = 0,
            pressed = false,
            dismissed = false
        )
    )
}
