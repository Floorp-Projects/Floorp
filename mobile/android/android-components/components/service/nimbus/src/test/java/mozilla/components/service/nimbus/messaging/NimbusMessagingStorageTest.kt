/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

package mozilla.components.service.nimbus.messaging

import kotlinx.coroutines.runBlocking
import kotlinx.coroutines.test.runTest
import mozilla.components.service.nimbus.messaging.ControlMessageBehavior.SHOW_NEXT_MESSAGE
import mozilla.components.support.test.any
import mozilla.components.support.test.mock
import mozilla.components.support.test.robolectric.testContext
import mozilla.components.support.test.rule.MainCoroutineRule
import org.junit.After
import org.junit.Assert.assertEquals
import org.junit.Assert.assertFalse
import org.junit.Assert.assertNotNull
import org.junit.Assert.assertNull
import org.junit.Assert.assertTrue
import org.junit.Before
import org.junit.Rule
import org.junit.Test
import org.junit.runner.RunWith
import org.mockito.Mock
import org.mockito.Mockito.doReturn
import org.mockito.Mockito.never
import org.mockito.Mockito.spy
import org.mockito.Mockito.times
import org.mockito.Mockito.verify
import org.mockito.Mockito.`when`
import org.mockito.MockitoAnnotations
import org.mozilla.experiments.nimbus.FeaturesInterface
import org.mozilla.experiments.nimbus.GleanPlumbInterface
import org.mozilla.experiments.nimbus.GleanPlumbMessageHelper
import org.mozilla.experiments.nimbus.NullVariables
import org.mozilla.experiments.nimbus.Res
import org.mozilla.experiments.nimbus.internal.FeatureHolder
import org.mozilla.experiments.nimbus.internal.NimbusException
import org.robolectric.RobolectricTestRunner

@RunWith(RobolectricTestRunner::class)
@kotlinx.coroutines.ExperimentalCoroutinesApi
class NimbusMessagingStorageTest {
    @Mock private lateinit var metadataStorage: MessageMetadataStorage

    @Mock private lateinit var gleanPlumb: GleanPlumbInterface

    private lateinit var storage: NimbusMessagingStorage
    private lateinit var messagingFeature: FeatureHolder<Messaging>
    private var malformedWasReported = false
    private var malformedMessageIds = mutableSetOf<String>()
    private val reportMalformedMessage: (String) -> Unit = {
        malformedMessageIds.add(it)
        malformedWasReported = true
    }
    private lateinit var featuresInterface: FeaturesInterface

    @get:Rule
    val coroutinesTestRule = MainCoroutineRule()

    @Before
    fun setup() {
        MockitoAnnotations.openMocks(this)
        NullVariables.instance.setContext(testContext)

        val (messagingFeatureTmp, featuresInterfaceTmp) = createMessagingFeature()

        messagingFeature = spy(messagingFeatureTmp)
        featuresInterface = featuresInterfaceTmp

        runBlocking {
            `when`(metadataStorage.getMetadata())
                .thenReturn(mapOf("message-1" to Message.Metadata(id = "message-1")))

            `when`(metadataStorage.addMetadata(any()))
                .thenReturn(mock())
        }

        storage = NimbusMessagingStorage(
            testContext,
            metadataStorage,
            reportMalformedMessage,
            gleanPlumb,
            messagingFeature,
        )
    }

    @After
    fun tearDown() {
        malformedWasReported = false
        malformedMessageIds.clear()
    }

    @Test
    fun `WHEN calling getCoenrollingFeatureIds THEN messaging is in that list`() {
        assertTrue(FxNimbusMessaging.getCoenrollingFeatureIds().contains(MESSAGING_FEATURE_ID))
    }

    @Test
    fun `WHEN calling getMessages THEN provide a list of available messages for a given surface`() =
        runTest {
            val homescreenMessage = storage.getMessages().first()

            assertEquals("message-1", homescreenMessage.id)
            assertEquals("message-1", homescreenMessage.metadata.id)

            val notificationMessage = storage.getMessages().last()
            assertEquals("message-2", notificationMessage.id)
        }

    @Test
    fun `WHEN calling getMessages THEN provide a list of sorted messages by priority`() =
        runTest {
            val messages = mapOf(
                "low-message" to createMessageData(style = "low-priority"),
                "high-message" to createMessageData(style = "high-priority"),
                "medium-message" to createMessageData(style = "medium-priority"),
            )
            val styles = mapOf(
                "high-priority" to createStyle(priority = 100),
                "medium-priority" to createStyle(priority = 50),
                "low-priority" to createStyle(priority = 1),
            )
            val (messagingFeature, _) = createMessagingFeature(
                styles = styles,
                messages = messages,
            )

            val storage = NimbusMessagingStorage(
                testContext,
                metadataStorage,
                reportMalformedMessage,
                gleanPlumb,
                messagingFeature,
            )

            val results = storage.getMessages()

            assertEquals("high-message", results[0].id)
            assertEquals("medium-message", results[1].id)
            assertEquals("low-message", results[2].id)
        }

    @Test
    fun `GIVEN pressed message WHEN calling getMessages THEN filter out the pressed message`() =
        runTest {
            val metadataList = mapOf(
                "pressed-message" to Message.Metadata(id = "pressed-message", pressed = true),
                "normal-message" to Message.Metadata(id = "normal-message", pressed = false),
            )
            val messages = mapOf(
                "pressed-message" to createMessageData(style = "high-priority"),
                "normal-message" to createMessageData(style = "high-priority"),
            )
            val styles = mapOf(
                "high-priority" to createStyle(priority = 100),
            )
            val metadataStorage: MessageMetadataStorage = mock()
            val (messagingFeature, _) = createMessagingFeature(
                styles = styles,
                messages = messages,
            )

            `when`(metadataStorage.getMetadata()).thenReturn(metadataList)

            val storage = NimbusMessagingStorage(
                testContext,
                metadataStorage,
                reportMalformedMessage,
                gleanPlumb,
                messagingFeature,
            )

            val results = storage.getMessages()

            assertEquals(1, results.size)
            assertEquals("normal-message", results[0].id)
        }

    @Test
    fun `GIVEN dismissed message WHEN calling getMessages THEN filter out the dismissed message`() =
        runTest {
            val metadataList = mapOf(
                "dismissed-message" to Message.Metadata(id = "dismissed-message", dismissed = true),
                "normal-message" to Message.Metadata(id = "normal-message", dismissed = false),
            )
            val messages = mapOf(
                "dismissed-message" to createMessageData(style = "high-priority"),
                "normal-message" to createMessageData(style = "high-priority"),
            )
            val styles = mapOf(
                "high-priority" to createStyle(priority = 100),
            )
            val metadataStorage: MessageMetadataStorage = mock()
            val (messagingFeature, _) = createMessagingFeature(
                styles = styles,
                messages = messages,
            )

            `when`(metadataStorage.getMetadata()).thenReturn(metadataList)

            val storage = NimbusMessagingStorage(
                testContext,
                metadataStorage,
                reportMalformedMessage,
                gleanPlumb,
                messagingFeature,
            )

            val results = storage.getMessages()

            assertEquals(1, results.size)
            assertEquals("normal-message", results[0].id)
        }

    @Test
    fun `GIVEN a message that the maxDisplayCount WHEN calling getMessages THEN filter out the message`() =
        runTest {
            val metadataList = mapOf(
                "shown-many-times-message" to Message.Metadata(
                    id = "shown-many-times-message",
                    displayCount = 10,
                ),
                "shown-two-times-message" to Message.Metadata(
                    id = "shown-two-times-message",
                    displayCount = 2,
                ),
                "normal-message" to Message.Metadata(id = "normal-message", displayCount = 0),
            )
            val messages = mapOf(
                "shown-many-times-message" to createMessageData(
                    style = "high-priority",
                ),
                "shown-two-times-message" to createMessageData(
                    style = "high-priority",
                ),
                "normal-message" to createMessageData(style = "high-priority"),
            )
            val styles = mapOf(
                "high-priority" to createStyle(priority = 100, maxDisplayCount = 2),
            )
            val metadataStorage: MessageMetadataStorage = mock()
            val (messagingFeature, _) = createMessagingFeature(
                styles = styles,
                messages = messages,
            )

            `when`(metadataStorage.getMetadata()).thenReturn(metadataList)

            val storage = NimbusMessagingStorage(
                testContext,
                metadataStorage,
                reportMalformedMessage,
                gleanPlumb,
                messagingFeature,
            )

            val results = storage.getMessages()

            assertEquals(1, results.size)
            assertEquals("normal-message", results[0].id)
        }

    @Test
    fun `GIVEN a malformed message WHEN calling getMessages THEN provide a list of messages ignoring the malformed one`() =
        runTest {
            val messages = storage.getMessages()
            val firstMessage = messages.first()

            assertEquals("message-1", firstMessage.id)
            assertEquals("message-1", firstMessage.metadata.id)
            assertTrue(messages.size == 2)
            assertTrue(malformedWasReported)
        }

    @Test
    fun `GIVEN a malformed action WHEN calling sanitizeAction THEN return null`() {
        val actionsMap = mapOf("action-1" to "action-1-url")

        val notFoundAction =
            storage.sanitizeAction("messageId", "no-found-action", actionsMap, false)
        val emptyAction = storage.sanitizeAction("messageId", "", actionsMap, false)
        val blankAction = storage.sanitizeAction("messageId", " ", actionsMap, false)

        assertNull(notFoundAction)
        assertNull(emptyAction)
        assertNull(blankAction)
        assertTrue(malformedWasReported)
    }

    @Test
    fun `GIVEN a previously stored malformed action WHEN calling sanitizeAction THEN return null and not report malFormed`() {
        val actionsMap = mapOf("action-1" to "action-1-url")

        storage.malFormedMap["malformed-action"] = "messageId"

        val action = storage.sanitizeAction("messageId", "malformed-action", actionsMap, false)

        assertNull(action)
        assertFalse(malformedWasReported)
    }

    @Test
    fun `GIVEN a non-previously stored malformed action WHEN calling sanitizeAction THEN return null and report malFormed`() {
        val actionsMap = mapOf("action-1" to "action-1-url")

        val action = storage.sanitizeAction("messageId", "malformed-action", actionsMap, false)

        assertNull(action)
        assertTrue(storage.malFormedMap.containsKey("malformed-action"))
        assertTrue(malformedWasReported)
    }

    @Test
    fun `WHEN calling updateMetadata THEN delegate to metadataStorage`() = runTest {
        storage.updateMetadata(mock())

        verify(metadataStorage).updateMetadata(any())
    }

    @Test
    fun `GIVEN a valid action WHEN calling sanitizeAction THEN return the action`() {
        val actionsMap = mapOf("action-1" to "action-1-url")

        val validAction = storage.sanitizeAction("messageId", "action-1", actionsMap, false)

        assertEquals("action-1-url", validAction)
    }

    @Test
    fun `GIVEN a valid action for control message WHEN calling sanitizeAction THEN return a empty action`() {
        val actionsMap = mapOf("action-1" to "action-1-url")

        val validAction = storage.sanitizeAction("messageId", "", actionsMap, true)

        assertEquals("CONTROL_ACTION", validAction)
        assertFalse(malformedWasReported)
    }

    @Test
    fun `GIVEN a trigger action WHEN calling sanitizeTriggers THEN return null`() {
        val triggersMap = mapOf("trigger-1" to "trigger-1-expression")

        val notFoundTrigger =
            storage.sanitizeTriggers("messageId", listOf("no-found-trigger"), triggersMap)
        val emptyTrigger = storage.sanitizeTriggers("messageId", listOf(""), triggersMap)
        val blankTrigger = storage.sanitizeTriggers("messageId", listOf(" "), triggersMap)

        assertNull(notFoundTrigger)
        assertNull(emptyTrigger)
        assertNull(blankTrigger)
        assertTrue(malformedWasReported)
    }

    @Test
    fun `GIVEN a previously stored malformed trigger WHEN calling sanitizeTriggers THEN no report malformed and return null`() {
        val triggersMap = mapOf("trigger-1" to "trigger-1-expression")

        storage.malFormedMap[" "] = "messageId"

        val trigger = storage.sanitizeTriggers("messageId", listOf(" "), triggersMap)

        assertNull(trigger)
        assertFalse(malformedWasReported)
    }

    @Test
    fun `GIVEN a non previously stored malformed trigger WHEN calling sanitizeTriggers THEN report malformed and return null`() {
        val triggersMap = mapOf("trigger-1" to "trigger-1-expression")

        val trigger = storage.sanitizeTriggers("messageId", listOf(" "), triggersMap)

        assertNull(trigger)
        assertTrue(storage.malFormedMap.containsKey(" "))
        assertTrue(malformedWasReported)
    }

    @Test
    fun `GIVEN a valid trigger WHEN calling sanitizeAction THEN return the trigger`() {
        val triggersMap = mapOf("trigger-1" to "trigger-1-expression")

        val validTrigger = storage.sanitizeTriggers("messageId", listOf("trigger-1"), triggersMap)

        assertEquals(listOf("trigger-1-expression"), validTrigger)
    }

    @Test
    fun `GIVEN an eligible message WHEN calling isMessageEligible THEN return true`() {
        val helper: GleanPlumbMessageHelper = mock()
        val message = Message(
            "same-id",
            mock(),
            action = "action",
            mock(),
            listOf("trigger"),
            Message.Metadata("same-id"),
        )

        `when`(helper.evalJexl(any())).thenReturn(true)

        val result = storage.isMessageEligible(message, helper)

        assertTrue(result)
    }

    @Test
    fun `GIVEN a malformed message key WHEN calling reportMalformedMessage THEN record a malformed feature event`() {
        val key = "malformed-message"
        storage.reportMalformedMessage(key)

        assertTrue(malformedWasReported)
        verify(featuresInterface).recordMalformedConfiguration("messaging", key)
    }

    @Test
    fun `GIVEN a malformed trigger WHEN calling isMessageEligible THEN return false`() {
        val helper: GleanPlumbMessageHelper = mock()
        val message = Message(
            "same-id",
            mock(),
            action = "action",
            mock(),
            listOf("trigger"),
            Message.Metadata("same-id"),
        )

        `when`(helper.evalJexl(any())).then { throw NimbusException.EvaluationException("") }

        val result = storage.isMessageEligible(message, helper)

        assertFalse(result)
    }

    @Test
    fun `GIVEN a previously malformed trigger WHEN calling isMessageEligible THEN return false and not evaluate`() {
        val helper: GleanPlumbMessageHelper = mock()
        val message = Message(
            "same-id",
            mock(),
            action = "action",
            mock(),
            listOf("trigger"),
            Message.Metadata("same-id"),
        )

        storage.malFormedMap["trigger"] = "same-id"

        `when`(helper.evalJexl(any())).then { throw NimbusException.EvaluationException("") }

        val result = storage.isMessageEligible(message, helper)

        assertFalse(result)
        verify(helper, never()).evalJexl("trigger")
        assertFalse(malformedWasReported)
    }

    @Test
    fun `GIVEN a non previously malformed trigger WHEN calling isMessageEligible THEN return false and not evaluate`() {
        val helper: GleanPlumbMessageHelper = mock()
        val message = Message(
            "same-id",
            mock(),
            action = "action",
            mock(),
            listOf("trigger"),
            Message.Metadata("same-id"),
        )

        `when`(helper.evalJexl(any())).then { throw NimbusException.EvaluationException("") }

        assertFalse(storage.malFormedMap.containsKey("trigger"))

        val result = storage.isMessageEligible(message, helper)

        assertFalse(result)
        assertTrue(storage.malFormedMap.containsKey("trigger"))
        assertTrue(malformedWasReported)
    }

    @Test
    fun `GIVEN none available messages are eligible WHEN calling getNextMessage THEN return null`() {
        val spiedStorage = spy(storage)
        val message = Message(
            "same-id",
            mock(),
            action = "action",
            mock(),
            listOf("trigger"),
            Message.Metadata("same-id"),
        )

        doReturn(false).`when`(spiedStorage).isMessageEligible(any(), any(), any())

        val result = spiedStorage.getNextMessage(HOMESCREEN, listOf(message))

        assertNull(result)
    }

    @Test
    fun `GIVEN an eligible message WHEN calling getNextMessage THEN return the message`() {
        val spiedStorage = spy(storage)
        val message = Message(
            "same-id",
            createMessageData(surface = HOMESCREEN),
            action = "action",
            mock(),
            listOf("trigger"),
            Message.Metadata("same-id"),
        )

        doReturn(true).`when`(spiedStorage).isMessageEligible(any(), any(), any())

        val result = spiedStorage.getNextMessage(HOMESCREEN, listOf(message))

        assertEquals(message.id, result!!.id)
    }

    @Test
    fun `GIVEN a message under experiment WHEN calling getNextMessage THEN call recordExposureEvent`() {
        val spiedStorage = spy(storage)
        val experiment = "my-experiment"
        val messageData: MessageData = createMessageData(isControl = false, experiment = experiment)

        val message = Message(
            "same-id",
            messageData,
            action = "action",
            mock(),
            listOf("trigger"),
            Message.Metadata("same-id"),
        )

        doReturn(true).`when`(spiedStorage).isMessageEligible(any(), any(), any())

        val result = spiedStorage.getNextMessage(HOMESCREEN, listOf(message))

        verify(featuresInterface).recordExposureEvent("messaging", experiment)
        assertEquals(message.id, result!!.id)
    }

    @Test
    fun `GIVEN a control message WHEN calling getNextMessage THEN return the next eligible message`() {
        val spiedStorage = spy(storage)
        val experiment = "my-experiment"
        val messageData: MessageData = createMessageData()
        val controlMessageData: MessageData = createMessageData(isControl = true, experiment = experiment)

        doReturn(SHOW_NEXT_MESSAGE).`when`(spiedStorage).getOnControlBehavior()

        val message = Message(
            "id",
            messageData,
            action = "action",
            mock(),
            listOf("trigger"),
            Message.Metadata("same-id"),
        )

        val controlMessage = Message(
            "control-id",
            controlMessageData,
            action = "action",
            mock(),
            listOf("trigger"),
            Message.Metadata("same-id"),
        )

        doReturn(true).`when`(spiedStorage).isMessageEligible(any(), any(), any())

        val result = spiedStorage.getNextMessage(
            HOMESCREEN,
            listOf(controlMessage, message),
        )

        verify(messagingFeature).recordExperimentExposure(experiment)
        assertEquals(message.id, result!!.id)
    }

    @Test
    fun `GIVEN a malformed control message WHEN calling getNextMessage THEN return the next eligible message`() {
        val spiedStorage = spy(storage)
        val messageData: MessageData = createMessageData()
        // the message isControl, but has no experiment property.
        val controlMessageData: MessageData = createMessageData(isControl = true)

        doReturn(SHOW_NEXT_MESSAGE).`when`(spiedStorage).getOnControlBehavior()

        val message = Message(
            "id",
            messageData,
            action = "action",
            mock(),
            listOf("trigger"),
            Message.Metadata("same-id"),
        )

        val controlMessage = Message(
            "control-id",
            controlMessageData,
            action = "action",
            mock(),
            listOf("trigger"),
            Message.Metadata("same-id"),
        )

        doReturn(true).`when`(spiedStorage).isMessageEligible(any(), any(), any())

        val result = spiedStorage.getNextMessage(
            HOMESCREEN,
            listOf(controlMessage, message),
        )

        verify(messagingFeature).recordMalformedConfiguration("control-id")

        assertEquals(message.id, result!!.id)
    }

    @Test
    fun `GIVEN a control message WHEN calling getNextMessage THEN return the next eligible message with the correct surface`() {
        val spiedStorage = spy(storage)
        val experiment = "my-experiment"
        val messageData: MessageData = createMessageData()
        val incorrectMessageData: MessageData = createMessageData(surface = NOTIFICATION)
        val controlMessageData: MessageData = createMessageData(isControl = true, experiment = experiment)

        doReturn(SHOW_NEXT_MESSAGE).`when`(spiedStorage).getOnControlBehavior()

        val message = Message(
            "id",
            messageData,
            action = "action",
            mock(),
            listOf("trigger"),
            Message.Metadata("same-id"),
        )

        val incorrectMessage = Message(
            "incorrect-id",
            incorrectMessageData,
            action = "action",
            mock(),
            listOf("trigger"),
            Message.Metadata("same-id"),
        )

        val controlMessage = Message(
            "control-id",
            controlMessageData,
            action = "action",
            mock(),
            listOf("trigger"),
            Message.Metadata("same-id"),
        )

        doReturn(true).`when`(spiedStorage).isMessageEligible(any(), any(), any())

        var result = spiedStorage.getNextMessage(
            HOMESCREEN,
            listOf(controlMessage, incorrectMessage, message),
        )

        verify(messagingFeature, times(1)).recordExperimentExposure(experiment)
        assertEquals(message.id, result!!.id)

        result = spiedStorage.getNextMessage(
            HOMESCREEN,
            listOf(controlMessage, incorrectMessage),
        )

        verify(messagingFeature, times(2)).recordExperimentExposure(experiment)
        assertNull(result)
    }

    @Test
    fun `WHEN a storage instance is created THEN do not invoke the feature`() = runTest {
        storage = NimbusMessagingStorage(
            testContext,
            metadataStorage,
            reportMalformedMessage,
            gleanPlumb,
            messagingFeature,
        )

        // We should not be using the feature holder until getMessages is called.
        verify(messagingFeature, never()).value()
    }

    @Test
    fun `WHEN calling getMessage THEN return message with matching key OR null if doesn't exist`() =
        runTest {
            val messages = mapOf(
                "low-message" to createMessageData(style = "low-priority"),
                "high-message" to createMessageData(style = "high-priority"),
                "medium-message" to createMessageData(style = "medium-priority"),
            )
            val styles = mapOf(
                "high-priority" to createStyle(priority = 100),
                "medium-priority" to createStyle(priority = 50),
                "low-priority" to createStyle(priority = 1),
            )

            val (messagingFeature, _) = createMessagingFeature(
                styles = styles,
                messages = messages,
            )

            `when`(metadataStorage.getMetadata()).thenReturn(
                mapOf(
                    "message-1" to Message.Metadata(id = "message-1"),
                ),
            )

            val storage = NimbusMessagingStorage(
                testContext,
                metadataStorage,
                reportMalformedMessage,
                gleanPlumb,
                messagingFeature,
            )

            assertEquals("high-message", storage.getMessage("high-message")?.id)
            assertEquals("medium-message", storage.getMessage("medium-message")?.id)
            assertEquals("low-message", storage.getMessage("low-message")?.id)
            assertEquals(null, storage.getMessage("no-message")?.id)
        }

    @Test
    fun `GIVEN a message without text THEN reject the message and report it as malformed`() = runTest {
        val (feature, _) = createMessagingFeature(
            styles = mapOf(
                "style-1" to createStyle(priority = 100),
            ),
            triggers = mapOf("trigger-1" to "://trigger-1"),
            messages = mapOf(
                "missing-text" to createMessageData(text = ""),
                "ok" to createMessageData(),
            ),
        )
        val storage = NimbusMessagingStorage(
            testContext,
            metadataStorage,
            reportMalformedMessage,
            gleanPlumb,
            feature,
        )

        assertNotNull(storage.getMessage("ok"))
        assertNull(storage.getMessage("missing-text"))
        assertTrue(malformedMessageIds.contains("missing-text"))
        assertFalse(malformedMessageIds.contains("ok"))
    }

    private fun createMessageData(
        text: String = "text-1",
        action: String = "action-1",
        style: String = "style-1",
        triggers: List<String> = listOf("trigger-1"),
        surface: MessageSurfaceId = HOMESCREEN,
        isControl: Boolean = false,
        experiment: String? = null,
    ) = MessageData(
        action = Res.string(action),
        style = style,
        trigger = triggers,
        surface = surface,
        isControl = isControl,
        text = Res.string(text),
        experiment = experiment,
    )

    private fun createMessagingFeature(
        triggers: Map<String, String> = mapOf("trigger-1" to "trigger-1-expression"),
        styles: Map<String, StyleData> = mapOf("style-1" to createStyle()),
        actions: Map<String, String> = mapOf("action-1" to "action-1-url"),
        messages: Map<String, MessageData> = mapOf(
            "message-1" to createMessageData(surface = HOMESCREEN),
            "message-2" to createMessageData(surface = NOTIFICATION),
            "malformed" to createMessageData(action = "malformed-action"),
            "blanktext" to createMessageData(text = ""),
        ),
    ): Pair<FeatureHolder<Messaging>, FeaturesInterface> {
        val messaging = Messaging(
            actions = actions,
            triggers = triggers,
            messages = messages,
            styles = styles,
        )
        val featureInterface: FeaturesInterface = mock()
        // "messaging" is a hard coded value generated from Nimbus.
        val messagingFeature = FeatureHolder({ featureInterface }, "messaging") { _, _ ->
            messaging
        }
        messagingFeature.withCachedValue(messaging)

        return messagingFeature to featureInterface
    }

    private fun createStyle(priority: Int = 1, maxDisplayCount: Int = 5): StyleData {
        val style1: StyleData = mock()
        `when`(style1.priority).thenReturn(priority)
        `when`(style1.maxDisplayCount).thenReturn(maxDisplayCount)
        return style1
    }

    companion object {
        private const val HOMESCREEN = "homescreen"
        private const val NOTIFICATION = "notification"
    }
}
