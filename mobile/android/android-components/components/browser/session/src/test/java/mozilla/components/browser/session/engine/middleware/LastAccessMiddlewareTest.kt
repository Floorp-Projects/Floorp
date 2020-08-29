/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

package mozilla.components.browser.session.engine.middleware

import mozilla.components.browser.state.action.BrowserAction
import mozilla.components.browser.state.action.TabListAction
import mozilla.components.browser.state.selector.selectedTab
import mozilla.components.browser.state.state.BrowserState
import mozilla.components.browser.state.state.createTab
import mozilla.components.browser.state.store.BrowserStore
import mozilla.components.lib.state.MiddlewareContext
import mozilla.components.support.test.ext.joinBlocking
import mozilla.components.support.test.mock
import mozilla.components.support.test.whenever
import org.junit.Assert.assertEquals
import org.junit.Assert.assertNotEquals
import org.junit.Assert.assertTrue
import org.junit.Before
import org.junit.Test

class LastAccessMiddlewareTest {
    lateinit var store: BrowserStore
    lateinit var context: MiddlewareContext<BrowserState, BrowserAction>

    @Before
    fun setup() {
        store = mock()
        context = mock()

        whenever(context.store).thenReturn(store)
    }

    @Test
    fun `UpdateLastAction is dispatched when tab is selected`() {
        val store = BrowserStore(
            initialState = BrowserState(
                listOf(
                    createTab("https://mozilla.org", id = "123"),
                    createTab("https://firefox.com", id = "456")
                ),
                selectedTabId = "123"
            ),
            middleware = listOf(LastAccessMiddleware())
        )

        assertEquals(0L, store.state.tabs[0].lastAccess)
        assertEquals(0L, store.state.tabs[1].lastAccess)

        store.dispatch(TabListAction.SelectTabAction("456")).joinBlocking()

        assertEquals(0L, store.state.tabs[0].lastAccess)
        assertNotEquals(0L, store.state.tabs[1].lastAccess)
    }

    @Test
    fun `UpdateLastAction is dispatched when a new tab is selected`() {
        val store = BrowserStore(
            initialState = BrowserState(
                listOf(
                    createTab("https://mozilla.org", id = "123")
                ),
                selectedTabId = "123"
            ),
            middleware = listOf(LastAccessMiddleware())
        )

        assertEquals(0L, store.state.selectedTab?.lastAccess)

        val newTab = createTab("https://firefox.com", id = "456")
        store.dispatch(TabListAction.AddTabAction(newTab, select = true)).joinBlocking()

        assertEquals("456", store.state.selectedTabId)
        assertNotEquals(0L, store.state.selectedTab?.lastAccess)
    }

    @Test
    fun `UpdateLastAction is not dispatched when a new tab is not selected`() {
        val store = BrowserStore(
            initialState = BrowserState(
                listOf(
                    createTab("https://mozilla.org", id = "123")
                ),
                selectedTabId = "123"
            ),
            middleware = listOf(LastAccessMiddleware())
        )

        assertEquals(0L, store.state.selectedTab?.lastAccess)

        val newTab = createTab("https://firefox.com", id = "456")
        store.dispatch(TabListAction.AddTabAction(newTab, select = false)).joinBlocking()

        assertEquals("123", store.state.selectedTabId)
        assertEquals(0L, store.state.selectedTab?.lastAccess)
        assertEquals(0L, store.state.tabs[1].lastAccess)
    }

    @Test
    fun `sanity check - next is always invoked in the middleware`() {
        var nextInvoked = false
        val middleware = LastAccessMiddleware()

        middleware.invoke(context, { nextInvoked = true }, TabListAction.RemoveTabAction("123"))

        assertTrue(nextInvoked)
    }
}
