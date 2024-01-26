/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

package mozilla.components.service.nimbus.messaging

import android.content.Intent
import android.net.Uri
import androidx.core.net.toUri
import kotlinx.coroutines.test.runTest
import mozilla.components.service.glean.testing.GleanTestRule
import mozilla.components.support.test.any
import mozilla.components.support.test.eq
import mozilla.components.support.test.robolectric.testContext
import mozilla.components.support.test.rule.MainCoroutineRule
import org.junit.Assert.assertEquals
import org.junit.Assert.assertFalse
import org.junit.Assert.assertNotNull
import org.junit.Assert.assertNull
import org.junit.Before
import org.junit.Rule
import org.junit.Test
import org.junit.runner.RunWith
import org.mockito.Mockito.mock
import org.mockito.Mockito.verify
import org.mockito.Mockito.`when`
import org.mozilla.experiments.nimbus.NullVariables
import org.robolectric.RobolectricTestRunner
import java.util.UUID
import mozilla.components.service.nimbus.GleanMetrics.Messaging as GleanMessaging

@RunWith(RobolectricTestRunner::class)
class NimbusMessagingControllerTest {

    private val storage: NimbusMessagingStorage = mock(NimbusMessagingStorage::class.java)

    @get:Rule
    val gleanTestRule = GleanTestRule(testContext)

    private val coroutinesTestRule = MainCoroutineRule()
    private val coroutineScope = coroutinesTestRule.scope

    private val deepLinkScheme = "deepLinkScheme"
    private val controller = NimbusMessagingController(storage, deepLinkScheme)

    @Before
    fun setup() {
        NullVariables.instance.setContext(testContext)
    }

    @Test
    fun `GIVEN message not expired WHEN calling onMessageDisplayed THEN record a messageShown event and update storage`() =
        coroutineScope.runTest {
            val message = createMessage("id-1", style = StyleData(maxDisplayCount = 2))
            val displayedMessage = createMessage("id-1", style = StyleData(maxDisplayCount = 2), displayCount = 1)
            `when`(storage.onMessageDisplayed(eq(message), any())).thenReturn(displayedMessage)

            // Assert telemetry is initially null
            assertNull(GleanMessaging.messageShown.testGetValue())
            assertNull(GleanMessaging.messageExpired.testGetValue())

            controller.onMessageDisplayed(message)

            // Shown telemetry
            assertNotNull(GleanMessaging.messageShown.testGetValue())
            val shownEvent = GleanMessaging.messageShown.testGetValue()!!
            assertEquals(1, shownEvent.size)
            assertEquals(message.id, shownEvent.single().extra!!["message_key"])

            // Expired telemetry
            assertNull(GleanMessaging.messageExpired.testGetValue())

            verify(storage).onMessageDisplayed(eq(message), any())
        }

    @Test
    fun `GIVEN message is expired WHEN calling onMessageDisplayed THEN record messageShown, messageExpired events and update storage`() =
        coroutineScope.runTest {
            val message =
                createMessage("id-1", style = StyleData(maxDisplayCount = 1), displayCount = 0)
            val displayedMessage = createMessage("id-1", style = StyleData(maxDisplayCount = 1), displayCount = 1)
            `when`(storage.onMessageDisplayed(any(), any())).thenReturn(displayedMessage)
            // Assert telemetry is initially null
            assertNull(GleanMessaging.messageShown.testGetValue())
            assertNull(GleanMessaging.messageExpired.testGetValue())

            controller.onMessageDisplayed(message)

            // Shown telemetry
            assertNotNull(GleanMessaging.messageShown.testGetValue())
            val shownEvent = GleanMessaging.messageShown.testGetValue()!!
            assertEquals(1, shownEvent.size)
            assertEquals(message.id, shownEvent.single().extra!!["message_key"])

            // Expired telemetry
            assertNotNull(GleanMessaging.messageExpired.testGetValue())
            val expiredEvent = GleanMessaging.messageExpired.testGetValue()!!
            assertEquals(1, expiredEvent.size)
            assertEquals(message.id, expiredEvent.single().extra!!["message_key"])

            verify(storage).onMessageDisplayed(message)
        }

    @Test
    fun `WHEN calling onMessageDismissed THEN record a messageDismissed event and update metadata`() =
        coroutineScope.runTest {
            val message = createMessage("id-1")
            assertNull(GleanMessaging.messageDismissed.testGetValue())

            controller.onMessageDismissed(message)

            assertNotNull(GleanMessaging.messageDismissed.testGetValue())
            val event = GleanMessaging.messageDismissed.testGetValue()!!
            assertEquals(1, event.size)
            assertEquals(message.id, event.single().extra!!["message_key"])

            verify(storage).updateMetadata(message.metadata.copy(dismissed = true))
        }

    @Test
    fun `GIVEN action is URL WHEN calling processMessageActionToUri THEN record a clicked telemetry event and return an open URI`() {
        val url = "http://mozilla.org"
        val message = createMessage("id-1", action = url)

        `when`(storage.generateUuidAndFormatAction(message.action))
            .thenReturn(Pair(null, message.action))

        // Assert telemetry is initially null
        assertNull(GleanMessaging.messageClicked.testGetValue())

        val encodedUrl = Uri.encode(url)
        val expectedUri = "$deepLinkScheme://open?url=$encodedUrl".toUri()

        val actualUri = controller.processMessageActionToUri(message)

        // Updated telemetry
        assertNotNull(GleanMessaging.messageClicked.testGetValue())
        val clickedEvent = GleanMessaging.messageClicked.testGetValue()!!
        assertEquals(1, clickedEvent.size)
        assertEquals(message.id, clickedEvent.single().extra!!["message_key"])

        assertEquals(expectedUri, actualUri)
    }

    @Test
    fun `GIVEN a URL with a {uuid} WHEN calling processMessageActionToUri THEN record a clicked telemetry event and return an open URI`() {
        val url = "http://mozilla.org?uuid={uuid}"
        val message = createMessage("id-1", action = url)
        val uuid = UUID.randomUUID().toString()
        `when`(storage.generateUuidAndFormatAction(any())).thenReturn(Pair(uuid, message.action))

        // Assert telemetry is initially null
        assertNull(GleanMessaging.messageClicked.testGetValue())

        val encodedUrl = Uri.encode(url)
        val expectedUri = "$deepLinkScheme://open?url=$encodedUrl".toUri()

        val actualUri = controller.processMessageActionToUri(message)

        // Updated telemetry
        val clickedEvents = GleanMessaging.messageClicked.testGetValue()
        assertNotNull(clickedEvents)
        val clickedEvent = clickedEvents!!.single()
        assertEquals(message.id, clickedEvent.extra!!["message_key"])
        assertEquals(uuid, clickedEvent.extra!!["action_uuid"])

        assertEquals(expectedUri, actualUri)
    }

    @Test
    fun `GIVEN action is deeplink WHEN calling processMessageActionToUri THEN return a deeplink URI`() {
        val message = createMessage("id-1", action = "://a-deep-link")
        `when`(storage.generateUuidAndFormatAction(message.action))
            .thenReturn(Pair(null, message.action))

        // Assert telemetry is initially null
        assertNull(GleanMessaging.messageClicked.testGetValue())

        val expectedUri = "$deepLinkScheme${message.action}".toUri()
        val actualUri = controller.processMessageActionToUri(message)

        // Updated telemetry
        assertNotNull(GleanMessaging.messageClicked.testGetValue())
        val clickedEvent = GleanMessaging.messageClicked.testGetValue()!!
        assertEquals(1, clickedEvent.size)
        assertEquals(message.id, clickedEvent.single().extra!!["message_key"])

        assertEquals(expectedUri, actualUri)
    }

    @Test
    fun `GIVEN action unknown format WHEN calling processMessageActionToUri THEN return the action URI`() {
        val message = createMessage("id-1", action = "unknown")
        `when`(storage.generateUuidAndFormatAction(message.action))
            .thenReturn(Pair(null, message.action))

        // Assert telemetry is initially null
        assertNull(GleanMessaging.messageClicked.testGetValue())

        val expectedUri = message.action.toUri()
        val actualUri = controller.processMessageActionToUri(message)

        // Updated telemetry
        assertNotNull(GleanMessaging.messageClicked.testGetValue())
        val clickedEvent = GleanMessaging.messageClicked.testGetValue()!!
        assertEquals(1, clickedEvent.size)
        assertEquals(message.id, clickedEvent.single().extra!!["message_key"])

        assertEquals(expectedUri, actualUri)
    }

    @Test
    fun `WHEN calling onMessageClicked THEN update stored metadata for message`() =
        coroutineScope.runTest {
            val message = createMessage("id-1")
            assertFalse(message.metadata.pressed)

            controller.onMessageClicked(message)

            val updatedMetadata = message.metadata.copy(pressed = true)
            verify(storage).updateMetadata(updatedMetadata)
        }

    @Test
    fun `WHEN getIntentForMessageAction is called THEN return a generated Intent with the processed Message action`() {
        val message = createMessage("id-1", action = "unknown")
        `when`(storage.generateUuidAndFormatAction(message.action))
            .thenReturn(Pair(null, message.action))
        assertNull(GleanMessaging.messageClicked.testGetValue())

        val actualIntent = controller.getIntentForMessage(message)

        // Updated telemetry
        assertNotNull(GleanMessaging.messageClicked.testGetValue())
        val event = GleanMessaging.messageClicked.testGetValue()!!
        assertEquals(1, event.size)
        assertEquals(message.id, event.single().extra!!["message_key"])

        // The processed Intent data
        assertEquals(Intent.ACTION_VIEW, actualIntent.action)
        val expectedUri = message.action.toUri()
        assertEquals(expectedUri, actualIntent.data)
    }

    @Test
    fun `GIVEN stored messages contains a matching message WHEN calling getMessage THEN return the matching message`() =
        coroutineScope.runTest {
            val message1 = createMessage("1")
            `when`(storage.getMessage(message1.id)).thenReturn(message1)
            val actualMessage = controller.getMessage(message1.id)

            assertEquals(message1, actualMessage)
        }

    @Test
    fun `GIVEN stored messages doesn't contain a matching message WHEN calling getMessage THEN return null`() =
        coroutineScope.runTest {
            `when`(storage.getMessage("unknown id")).thenReturn(null)
            val actualMessage = controller.getMessage("unknown id")

            assertNull(actualMessage)
        }

    private fun createMessage(
        id: String,
        messageData: MessageData = MessageData(),
        action: String = messageData.action,
        style: StyleData = StyleData(),
        displayCount: Int = 0,
    ): Message =
        Message(
            id,
            data = messageData,
            style = style,
            metadata = Message.Metadata(id, displayCount),
            triggerIfAll = emptyList(),
            excludeIfAny = emptyList(),
            action = action,
        )
}
