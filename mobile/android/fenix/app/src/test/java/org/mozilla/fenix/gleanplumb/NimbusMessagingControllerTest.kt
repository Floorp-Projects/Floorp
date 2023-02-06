/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

package org.mozilla.fenix.gleanplumb

import android.content.Intent
import android.net.Uri
import androidx.core.net.toUri
import io.mockk.coEvery
import io.mockk.coVerify
import io.mockk.every
import io.mockk.mockk
import kotlinx.coroutines.test.runTest
import mozilla.components.support.test.robolectric.testContext
import mozilla.components.support.test.rule.MainCoroutineRule
import mozilla.telemetry.glean.testing.GleanTestRule
import org.junit.Assert.assertEquals
import org.junit.Assert.assertFalse
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
import java.util.UUID

private const val MOCK_TIME_MILLIS = 1000L

@RunWith(FenixRobolectricTestRunner::class)
class NimbusMessagingControllerTest {
    private val storage: NimbusMessagingStorage = mockk(relaxed = true)

    @get:Rule
    val gleanTestRule = GleanTestRule(testContext)

    private val coroutinesTestRule = MainCoroutineRule()
    private val coroutineScope = coroutinesTestRule.scope

    private val controller = NimbusMessagingController(storage) { MOCK_TIME_MILLIS }

    @Before
    fun setup() {
        NullVariables.instance.setContext(testContext)
    }

    @Test
    fun `WHEN calling updateMessageAsDisplayed with message & no boot id THEN metadata for count and lastTimeShown is updated`() =
        coroutineScope.runTest {
            val message = createMessage("id-1")
            assertEquals(0, message.metadata.displayCount)
            assertEquals(0L, message.metadata.lastTimeShown)
            assertNull(message.metadata.latestBootIdentifier)

            val expectedMessage = with(message) {
                copy(
                    metadata = metadata.copy(
                        displayCount = 1,
                        lastTimeShown = MOCK_TIME_MILLIS,
                        latestBootIdentifier = null,
                    ),
                )
            }

            assertEquals(expectedMessage, controller.updateMessageAsDisplayed(message))
        }

    @Test
    fun `WHEN calling updateMessageAsDisplayed with message & boot id THEN metadata for count, lastTimeShown & latestBootIdentifier is updated`() =
        coroutineScope.runTest {
            val message = createMessage("id-1")
            assertEquals(0, message.metadata.displayCount)
            assertEquals(0L, message.metadata.lastTimeShown)
            assertNull(message.metadata.latestBootIdentifier)

            val bootId = "test boot id"
            val expectedMessage = with(message) {
                copy(
                    metadata = metadata.copy(
                        displayCount = 1,
                        lastTimeShown = MOCK_TIME_MILLIS,
                        latestBootIdentifier = bootId,
                    ),
                )
            }

            assertEquals(expectedMessage, controller.updateMessageAsDisplayed(message, bootId))
        }

    @Test
    fun `GIVEN message not expired WHEN calling onMessageDisplayed THEN record a messageShown event and update storage`() =
        coroutineScope.runTest {
            val message = createMessage("id-1", style = StyleData(maxDisplayCount = 1))
            // Assert telemetry is initially null
            assertNull(Messaging.messageShown.testGetValue())
            assertNull(Messaging.messageExpired.testGetValue())

            controller.onMessageDisplayed(message)

            // Shown telemetry
            assertNotNull(Messaging.messageShown.testGetValue())
            val shownEvent = Messaging.messageShown.testGetValue()!!
            assertEquals(1, shownEvent.size)
            assertEquals(message.id, shownEvent.single().extra!!["message_key"])

            // Expired telemetry
            assertNull(Messaging.messageExpired.testGetValue())

            coVerify { storage.updateMetadata(message.metadata) }
        }

    @Test
    fun `GIVEN message is expired WHEN calling onMessageDisplayed THEN record messageShown, messageExpired events and update storage`() =
        coroutineScope.runTest {
            val message =
                createMessage("id-1", style = StyleData(maxDisplayCount = 1), displayCount = 1)
            // Assert telemetry is initially null
            assertNull(Messaging.messageShown.testGetValue())
            assertNull(Messaging.messageExpired.testGetValue())

            controller.onMessageDisplayed(message)

            // Shown telemetry
            assertNotNull(Messaging.messageShown.testGetValue())
            val shownEvent = Messaging.messageShown.testGetValue()!!
            assertEquals(1, shownEvent.size)
            assertEquals(message.id, shownEvent.single().extra!!["message_key"])

            // Expired telemetry
            assertNotNull(Messaging.messageExpired.testGetValue())
            val expiredEvent = Messaging.messageExpired.testGetValue()!!
            assertEquals(1, expiredEvent.size)
            assertEquals(message.id, expiredEvent.single().extra!!["message_key"])

            coVerify { storage.updateMetadata(message.metadata) }
        }

    @Test
    fun `WHEN calling onMessageDismissed THEN record a messageDismissed event and update metadata`() =
        coroutineScope.runTest {
            val message = createMessage("id-1")
            assertNull(Messaging.messageDismissed.testGetValue())

            controller.onMessageDismissed(message.metadata)

            assertNotNull(Messaging.messageDismissed.testGetValue())
            val event = Messaging.messageDismissed.testGetValue()!!
            assertEquals(1, event.size)
            assertEquals(message.id, event.single().extra!!["message_key"])

            coVerify { storage.updateMetadata(message.metadata.copy(dismissed = true)) }
        }

    @Test
    fun `GIVEN action is URL WHEN calling processMessageActionToUri THEN record a clicked telemetry event and return an open URI`() {
        val url = "http://mozilla.org"
        val message = createMessage("id-1", action = url)
        every { storage.generateUuidAndFormatAction(message.action) } returns Pair(
            null,
            message.action,
        )
        // Assert telemetry is initially null
        assertNull(Messaging.messageClicked.testGetValue())

        val encodedUrl = Uri.encode(url)
        val expectedUri = "${BuildConfig.DEEP_LINK_SCHEME}://open?url=$encodedUrl".toUri()

        val actualUri = controller.processMessageActionToUri(message)

        // Updated telemetry
        assertNotNull(Messaging.messageClicked.testGetValue())
        val clickedEvent = Messaging.messageClicked.testGetValue()!!
        assertEquals(1, clickedEvent.size)
        assertEquals(message.id, clickedEvent.single().extra!!["message_key"])

        assertEquals(expectedUri, actualUri)
    }

    @Test
    fun `GIVEN a URL with a {uuid} WHEN calling processMessageActionToUri THEN record a clicked telemetry event and return an open URI`() {
        val url = "http://mozilla.org?uuid={uuid}"
        val message = createMessage("id-1", action = url)
        val uuid = UUID.randomUUID().toString()
        every { storage.generateUuidAndFormatAction(any()) } returns Pair(uuid, message.action)

        // Assert telemetry is initially null
        assertNull(Messaging.messageClicked.testGetValue())

        val encodedUrl = Uri.encode(url)
        val expectedUri = "${BuildConfig.DEEP_LINK_SCHEME}://open?url=$encodedUrl".toUri()

        val actualUri = controller.processMessageActionToUri(message)

        // Updated telemetry
        val clickedEvents = Messaging.messageClicked.testGetValue()
        assertNotNull(clickedEvents)
        val clickedEvent = clickedEvents!!.single()
        assertEquals(message.id, clickedEvent.extra!!["message_key"])
        assertEquals(uuid, clickedEvent.extra!!["action_uuid"])

        assertEquals(expectedUri, actualUri)
    }

    @Test
    fun `GIVEN action is deeplink WHEN calling processMessageActionToUri THEN return a deeplink URI`() {
        val message = createMessage("id-1", action = "://a-deep-link")
        every { storage.generateUuidAndFormatAction(message.action) } returns Pair(
            null,
            message.action,
        )
        // Assert telemetry is initially null
        assertNull(Messaging.messageClicked.testGetValue())

        val expectedUri = "${BuildConfig.DEEP_LINK_SCHEME}${message.action}".toUri()
        val actualUri = controller.processMessageActionToUri(message)

        // Updated telemetry
        assertNotNull(Messaging.messageClicked.testGetValue())
        val clickedEvent = Messaging.messageClicked.testGetValue()!!
        assertEquals(1, clickedEvent.size)
        assertEquals(message.id, clickedEvent.single().extra!!["message_key"])

        assertEquals(expectedUri, actualUri)
    }

    @Test
    fun `GIVEN action unknown format WHEN calling processMessageActionToUri THEN return the action URI`() {
        val message = createMessage("id-1", action = "unknown")
        every { storage.generateUuidAndFormatAction(message.action) } returns Pair(
            null,
            message.action,
        )
        // Assert telemetry is initially null
        assertNull(Messaging.messageClicked.testGetValue())

        val expectedUri = message.action.toUri()
        val actualUri = controller.processMessageActionToUri(message)

        // Updated telemetry
        assertNotNull(Messaging.messageClicked.testGetValue())
        val clickedEvent = Messaging.messageClicked.testGetValue()!!
        assertEquals(1, clickedEvent.size)
        assertEquals(message.id, clickedEvent.single().extra!!["message_key"])

        assertEquals(expectedUri, actualUri)
    }

    @Test
    fun `WHEN calling onMessageClicked THEN update stored metadata for message`() =
        coroutineScope.runTest {
            val message = createMessage("id-1")
            assertFalse(message.metadata.pressed)

            controller.onMessageClicked(message.metadata)

            val updatedMetadata = message.metadata.copy(pressed = true)
            coVerify { storage.updateMetadata(updatedMetadata) }
        }

    @Test
    fun `WHEN getIntentForMessageAction is called THEN return a generated Intent with the processed Message action`() {
        val message = createMessage("id-1", action = "unknown")
        every { storage.generateUuidAndFormatAction(message.action) } returns Pair(
            null,
            message.action,
        )

        val actualIntent = controller.getIntentForMessageAction(message.action)

        // The processed Intent data
        assertEquals(Intent.ACTION_VIEW, actualIntent.action)
        val expectedUri = message.action.toUri()
        assertEquals(expectedUri, actualIntent.data)
    }

    @Test
    fun `GIVEN stored messages contains a matching message WHEN calling getMessage THEN return the matching message`() =
        coroutineScope.runTest {
            val message1 = createMessage("1")
            val message2 = createMessage("2")
            val message3 = createMessage("3")
            val messages = listOf(message1, message2, message3)
            coEvery { storage.getMessages() }.returns(messages)

            val actualMessage = controller.getMessage(message2.id)

            assertEquals(message2, actualMessage)
        }

    @Test
    fun `GIVEN stored messages contains multiple matching messages WHEN calling getMessage THEN return the first matching message`() =
        coroutineScope.runTest {
            val id = "same id"
            val message1 = createMessage(id)
            val message2 = createMessage(id)
            val message3 = createMessage(id)
            val messages = listOf(message1, message2, message3)
            coEvery { storage.getMessages() }.returns(messages)

            val actualMessage = controller.getMessage(id)

            assertEquals(message1, actualMessage)
        }

    @Test
    fun `GIVEN stored messages doesn't contain a matching message WHEN calling getMessage THEN return null`() =
        coroutineScope.runTest {
            val message1 = createMessage("1")
            val message2 = createMessage("2")
            val message3 = createMessage("3")
            val messages = listOf(message1, message2, message3)
            coEvery { storage.getMessages() }.returns(messages)

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
            triggers = emptyList(),
            action = action,
        )
}
