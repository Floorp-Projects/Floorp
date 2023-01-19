/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

package org.mozilla.fenix.gleanplumb

import android.net.Uri
import io.mockk.coVerify
import io.mockk.every
import io.mockk.mockk
import kotlinx.coroutines.test.runTest
import mozilla.components.support.test.robolectric.testContext
import mozilla.components.support.test.rule.MainCoroutineRule
import mozilla.telemetry.glean.testing.GleanTestRule
import org.junit.Assert.assertEquals
import org.junit.Assert.assertNotNull
import org.junit.Assert.assertNull
import org.junit.Before
import org.junit.Rule
import org.junit.Test
import org.junit.runner.RunWith
import org.mozilla.experiments.nimbus.NullVariables
import org.mozilla.fenix.BuildConfig
import org.mozilla.fenix.GleanMetrics.Messaging
import org.mozilla.fenix.helpers.FenixRobolectricTestRunner
import org.mozilla.fenix.nimbus.MessageData
import org.mozilla.fenix.nimbus.StyleData
import java.util.*

@RunWith(FenixRobolectricTestRunner::class)
class NimbusMessagingControllerTest {
    private val storage: NimbusMessagingStorage = mockk(relaxed = true)

    @get:Rule
    val gleanTestRule = GleanTestRule(testContext)

    private val coroutinesTestRule = MainCoroutineRule()
    private val coroutineScope = coroutinesTestRule.scope

    private val controller = NimbusMessagingController(storage) { 0L }

    @Before
    fun setup() {
        NullVariables.instance.setContext(testContext)
    }

    @Test
    fun `WHEN calling onMessageDismissed THEN record a messageDismissed event and updates metadata`() = coroutineScope.runTest {
        val message = createMessage("id-1")
        assertNull(Messaging.messageDismissed.testGetValue())

        controller.onMessageDismissed(message)

        assertNotNull(Messaging.messageDismissed.testGetValue())
        val event = Messaging.messageDismissed.testGetValue()!!
        assertEquals(1, event.size)
        assertEquals(message.id, event.single().extra!!["message_key"])

        coVerify { storage.updateMetadata(message.metadata.copy(dismissed = true)) }
    }

    @Test
    fun `WHEN calling processDisplayedMessage THEN record a messageDisplayed event and updates metadata`() = coroutineScope.runTest {
        val message = createMessage("id-1")
        assertNull(Messaging.messageShown.testGetValue())
        assertEquals(0, message.metadata.displayCount)

        val updated = controller.processDisplayedMessage(message)
        controller.onMessageDisplayed(updated)

        assertNotNull(Messaging.messageShown.testGetValue())
        val event = Messaging.messageShown.testGetValue()!!
        assertEquals(1, event.size)
        assertEquals(message.id, event.single().extra!!["message_key"])

        coVerify { storage.updateMetadata(message.metadata.copy(displayCount = 1)) }
        assertEquals(1, updated.metadata.displayCount)
    }

    @Test
    fun `WHEN calling processDisplayedMessage on an expiring message THEN record a messageExpired event`() = coroutineScope.runTest {
        val message = createMessage("id-1", style = StyleData(maxDisplayCount = 1))
        assertNull(Messaging.messageShown.testGetValue())
        assertEquals(0, message.metadata.displayCount)

        val updated = controller.processDisplayedMessage(message)
        controller.onMessageDisplayed(updated)

        assertNotNull(Messaging.messageShown.testGetValue())
        val shownEvent = Messaging.messageShown.testGetValue()!!
        assertEquals(1, shownEvent.size)
        assertEquals(message.id, shownEvent.single().extra!!["message_key"])

        coVerify { storage.updateMetadata(message.metadata.copy(displayCount = 1)) }
        assertEquals(1, updated.metadata.displayCount)

        assertNotNull(Messaging.messageExpired.testGetValue())
        val expiredEvent = Messaging.messageExpired.testGetValue()!!
        assertEquals(1, expiredEvent.size)
        assertEquals(message.id, expiredEvent.single().extra!!["message_key"])
    }

    @Test
    fun `GIVEN a URL WHEN calling createMessageAction THEN treat it as an open uri deeplink`() {
        val message = createMessage("id-1", action = "http://mozilla.org")
        every { storage.getMessageAction(any()) } returns Pair(null, message.action)

        val uri = controller.processMessageAction(message)

        val encodedUrl = Uri.encode("http://mozilla.org")
        assertEquals(
            "${BuildConfig.DEEP_LINK_SCHEME}://open?url=$encodedUrl",
            uri,
        )
    }

    @Test
    fun `GIVEN an deeplink WHEN calling createMessageAction THEN treat it as a deeplink`() {
        val message = createMessage("id-1", action = "://a-deep-link")
        every { storage.getMessageAction(any()) } returns Pair(null, message.action)

        val uri = controller.processMessageAction(message)

        assertEquals(
            "${BuildConfig.DEEP_LINK_SCHEME}://a-deep-link",
            uri,
        )
    }

    @Test
    fun `GIVEN a URL WHEN calling createMessageAction THEN record a messageClicked event`() {
        val message = createMessage("id-1", action = "http://mozilla.org")
        every { storage.getMessageAction(any()) } returns Pair(null, message.action)

        controller.processMessageAction(message)

        val clickedEvents = Messaging.messageClicked.testGetValue()
        assertNotNull(clickedEvents)
        val clickedEvent = clickedEvents!!.single()

        assertEquals(message.id, clickedEvent.extra!!["message_key"])
        assertEquals(null, clickedEvent.extra!!["action_uuid"])
    }

    @Test
    fun `GIVEN a URL with a {uuid} WHEN calling createMessageAction THEN record a messageClicked event with a uuid`() {
        val message = createMessage("id-1", action = "http://mozilla.org?uuid={uuid}")
        val uuid = UUID.randomUUID().toString()
        every { storage.getMessageAction(any()) } returns Pair(uuid, message.action)

        controller.processMessageAction(message)

        val clickedEvents = Messaging.messageClicked.testGetValue()
        assertNotNull(clickedEvents)
        val clickedEvent = clickedEvents!!.single()

        assertEquals(message.id, clickedEvent.extra!!["message_key"])
        assertEquals(uuid, clickedEvent.extra!!["action_uuid"])
    }

    private fun createMessage(
        id: String,
        messageData: MessageData = MessageData(),
        action: String = messageData.action,
        style: StyleData = StyleData(),
    ): Message =
        Message(
            id,
            data = messageData,
            style = style,
            metadata = Message.Metadata(id),
            triggers = emptyList(),
            action = action,
        )
}
