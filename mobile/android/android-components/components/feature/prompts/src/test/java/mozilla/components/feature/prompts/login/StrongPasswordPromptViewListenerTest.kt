/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */
package mozilla.components.feature.prompts.login

import mozilla.components.browser.state.action.ContentAction
import mozilla.components.browser.state.state.BrowserState
import mozilla.components.browser.state.state.ContentState
import mozilla.components.browser.state.state.CustomTabSessionState
import mozilla.components.browser.state.state.TabSessionState
import mozilla.components.browser.state.store.BrowserStore
import mozilla.components.concept.engine.prompt.PromptRequest
import mozilla.components.concept.storage.Login
import mozilla.components.support.test.any
import mozilla.components.support.test.mock
import mozilla.components.support.test.whenever
import org.junit.Assert
import org.junit.Before
import org.junit.Test
import org.mockito.Mockito

class StrongPasswordPromptViewListenerTest {
    private val suggestedPassword = "generatedPassword123#"

    private val login =
        Login(
            guid = "A",
            origin = "https://www.mozilla.org",
            username = "username",
            password = "password",
        )
    private val login2 =
        Login(
            guid = "B",
            origin = "https://www.mozilla.org",
            username = "username2",
            password = "password",
        )

    private var onDismissWasCalled = false
    private var confirmedLogin: Login? = null

    private val request = PromptRequest.SelectLoginPrompt(
        logins = listOf(login, login2),
        generatedPassword = suggestedPassword,
        onConfirm = { confirmedLogin = it },
        onDismiss = { onDismissWasCalled = true },
    )

    private lateinit var store: BrowserStore
    private lateinit var state: BrowserState

    private lateinit var suggestStrongPasswordPromptViewListener: StrongPasswordPromptViewListener
    private lateinit var suggestStrongPasswordBar: SuggestStrongPasswordBar

    private val onGeneratedPasswordPromptClick: () -> Unit = mock()

    @Before
    fun setup() {
        state = mock()
        store = mock()
        suggestStrongPasswordBar = mock()
        whenever(store.state).thenReturn(state)
        suggestStrongPasswordPromptViewListener = StrongPasswordPromptViewListener(store, suggestStrongPasswordBar)
    }

    @Test
    fun `StrongPasswordGenerator shows the suggest strong password bar on a custom tab`() {
        val customTabContent: ContentState = mock()
        whenever(customTabContent.promptRequests).thenReturn(listOf(request))
        val customTab = CustomTabSessionState(
            id = "custom-tab",
            content = customTabContent,
            trackingProtection = mock(),
            config = mock(),
        )

        whenever(state.customTabs).thenReturn(listOf(customTab))

        suggestStrongPasswordPromptViewListener = StrongPasswordPromptViewListener(store, suggestStrongPasswordBar)
        suggestStrongPasswordPromptViewListener.onGeneratedPasswordPromptClick = onGeneratedPasswordPromptClick
        suggestStrongPasswordPromptViewListener.handleSuggestStrongPasswordRequest()
        Mockito.verify(suggestStrongPasswordBar).showPrompt()
    }

    @Test
    fun `StrongPasswordGenerator shows the suggest strong password bar on a selected tab`() {
        prepareSelectedSession(request)
        suggestStrongPasswordPromptViewListener = StrongPasswordPromptViewListener(store, suggestStrongPasswordBar)
        suggestStrongPasswordPromptViewListener.onGeneratedPasswordPromptClick = onGeneratedPasswordPromptClick
        suggestStrongPasswordPromptViewListener.handleSuggestStrongPasswordRequest()
        Mockito.verify(suggestStrongPasswordBar).showPrompt()
    }

    @Test
    fun `WHEN dismissCurrentSuggestStrongPassword is called without a parameter THEN the active login prompt is dismissed`() {
        val selectedSession = prepareSelectedSession(request)
        suggestStrongPasswordPromptViewListener =
            StrongPasswordPromptViewListener(store, suggestStrongPasswordBar, selectedSession.id)

        Mockito.verify(store, Mockito.never()).dispatch(any())
        suggestStrongPasswordPromptViewListener.dismissCurrentSuggestStrongPassword()

        Assert.assertTrue(onDismissWasCalled)
        Mockito.verify(store)
            .dispatch(ContentAction.ConsumePromptRequestAction(selectedSession.id, request))
        Mockito.verify(suggestStrongPasswordBar).hidePrompt()
    }

    @Test
    fun `WHEN dismissCurrentSuggestStrongPassword is called with the active login prompt passed as parameter THEN the prompt is dismissed`() {
        val selectedSession = prepareSelectedSession(request)
        suggestStrongPasswordPromptViewListener =
            StrongPasswordPromptViewListener(store, suggestStrongPasswordBar, selectedSession.id)

        Mockito.verify(store, Mockito.never()).dispatch(any())
        suggestStrongPasswordPromptViewListener.dismissCurrentSuggestStrongPassword(request)

        Assert.assertTrue(onDismissWasCalled)
        Mockito.verify(store)
            .dispatch(ContentAction.ConsumePromptRequestAction(selectedSession.id, request))
        Mockito.verify(suggestStrongPasswordBar).hidePrompt()
    }

    private fun prepareSelectedSession(request: PromptRequest? = null): TabSessionState {
        val promptRequest: PromptRequest = request ?: mock()
        val content: ContentState = mock()
        whenever(content.promptRequests).thenReturn(listOf(promptRequest))

        val selected = TabSessionState("browser-tab", content, mock(), mock())
        whenever(state.selectedTabId).thenReturn(selected.id)
        whenever(state.tabs).thenReturn(listOf(selected))
        return selected
    }
}
