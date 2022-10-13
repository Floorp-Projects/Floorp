/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

package mozilla.components.feature.session.middleware

import androidx.test.ext.junit.runners.AndroidJUnit4
import mozilla.components.browser.state.action.BrowserAction
import mozilla.components.browser.state.action.ContentAction
import mozilla.components.browser.state.action.TabListAction
import mozilla.components.browser.state.selector.findTab
import mozilla.components.browser.state.selector.selectedTab
import mozilla.components.browser.state.state.BrowserState
import mozilla.components.browser.state.state.createTab
import mozilla.components.browser.state.state.recover.RecoverableTab
import mozilla.components.browser.state.state.recover.TabState
import mozilla.components.browser.state.store.BrowserStore
import mozilla.components.lib.state.MiddlewareContext
import mozilla.components.support.test.ext.joinBlocking
import mozilla.components.support.test.mock
import mozilla.components.support.test.whenever
import org.junit.Assert.assertEquals
import org.junit.Assert.assertFalse
import org.junit.Assert.assertNotEquals
import org.junit.Assert.assertNotNull
import org.junit.Assert.assertTrue
import org.junit.Before
import org.junit.Test
import org.junit.runner.RunWith

@RunWith(AndroidJUnit4::class)
class LastAccessMiddlewareTest {
    lateinit var store: BrowserStore
    lateinit var context: MiddlewareContext<BrowserState, BrowserAction>

    @Before
    fun setup() {
        store = BrowserStore()
        context = mock()

        whenever(context.store).thenReturn(store)
        whenever(context.state).thenReturn(store.state)
    }

    @Test
    fun `UpdateLastAction is dispatched when tab is selected`() {
        val store = BrowserStore(
            initialState = BrowserState(
                listOf(
                    createTab("https://mozilla.org", id = "123"),
                    createTab("https://firefox.com", id = "456"),
                ),
                selectedTabId = "123",
            ),
            middleware = listOf(LastAccessMiddleware()),
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
                    createTab("https://mozilla.org", id = "123"),
                ),
                selectedTabId = "123",
            ),
            middleware = listOf(LastAccessMiddleware()),
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
                    createTab("https://mozilla.org", id = "123"),
                ),
                selectedTabId = "123",
            ),
            middleware = listOf(LastAccessMiddleware()),
        )

        assertEquals(0L, store.state.selectedTab?.lastAccess)

        val newTab = createTab("https://firefox.com", id = "456")
        store.dispatch(TabListAction.AddTabAction(newTab, select = false)).joinBlocking()

        assertEquals("123", store.state.selectedTabId)
        assertEquals(0L, store.state.selectedTab?.lastAccess)
        assertEquals(0L, store.state.tabs[1].lastAccess)
    }

    @Test
    fun `UpdateLastAction is dispatched when URL of selected tab changes`() {
        val tab = createTab("https://firefox.com", id = "123")
        val store = BrowserStore(
            initialState = BrowserState(
                listOf(tab),
                selectedTabId = tab.id,
            ),
            middleware = listOf(LastAccessMiddleware()),
        )
        assertEquals(0L, store.state.selectedTab?.lastAccess)

        store.dispatch(ContentAction.UpdateUrlAction(tab.id, "https://mozilla.org")).joinBlocking()
        assertNotEquals(0L, store.state.selectedTab?.lastAccess)
    }

    @Test
    fun `UpdateLastAction is not dispatched when URL of non-selected tab changes`() {
        val tab = createTab("https://firefox.com", id = "123")
        val store = BrowserStore(
            initialState = BrowserState(
                listOf(tab),
                selectedTabId = tab.id,
            ),
            middleware = listOf(LastAccessMiddleware()),
        )
        assertEquals(0L, store.state.selectedTab?.lastAccess)

        val newTab = createTab("https://mozilla.org", id = "456")
        store.dispatch(TabListAction.AddTabAction(newTab)).joinBlocking()
        store.dispatch(ContentAction.UpdateUrlAction(newTab.id, "https://mozilla.org")).joinBlocking()
        assertEquals(0L, store.state.selectedTab?.lastAccess)
        assertEquals(0L, store.state.findTab(newTab.id)?.lastAccess)
    }

    @Test
    fun `UpdateLastAction is dispatched when tab is selected during removal of single tab`() {
        val store = BrowserStore(
            initialState = BrowserState(
                listOf(
                    createTab("https://mozilla.org", id = "123"),
                    createTab("https://firefox.com", id = "456"),
                ),
                selectedTabId = "123",
            ),
            middleware = listOf(LastAccessMiddleware()),
        )

        assertEquals(0L, store.state.tabs[0].lastAccess)
        assertEquals(0L, store.state.tabs[1].lastAccess)

        store.dispatch(TabListAction.RemoveTabAction("123")).joinBlocking()

        val selectedTab = store.state.findTab("456")
        assertNotNull(selectedTab)
        assertEquals(selectedTab!!.id, store.state.selectedTabId)
        assertNotEquals(0L, selectedTab.lastAccess)
    }

    @Test
    fun `UpdateLastAction is not dispatched when no new tab is selected during removal of single tab`() {
        val store = BrowserStore(
            initialState = BrowserState(
                listOf(
                    createTab("https://mozilla.org", id = "123"),
                    createTab("https://firefox.com", id = "456"),
                ),
                selectedTabId = "123",
            ),
            middleware = listOf(LastAccessMiddleware()),
        )

        assertEquals(0L, store.state.tabs[0].lastAccess)
        assertEquals(0L, store.state.tabs[1].lastAccess)

        store.dispatch(TabListAction.RemoveTabAction("456")).joinBlocking()
        val selectedTab = store.state.findTab("123")
        assertNotNull(selectedTab)
        assertEquals(selectedTab!!.id, store.state.selectedTabId)
        assertEquals(0L, selectedTab.lastAccess)
    }

    @Test
    fun `UpdateLastAction is dispatched when tab is selected during removal of multiple tab`() {
        val store = BrowserStore(
            initialState = BrowserState(
                listOf(
                    createTab("https://mozilla.org", id = "123"),
                    createTab("https://firefox.com", id = "456"),
                    createTab("https://getpocket.com", id = "789"),
                ),
                selectedTabId = "123",
            ),
            middleware = listOf(LastAccessMiddleware()),
        )

        assertEquals(0L, store.state.tabs[0].lastAccess)
        assertEquals(0L, store.state.tabs[1].lastAccess)
        assertEquals(0L, store.state.tabs[2].lastAccess)

        store.dispatch(TabListAction.RemoveTabsAction(listOf("123", "456"))).joinBlocking()

        val selectedTab = store.state.findTab("789")
        assertNotNull(selectedTab)
        assertEquals(selectedTab!!.id, store.state.selectedTabId)
        assertNotEquals(0L, selectedTab.lastAccess)
    }

    @Test
    fun `UpdateLastAction is not dispatched when no new tab is selected during removal of multiple tab`() {
        val store = BrowserStore(
            initialState = BrowserState(
                listOf(
                    createTab("https://mozilla.org", id = "123"),
                    createTab("https://firefox.com", id = "456"),
                    createTab("https://getpocket.com", id = "789"),
                ),
                selectedTabId = "123",
            ),
            middleware = listOf(LastAccessMiddleware()),
        )

        assertEquals(0L, store.state.tabs[0].lastAccess)
        assertEquals(0L, store.state.tabs[1].lastAccess)
        assertEquals(0L, store.state.tabs[2].lastAccess)

        store.dispatch(TabListAction.RemoveTabsAction(listOf("456", "789"))).joinBlocking()
        val selectedTab = store.state.findTab("123")
        assertEquals(selectedTab!!.id, store.state.selectedTabId)
        assertEquals(0L, selectedTab.lastAccess)
    }

    @Test
    fun `UpdateLastAction is dispatched when tab is selected during removal of all private tab`() {
        val store = BrowserStore(
            initialState = BrowserState(
                listOf(
                    createTab("https://mozilla.org", id = "123", private = true),
                    createTab("https://firefox.com", id = "456", private = true),
                    createTab("https://getpocket.com", id = "789"),
                ),
                selectedTabId = "123",
            ),
            middleware = listOf(LastAccessMiddleware()),
        )

        assertEquals(0L, store.state.tabs[0].lastAccess)
        assertEquals(0L, store.state.tabs[1].lastAccess)
        assertEquals(0L, store.state.tabs[2].lastAccess)

        store.dispatch(TabListAction.RemoveAllPrivateTabsAction).joinBlocking()

        val selectedTab = store.state.findTab("789")
        assertNotNull(selectedTab)
        assertEquals(selectedTab!!.id, store.state.selectedTabId)
        assertNotEquals(0L, selectedTab.lastAccess)
    }

    @Test
    fun `UpdateLastAction is not dispatched when no new tab is selected during removal of all private tab`() {
        val store = BrowserStore(
            initialState = BrowserState(
                listOf(
                    createTab("https://mozilla.org", id = "123"),
                    createTab("https://firefox.com", id = "456", private = true),
                    createTab("https://getpocket.com", id = "789", private = true),
                ),
                selectedTabId = "123",
            ),
            middleware = listOf(LastAccessMiddleware()),
        )

        assertEquals(0L, store.state.tabs[0].lastAccess)
        assertEquals(0L, store.state.tabs[1].lastAccess)
        assertEquals(0L, store.state.tabs[2].lastAccess)

        store.dispatch(TabListAction.RemoveAllPrivateTabsAction).joinBlocking()

        val selectedTab = store.state.findTab("123")
        assertNotNull(selectedTab)
        assertEquals(selectedTab!!.id, store.state.selectedTabId)
        assertEquals(0L, selectedTab.lastAccess)
    }

    @Test
    fun `UpdateLastAction is invoked for selected tab from RestoreAction`() {
        val recentTime = System.currentTimeMillis()
        val lastAccess = 3735928559
        val store = BrowserStore(
            middleware = listOf(LastAccessMiddleware()),
        )
        val recoverableTabs = listOf(
            RecoverableTab(
                engineSessionState = null,
                state = TabState(url = "https://firefox.com", id = "1", lastAccess = lastAccess),
            ),
            RecoverableTab(
                engineSessionState = null,
                state = TabState(url = "https://mozilla.org", id = "2", lastAccess = lastAccess),
            ),
        )

        store.dispatch(
            TabListAction.RestoreAction(
                recoverableTabs,
                "2",
                TabListAction.RestoreAction.RestoreLocation.BEGINNING,
            ),
        ).joinBlocking()

        assertTrue(store.state.tabs.size == 2)

        val restoredTab1 = store.state.findTab("1")
        val restoredTab2 = store.state.findTab("2")
        assertNotNull(restoredTab1)
        assertNotNull(restoredTab2)

        assertNotEquals(restoredTab2!!.lastAccess, lastAccess)
        assertTrue(restoredTab2.lastAccess > lastAccess)
        assertTrue(restoredTab2.lastAccess > recentTime)

        assertEquals(restoredTab1!!.lastAccess, lastAccess)
        assertFalse(restoredTab1.lastAccess > lastAccess)
        assertFalse(restoredTab1.lastAccess > recentTime)
    }

    @Test
    fun `sanity check - next is always invoked in the middleware`() {
        var nextInvoked = false
        val middleware = LastAccessMiddleware()

        middleware.invoke(context, { nextInvoked = true }, TabListAction.RemoveTabAction("123"))

        assertTrue(nextInvoked)
    }
}
