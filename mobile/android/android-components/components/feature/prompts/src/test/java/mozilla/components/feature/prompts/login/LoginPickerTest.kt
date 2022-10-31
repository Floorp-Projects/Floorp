/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

package mozilla.components.feature.prompts.login

import androidx.test.ext.junit.runners.AndroidJUnit4
import mozilla.components.browser.state.state.BrowserState
import mozilla.components.browser.state.state.ContentState
import mozilla.components.browser.state.state.CustomTabSessionState
import mozilla.components.browser.state.state.TabSessionState
import mozilla.components.browser.state.store.BrowserStore
import mozilla.components.concept.engine.prompt.PromptRequest
import mozilla.components.concept.storage.Login
import mozilla.components.support.test.mock
import mozilla.components.support.test.whenever
import org.junit.Assert.assertEquals
import org.junit.Assert.assertTrue
import org.junit.Before
import org.junit.Test
import org.junit.runner.RunWith
import org.mockito.Mockito.verify

@RunWith(AndroidJUnit4::class)
class LoginPickerTest {
    val login =
        Login(guid = "A", origin = "https://www.mozilla.org", username = "username", password = "password")
    val login2 =
        Login(guid = "B", origin = "https://www.mozilla.org", username = "username2", password = "password")

    var onDismissWasCalled = false
    var confirmedLogin: Login? = null
    private val request = PromptRequest.SelectLoginPrompt(
        listOf(login, login2),
        onConfirm = { confirmedLogin = it },
        onDismiss = { onDismissWasCalled = true },
    )

    var manageLoginsCalled = false
    private lateinit var store: BrowserStore
    private lateinit var state: BrowserState
    private lateinit var loginPicker: LoginPicker
    private lateinit var loginSelectBar: LoginSelectBar
    private var onManageLogins: () -> Unit = { manageLoginsCalled = true }

    @Before
    fun setup() {
        state = mock()
        store = mock()
        loginSelectBar = mock()
        whenever(store.state).thenReturn(state)
        loginPicker = LoginPicker(store, loginSelectBar, onManageLogins)
    }

    @Test
    fun `LoginPicker shows the login select bar on a custom tab`() {
        val customTabContent: ContentState = mock()
        whenever(customTabContent.promptRequests).thenReturn(listOf(request))
        val customTab = CustomTabSessionState("custom-tab", customTabContent, mock(), mock())

        whenever(state.customTabs).thenReturn(listOf(customTab))
        loginPicker = LoginPicker(store, loginSelectBar, onManageLogins, customTab.id)
        loginPicker.handleSelectLoginRequest(request)
        verify(loginSelectBar).showPrompt(request.logins)
    }

    @Test
    fun `LoginPicker shows the login select bar on a selected tab`() {
        prepareSelectedSession(request)
        loginPicker = LoginPicker(store, loginSelectBar, onManageLogins)
        loginPicker.handleSelectLoginRequest(request)
        verify(loginSelectBar).showPrompt(request.logins)
    }

    @Test
    fun `LoginPicker selects and login through the request and hides view`() {
        prepareSelectedSession(request)
        loginPicker = LoginPicker(store, loginSelectBar, onManageLogins)

        loginPicker.handleSelectLoginRequest(request)

        loginPicker.onOptionSelect(login)

        assertEquals(confirmedLogin, login)
        verify(loginSelectBar).hidePrompt()
    }

    @Test
    fun `LoginPicker invokes manage logins and hides view`() {
        manageLoginsCalled = false
        onDismissWasCalled = false

        prepareSelectedSession(request)
        loginPicker = LoginPicker(store, loginSelectBar, onManageLogins)

        loginPicker.handleSelectLoginRequest(request)

        loginPicker.onManageOptions()

        assertTrue(manageLoginsCalled)
        assertTrue(onDismissWasCalled)
        verify(loginSelectBar).hidePrompt()
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
