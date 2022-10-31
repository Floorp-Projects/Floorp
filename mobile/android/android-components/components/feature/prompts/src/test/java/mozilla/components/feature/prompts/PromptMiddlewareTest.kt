/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

package mozilla.components.feature.prompts

import androidx.test.ext.junit.runners.AndroidJUnit4
import kotlinx.coroutines.ExperimentalCoroutinesApi
import mozilla.components.browser.state.action.ContentAction
import mozilla.components.browser.state.state.BrowserState
import mozilla.components.browser.state.state.TabSessionState
import mozilla.components.browser.state.state.createTab
import mozilla.components.browser.state.store.BrowserStore
import mozilla.components.concept.engine.prompt.PromptRequest
import mozilla.components.support.test.ext.joinBlocking
import mozilla.components.support.test.rule.MainCoroutineRule
import org.junit.Assert.assertEquals
import org.junit.Before
import org.junit.Rule
import org.junit.Test
import org.junit.runner.RunWith
import org.mockito.Mockito.never
import org.mockito.Mockito.spy
import org.mockito.Mockito.verify

@RunWith(AndroidJUnit4::class)
class PromptMiddlewareTest {

    @get:Rule
    val coroutinesTestRule = MainCoroutineRule()

    private lateinit var store: BrowserStore

    private val tabId = "test-tab"
    private fun tab(): TabSessionState? {
        return store.state.tabs.find { it.id == tabId }
    }

    @Before
    @ExperimentalCoroutinesApi
    fun setUp() {
        store = BrowserStore(
            BrowserState(
                tabs = listOf(
                    createTab("https://www.mozilla.org", id = tabId),
                ),
                selectedTabId = tabId,
            ),
            middleware = listOf(PromptMiddleware()),
        )
    }

    @Test
    fun `Process only one popup prompt request at a time`() {
        val onDeny = spy { }
        val popupPrompt1 = PromptRequest.Popup("https://firefox.com", onAllow = { }, onDeny = onDeny)
        store.dispatch(ContentAction.UpdatePromptRequestAction(tabId, popupPrompt1)).joinBlocking()
        assertEquals(1, tab()!!.content.promptRequests.size)
        assertEquals(popupPrompt1, tab()!!.content.promptRequests[0])
        verify(onDeny, never()).invoke()

        val popupPrompt2 = PromptRequest.Popup("https://firefox.com", onAllow = { }, onDeny = onDeny)
        store.dispatch(ContentAction.UpdatePromptRequestAction(tabId, popupPrompt2)).joinBlocking()
        assertEquals(1, tab()!!.content.promptRequests.size)
        assertEquals(popupPrompt1, tab()!!.content.promptRequests[0])
        verify(onDeny).invoke()
    }

    @Test
    fun `Process popup followed by other prompt request`() {
        val onDeny = spy { }
        val popupPrompt = PromptRequest.Popup("https://firefox.com", onAllow = { }, onDeny = onDeny)
        store.dispatch(ContentAction.UpdatePromptRequestAction(tabId, popupPrompt)).joinBlocking()
        assertEquals(1, tab()!!.content.promptRequests.size)
        assertEquals(popupPrompt, tab()!!.content.promptRequests[0])
        verify(onDeny, never()).invoke()

        val alert = PromptRequest.Alert("title", "message", false, { }, { })
        store.dispatch(ContentAction.UpdatePromptRequestAction(tabId, alert)).joinBlocking()
        assertEquals(2, tab()!!.content.promptRequests.size)
        assertEquals(popupPrompt, tab()!!.content.promptRequests[0])
        assertEquals(alert, tab()!!.content.promptRequests[1])
    }

    @Test
    fun `Process popup after other prompt request`() {
        val alert = PromptRequest.Alert("title", "message", false, { }, { })
        store.dispatch(ContentAction.UpdatePromptRequestAction(tabId, alert)).joinBlocking()
        assertEquals(1, tab()!!.content.promptRequests.size)
        assertEquals(alert, tab()!!.content.promptRequests[0])

        val onDeny = spy { }
        val popupPrompt = PromptRequest.Popup("https://firefox.com", onAllow = { }, onDeny = onDeny)
        store.dispatch(ContentAction.UpdatePromptRequestAction(tabId, popupPrompt)).joinBlocking()
        assertEquals(2, tab()!!.content.promptRequests.size)
        assertEquals(alert, tab()!!.content.promptRequests[0])
        assertEquals(popupPrompt, tab()!!.content.promptRequests[1])
        verify(onDeny, never()).invoke()
    }

    @Test
    fun `Process other prompt requests`() {
        val alert = PromptRequest.Alert("title", "message", false, { }, { })
        store.dispatch(ContentAction.UpdatePromptRequestAction(tabId, alert)).joinBlocking()
        assertEquals(1, tab()!!.content.promptRequests.size)
        assertEquals(alert, tab()!!.content.promptRequests[0])

        val beforeUnloadPrompt = PromptRequest.BeforeUnload("title", onLeave = { }, onStay = { })
        store.dispatch(ContentAction.UpdatePromptRequestAction(tabId, beforeUnloadPrompt)).joinBlocking()
        assertEquals(2, tab()!!.content.promptRequests.size)
        assertEquals(alert, tab()!!.content.promptRequests[0])
        assertEquals(beforeUnloadPrompt, tab()!!.content.promptRequests[1])
    }
}
