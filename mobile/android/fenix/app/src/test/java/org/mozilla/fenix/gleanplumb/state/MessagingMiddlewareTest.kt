/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

package org.mozilla.fenix.gleanplumb.state

import io.mockk.Runs
import io.mockk.coEvery
import io.mockk.coVerify
import io.mockk.every
import io.mockk.just
import io.mockk.mockk
import io.mockk.spyk
import io.mockk.verify
import mozilla.components.lib.state.MiddlewareContext
import mozilla.components.service.glean.testing.GleanTestRule
import mozilla.components.support.test.robolectric.testContext
import mozilla.components.support.test.rule.MainCoroutineRule
import mozilla.components.support.test.rule.runTestOnMain
import org.junit.Assert.assertTrue
import org.junit.Before
import org.junit.Rule
import org.junit.Test
import org.junit.runner.RunWith
import org.mozilla.fenix.components.AppStore
import org.mozilla.fenix.components.appstate.AppAction
import org.mozilla.fenix.components.appstate.AppAction.MessagingAction.ConsumeMessageToShow
import org.mozilla.fenix.components.appstate.AppAction.MessagingAction.Evaluate
import org.mozilla.fenix.components.appstate.AppAction.MessagingAction.MessageClicked
import org.mozilla.fenix.components.appstate.AppAction.MessagingAction.MessageDismissed
import org.mozilla.fenix.components.appstate.AppAction.MessagingAction.Restore
import org.mozilla.fenix.components.appstate.AppAction.MessagingAction.UpdateMessageToShow
import org.mozilla.fenix.components.appstate.AppAction.MessagingAction.UpdateMessages
import org.mozilla.fenix.components.appstate.AppState
import org.mozilla.fenix.gleanplumb.Message
import org.mozilla.fenix.gleanplumb.MessagingState
import org.mozilla.fenix.gleanplumb.NimbusMessagingController
import org.mozilla.fenix.gleanplumb.NimbusMessagingStorage
import org.mozilla.fenix.helpers.FenixRobolectricTestRunner
import org.mozilla.fenix.nimbus.MessageData
import org.mozilla.fenix.nimbus.MessageSurfaceId
import org.mozilla.fenix.nimbus.StyleData

@RunWith(FenixRobolectricTestRunner::class)
class MessagingMiddlewareTest {
    @get:Rule
    val coroutinesTestRule = MainCoroutineRule()
    private val coroutineScope = coroutinesTestRule.scope
    private lateinit var appStore: AppStore
    private lateinit var middleware: MessagingMiddleware
    private lateinit var messagingStorage: NimbusMessagingStorage
    private lateinit var messagingController: NimbusMessagingController
    private lateinit var middlewareContext: MiddlewareContext<AppState, AppAction>

    @get:Rule
    val gleanTestRule = GleanTestRule(testContext)

    @Before
    fun setUp() {
        appStore = mockk(relaxed = true)
        messagingStorage = mockk(relaxed = true)
        messagingController = NimbusMessagingController(messagingStorage) { 0L }
        middlewareContext = mockk(relaxed = true)
        every { middlewareContext.store } returns appStore

        middleware = MessagingMiddleware(
            messagingStorage,
            messagingController,
            coroutineScope,
        )
    }

    @Test
    fun `WHEN Restore THEN get messages from the storage and UpdateMessages`() = runTestOnMain {
        val messages: List<Message> = emptyList()

        coEvery { messagingStorage.getMessages() } returns messages

        middleware.invoke(middlewareContext, {}, Restore)

        verify { appStore.dispatch(UpdateMessages(messages)) }
    }

    @Test
    fun `WHEN Restore THEN getNextMessage from the storage and UpdateMessageToShow`() = runTestOnMain {
        val message: Message = mockk(relaxed = true)
        val appState: AppState = mockk(relaxed = true)
        val messagingState: MessagingState = mockk(relaxed = true)

        every { messagingState.messages } returns emptyList()
        every { appState.messaging } returns messagingState
        every { middlewareContext.state } returns appState
        every { messagingStorage.getNextMessage(MessageSurfaceId.HOMESCREEN, any()) } returns message

        middleware.invoke(middlewareContext, {}, Evaluate(MessageSurfaceId.HOMESCREEN))

        verify { middlewareContext.dispatch(UpdateMessageToShow(message)) }
    }

    @Test
    fun `WHEN MessageClicked THEN update storage`() = runTestOnMain {
        val message = Message(
            "control-id",
            mockk(relaxed = true),
            action = "action",
            mockk(relaxed = true),
            listOf("trigger"),
            Message.Metadata("same-id"),
        )
        val appState: AppState = mockk(relaxed = true)
        val messagingState: MessagingState = mockk(relaxed = true)

        every { messagingState.messages } returns emptyList()
        every { appState.messaging } returns messagingState
        every { middlewareContext.state } returns appState

        middleware.invoke(middlewareContext, {}, MessageClicked(message))

        coVerify { messagingController.onMessageClicked(message) }
        coVerify { messagingStorage.updateMetadata(message.metadata.copy(pressed = true)) }
        verify { middlewareContext.dispatch(UpdateMessages(emptyList())) }
    }

    @Test
    fun `WHEN MessageDismissed THEN update storage`() = runTestOnMain {
        val message = Message(
            "control-id",
            mockk(relaxed = true),
            action = "action",
            mockk(relaxed = true),
            listOf("trigger"),
            Message.Metadata("same-id"),
        )
        val appState: AppState = mockk(relaxed = true)
        val messagingState: MessagingState = mockk(relaxed = true)

        every { messagingState.messages } returns emptyList()
        every { appState.messaging } returns messagingState
        every { middlewareContext.state } returns appState

        middleware.invoke(
            middlewareContext,
            {},
            MessageDismissed(message),
        )

        coVerify { messagingController.onMessageDismissed(message) }
        coVerify { messagingStorage.updateMetadata(message.metadata.copy(dismissed = true)) }
        verify { middlewareContext.dispatch(UpdateMessages(emptyList())) }
    }

    @Test
    fun `GIVEN a expiring message WHEN MessageDisplayed THEN update storage`() = runTestOnMain {
        val message = Message(
            "control-id",
            mockk(relaxed = true),
            action = "action",
            mockk(relaxed = true),
            listOf("trigger"),
            Message.Metadata("same-id"),
        )
        val appState: AppState = mockk(relaxed = true)
        val messagingState: MessagingState = mockk(relaxed = true)
        val spiedMiddleware = spyk(middleware)

        every { messagingState.messages } returns emptyList()
        every { appState.messaging } returns messagingState
        every { middlewareContext.state } returns appState

        spiedMiddleware.onMessagedDisplayed(message, middlewareContext)

        coVerify { messagingController.processDisplayedMessage(message) }
        coVerify { messagingStorage.updateMetadata(message.metadata.copy(displayCount = 1)) }
        verify { middlewareContext.dispatch(UpdateMessages(emptyList())) }
    }

    @Test
    fun `WHEN onMessageDismissed THEN updateMetadata,removeMessage , UpdateMessages and removeMessageToShowIfNeeded`() = runTestOnMain {
        val message = Message(
            "control-id",
            mockk(relaxed = true),
            action = "action",
            mockk(relaxed = true),
            listOf("trigger"),
            Message.Metadata("same-id"),
        )

        val spiedMiddleware = spyk(middleware)

        every { spiedMiddleware.removeMessage(middlewareContext, message) } returns emptyList()
        every { spiedMiddleware.consumeMessageToShowIfNeeded(middlewareContext, message) } just Runs

        spiedMiddleware.onMessageDismissed(middlewareContext, message)

        coVerify { messagingController.onMessageDismissed(message) }
        coVerify { messagingStorage.updateMetadata(message.metadata.copy(dismissed = true)) }
        verify { middlewareContext.dispatch(UpdateMessages(emptyList())) }
        verify { spiedMiddleware.removeMessage(middlewareContext, message) }
    }

    @Test
    fun `WHEN removeMessage THEN remove the message`() = runTestOnMain {
        val message = Message(
            "control-id",
            mockk(relaxed = true),
            action = "action",
            mockk(relaxed = true),
            listOf("trigger"),
            Message.Metadata("same-id"),
        )
        val messages = listOf(message)
        val appState: AppState = mockk(relaxed = true)
        val messagingState: MessagingState = mockk(relaxed = true)

        every { messagingState.messages } returns messages
        every { appState.messaging } returns messagingState
        every { middlewareContext.state } returns appState

        val results = middleware.removeMessage(middlewareContext, message)

        assertTrue(results.isEmpty())
    }

    @Test
    fun `WHEN consumeMessageToShowIfNeeded THEN consume the message`() = runTestOnMain {
        val message = Message(
            "control-id",
            MessageData(),
            action = "action",
            mockk(relaxed = true),
            listOf("trigger"),
            Message.Metadata("same-id"),
        )
        val appState: AppState = mockk(relaxed = true)
        val messagingState: MessagingState = mockk(relaxed = true)

        every { messagingState.messageToShow } returns mapOf(message.surface to message)
        every { appState.messaging } returns messagingState
        every { middlewareContext.state } returns appState

        middleware.consumeMessageToShowIfNeeded(middlewareContext, message)

        verify { middlewareContext.dispatch(ConsumeMessageToShow(message.surface)) }
    }

    @Test
    fun `WHEN updateMessage THEN update available messages`() = runTestOnMain {
        val oldMessage = Message(
            "oldMessage",
            MessageData(),
            action = "action",
            mockk(relaxed = true),
            listOf("trigger"),
            Message.Metadata("same-id", pressed = false),
        )

        val updatedMessage = Message(
            "oldMessage",
            MessageData(),
            action = "action",
            mockk(relaxed = true),
            listOf("trigger"),
            Message.Metadata("same-id", pressed = true),
        )

        val spiedMiddleware = spyk(middleware)

        val appState: AppState = mockk(relaxed = true)
        val messagingState: MessagingState = mockk(relaxed = true)

        every { messagingState.messageToShow } returns mapOf(oldMessage.surface to oldMessage)
        every { appState.messaging } returns messagingState
        every { middlewareContext.state } returns appState
        every { spiedMiddleware.removeMessage(middlewareContext, oldMessage) } returns emptyList()

        val results = spiedMiddleware.updateMessage(middlewareContext, oldMessage, updatedMessage)

        verify { middlewareContext.dispatch(UpdateMessageToShow(updatedMessage)) }
        verify { spiedMiddleware.removeMessage(middlewareContext, oldMessage) }

        assertTrue(results.size == 1)
        assertTrue(results.first().metadata.pressed)
    }

    @Test
    fun `GIVEN a message with that not surpassed the maxDisplayCount WHEN onMessagedDisplayed THEN update the available messages and the updateMetadata`() = runTestOnMain {
        val style: StyleData = mockk(relaxed = true)
        val oldMessageData: MessageData = mockk(relaxed = true)
        val oldMessage = Message(
            "oldMessage",
            oldMessageData,
            action = "action",
            style,
            listOf("trigger"),
            Message.Metadata("same-id", displayCount = 0),
        )
        val updatedMessage = oldMessage.copy(metadata = oldMessage.metadata.copy(displayCount = 1))
        val spiedMiddleware = spyk(middleware)

        every { style.maxDisplayCount } returns 2
        every {
            spiedMiddleware.updateMessage(
                middlewareContext,
                oldMessage,
                updatedMessage,
            )
        } returns emptyList()

        spiedMiddleware.onMessagedDisplayed(oldMessage, middlewareContext)

        verify { spiedMiddleware.updateMessage(middlewareContext, oldMessage, updatedMessage) }
        verify { middlewareContext.dispatch(UpdateMessages(emptyList())) }
        coVerify { messagingController.processDisplayedMessage(oldMessage) }
        coVerify { messagingStorage.updateMetadata(updatedMessage.metadata) }
    }

    @Test
    fun `GIVEN a message with that surpassed the maxDisplayCount WHEN onMessagedDisplayed THEN remove the message and consume it`() = runTestOnMain {
        val style: StyleData = mockk(relaxed = true)
        val oldMessageData: MessageData = mockk(relaxed = true)
        val oldMessage = Message(
            "oldMessage",
            oldMessageData,
            action = "action",
            style,
            listOf("trigger"),
            Message.Metadata("same-id", displayCount = 0),
        )
        val updatedMessage = oldMessage.copy(metadata = oldMessage.metadata.copy(displayCount = 1))
        val spiedMiddleware = spyk(middleware)

        every { style.maxDisplayCount } returns 1
        every {
            spiedMiddleware.consumeMessageToShowIfNeeded(
                middlewareContext,
                oldMessage,
            )
        } just Runs
        every { spiedMiddleware.removeMessage(middlewareContext, oldMessage) } returns emptyList()

        spiedMiddleware.onMessagedDisplayed(oldMessage, middlewareContext)

        verify { spiedMiddleware.consumeMessageToShowIfNeeded(middlewareContext, oldMessage) }
        verify { spiedMiddleware.removeMessage(middlewareContext, oldMessage) }
        verify { middlewareContext.dispatch(UpdateMessages(emptyList())) }
        coVerify { messagingController.processDisplayedMessage(oldMessage) }
        coVerify { messagingStorage.updateMetadata(updatedMessage.metadata) }
    }
}
