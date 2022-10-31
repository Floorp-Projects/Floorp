/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

package mozilla.components.browser.state.engine.middleware

import mozilla.components.browser.state.action.EngineAction
import mozilla.components.browser.state.action.TabListAction
import mozilla.components.browser.state.state.BrowserState
import mozilla.components.browser.state.state.createTab
import mozilla.components.browser.state.store.BrowserStore
import mozilla.components.concept.engine.EngineSession
import mozilla.components.support.test.ext.joinBlocking
import mozilla.components.support.test.mock
import org.junit.Assert.assertEquals
import org.junit.Assert.assertNull
import org.junit.Test
import org.mockito.ArgumentMatchers.anyBoolean
import org.mockito.Mockito.never
import org.mockito.Mockito.verify

class WebExtensionMiddlewareTest {

    @Test
    fun `marks engine session as active when selected`() {
        val middleware = WebExtensionMiddleware()

        val store = BrowserStore(
            initialState = BrowserState(
                tabs = listOf(
                    createTab("https://www.mozilla.org", id = "1"),
                    createTab("https://www.firefox.com", id = "2"),
                ),
            ),
            middleware = listOf(middleware),
        )

        val engineSession1: EngineSession = mock()
        val engineSession2: EngineSession = mock()
        store.dispatch(EngineAction.LinkEngineSessionAction("1", engineSession1)).joinBlocking()
        store.dispatch(EngineAction.LinkEngineSessionAction("2", engineSession2)).joinBlocking()

        assertNull(store.state.activeWebExtensionTabId)
        verify(engineSession1, never()).markActiveForWebExtensions(anyBoolean())
        verify(engineSession2, never()).markActiveForWebExtensions(anyBoolean())

        store.dispatch(TabListAction.SelectTabAction("1")).joinBlocking()
        assertEquals("1", store.state.activeWebExtensionTabId)
        verify(engineSession1).markActiveForWebExtensions(true)
        verify(engineSession2, never()).markActiveForWebExtensions(anyBoolean())

        store.dispatch(TabListAction.SelectTabAction("2")).joinBlocking()
        assertEquals("2", store.state.activeWebExtensionTabId)
        verify(engineSession1).markActiveForWebExtensions(false)
        verify(engineSession2).markActiveForWebExtensions(true)
    }

    @Test
    fun `marks selected engine session as active when linked`() {
        val middleware = WebExtensionMiddleware()

        val store = BrowserStore(
            initialState = BrowserState(
                tabs = listOf(
                    createTab("https://www.mozilla.org", id = "1"),
                    createTab("https://www.firefox.com", id = "2"),
                ),
                selectedTabId = "1",
            ),
            middleware = listOf(middleware),
        )

        val engineSession1: EngineSession = mock()
        val engineSession2: EngineSession = mock()
        assertNull(store.state.activeWebExtensionTabId)
        verify(engineSession1, never()).markActiveForWebExtensions(anyBoolean())
        verify(engineSession2, never()).markActiveForWebExtensions(anyBoolean())

        store.dispatch(EngineAction.LinkEngineSessionAction("1", engineSession1)).joinBlocking()
        assertEquals("1", store.state.activeWebExtensionTabId)
        verify(engineSession1).markActiveForWebExtensions(true)
        verify(engineSession2, never()).markActiveForWebExtensions(anyBoolean())
    }

    @Test
    fun `marks selected engine session as inactive when unlinked`() {
        val middleware = WebExtensionMiddleware()

        val store = BrowserStore(
            initialState = BrowserState(
                tabs = listOf(
                    createTab("https://www.mozilla.org", id = "1"),
                ),
                selectedTabId = "1",
            ),
            middleware = listOf(middleware),
        )

        val engineSession1: EngineSession = mock()
        store.dispatch(EngineAction.LinkEngineSessionAction("1", engineSession1)).joinBlocking()
        assertEquals("1", store.state.activeWebExtensionTabId)
        verify(engineSession1).markActiveForWebExtensions(true)

        store.dispatch(EngineAction.UnlinkEngineSessionAction("1")).joinBlocking()
        verify(engineSession1).markActiveForWebExtensions(false)
    }

    @Test
    fun `marks new selected engine session as active when previous one is removed`() {
        val middleware = WebExtensionMiddleware()

        val store = BrowserStore(
            initialState = BrowserState(
                tabs = listOf(
                    createTab("https://www.mozilla.org", id = "1"),
                    createTab("https://www.firefox.com", id = "2"),
                ),
            ),
            middleware = listOf(middleware),
        )

        val engineSession1: EngineSession = mock()
        val engineSession2: EngineSession = mock()
        store.dispatch(EngineAction.LinkEngineSessionAction("1", engineSession1)).joinBlocking()
        store.dispatch(EngineAction.LinkEngineSessionAction("2", engineSession2)).joinBlocking()

        store.dispatch(TabListAction.SelectTabAction("1")).joinBlocking()
        assertEquals("1", store.state.activeWebExtensionTabId)
        verify(engineSession2, never()).markActiveForWebExtensions(anyBoolean())

        store.dispatch(TabListAction.RemoveTabAction("1")).joinBlocking()
        assertEquals("2", store.state.activeWebExtensionTabId)
        verify(engineSession2).markActiveForWebExtensions(true)
    }
}
