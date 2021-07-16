/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

package mozilla.components.feature.session.middleware.undo

import kotlinx.coroutines.CoroutineScope
import kotlinx.coroutines.Dispatchers
import kotlinx.coroutines.test.TestCoroutineDispatcher
import kotlinx.coroutines.test.resetMain
import kotlinx.coroutines.test.setMain
import mozilla.components.browser.state.action.TabListAction
import mozilla.components.browser.state.action.UndoAction
import mozilla.components.browser.state.selector.selectedTab
import mozilla.components.browser.state.state.BrowserState
import mozilla.components.browser.state.state.createTab
import mozilla.components.browser.state.store.BrowserStore
import mozilla.components.support.test.ext.joinBlocking
import mozilla.components.support.test.libstate.ext.waitUntilIdle
import org.junit.After
import org.junit.Assert.assertEquals
import org.junit.Assert.assertNull
import org.junit.Assert.assertTrue
import org.junit.Before
import org.junit.Test

class UndoMiddlewareTest {
    private lateinit var testDispatcher: TestCoroutineDispatcher

    @Before
    fun setUp() {
        testDispatcher = TestCoroutineDispatcher()
        Dispatchers.setMain(testDispatcher)
    }

    @After
    fun tearDown() {
        Dispatchers.resetMain()
        testDispatcher.cleanupTestCoroutines()
    }

    @Test
    fun `Undo scenario - Removing single tab`() {
        val store = BrowserStore(
            middleware = listOf(
                UndoMiddleware(clearAfterMillis = 60000)
            ),
            initialState = BrowserState(
                tabs = listOf(
                    createTab("https://www.mozilla.org", id = "mozilla"),
                    createTab("https://getpocket.com", id = "pocket")
                ),
                selectedTabId = "mozilla"
            )
        )

        assertEquals(2, store.state.tabs.size)
        assertEquals(2, store.state.tabs.size)
        assertEquals("https://www.mozilla.org", store.state.selectedTab!!.content.url)

        store.dispatch(
            TabListAction.RemoveTabAction(tabId = "mozilla")
        ).joinBlocking()

        assertEquals(1, store.state.tabs.size)
        assertEquals("https://getpocket.com", store.state.selectedTab!!.content.url)

        testDispatcher.withDispatchingPaused {
            // We need to pause the test dispatcher here to avoid it dispatching immediately.
            // Otherwise we deadlock the test here when we wait for the store to complete and
            // at the same time the middleware dispatches a coroutine on the dispatcher which will
            // also block on the store in SessionManager.restore().
            store.dispatch(UndoAction.RestoreRecoverableTabs).joinBlocking()
        }

        store.waitUntilIdle()

        assertEquals(2, store.state.tabs.size)
        assertEquals("https://www.mozilla.org", store.state.selectedTab!!.content.url)
    }

    @Test
    fun `Undo scenario - Removing list of tabs`() {
        val store = BrowserStore(
            middleware = listOf(
                UndoMiddleware(clearAfterMillis = 60000)
            ),
            initialState = BrowserState(
                tabs = listOf(
                    createTab("https://www.mozilla.org", id = "mozilla"),
                    createTab("https://getpocket.com", id = "pocket"),
                    createTab("https://firefox.com", id = "firefox")
                ),
                selectedTabId = "mozilla"
            )
        )

        assertEquals(3, store.state.tabs.size)
        assertEquals("https://www.mozilla.org", store.state.selectedTab!!.content.url)

        store.dispatch(
            TabListAction.RemoveTabsAction(listOf("mozilla", "pocket"))
        ).joinBlocking()

        assertEquals(1, store.state.tabs.size)
        assertEquals("https://firefox.com", store.state.selectedTab!!.content.url)

        testDispatcher.withDispatchingPaused {
            // We need to pause the test dispatcher here to avoid it dispatching immediately.
            // Otherwise we deadlock the test here when we wait for the store to complete and
            // at the same time the middleware dispatches a coroutine on the dispatcher which will
            // also block on the store in SessionManager.restore().
            store.dispatch(UndoAction.RestoreRecoverableTabs).joinBlocking()
        }

        store.waitUntilIdle()

        assertEquals(3, store.state.tabs.size)
        assertEquals("https://www.mozilla.org", store.state.selectedTab!!.content.url)
    }

    @Test
    fun `Undo scenario - Removing all normal tabs`() {
        val store = BrowserStore(
            middleware = listOf(
                UndoMiddleware(clearAfterMillis = 60000)
            ),
            initialState = BrowserState(
                tabs = listOf(
                    createTab("https://www.mozilla.org", id = "mozilla"),
                    createTab("https://getpocket.com", id = "pocket"),
                    createTab("https://reddit.com/r/firefox", id = "reddit", private = true)
                ),
                selectedTabId = "pocket"
            )
        )

        assertEquals(3, store.state.tabs.size)
        assertEquals("https://getpocket.com", store.state.selectedTab!!.content.url)

        store.dispatch(
            TabListAction.RemoveAllNormalTabsAction
        ).joinBlocking()

        assertEquals(1, store.state.tabs.size)
        assertNull(store.state.selectedTab)

        testDispatcher.withDispatchingPaused {
            // We need to pause the test dispatcher here to avoid it dispatching immediately.
            // Otherwise we deadlock the test here when we wait for the store to complete and
            // at the same time the middleware dispatches a coroutine on the dispatcher which will
            // also block on the store in SessionManager.restore().
            store.dispatch(UndoAction.RestoreRecoverableTabs).joinBlocking()
        }

        store.waitUntilIdle()

        assertEquals(3, store.state.tabs.size)
        assertEquals("https://getpocket.com", store.state.selectedTab!!.content.url)
    }

    @Test
    fun `Undo scenario - Removing all tabs`() {
        val store = BrowserStore(
            middleware = listOf(
                UndoMiddleware(clearAfterMillis = 60000)
            ),
            initialState = BrowserState(
                tabs = listOf(
                    createTab("https://www.mozilla.org", id = "mozilla"),
                    createTab("https://getpocket.com", id = "pocket"),
                    createTab("https://reddit.com/r/firefox", id = "reddit", private = true)
                ),
                selectedTabId = "pocket"
            )
        )

        assertEquals(3, store.state.tabs.size)
        assertEquals("https://getpocket.com", store.state.selectedTab!!.content.url)

        store.dispatch(
            TabListAction.RemoveAllTabsAction()
        ).joinBlocking()

        assertEquals(0, store.state.tabs.size)
        assertNull(store.state.selectedTab)

        testDispatcher.withDispatchingPaused {
            // We need to pause the test dispatcher here to avoid it dispatching immediately.
            // Otherwise we deadlock the test here when we wait for the store to complete and
            // at the same time the middleware dispatches a coroutine on the dispatcher which will
            // also block on the store in SessionManager.restore().
            store.dispatch(UndoAction.RestoreRecoverableTabs).joinBlocking()
        }

        store.waitUntilIdle()

        assertEquals(3, store.state.tabs.size)
        assertEquals("https://getpocket.com", store.state.selectedTab!!.content.url)
    }

    @Test
    fun `Undo scenario - Removing all tabs non-recoverable`() {
        val store = BrowserStore(
            middleware = listOf(
                UndoMiddleware(clearAfterMillis = 60000)
            ),
            initialState = BrowserState(
                tabs = listOf(
                    createTab("https://www.mozilla.org", id = "mozilla"),
                    createTab("https://getpocket.com", id = "pocket"),
                    createTab("https://reddit.com/r/firefox", id = "reddit", private = true)
                ),
                selectedTabId = "pocket"
            )
        )

        assertEquals(3, store.state.tabs.size)
        assertEquals("https://getpocket.com", store.state.selectedTab!!.content.url)

        store.dispatch(
            TabListAction.RemoveAllTabsAction(false)
        ).joinBlocking()

        assertEquals(0, store.state.tabs.size)
        assertNull(store.state.selectedTab)

        testDispatcher.withDispatchingPaused {
            // We need to pause the test dispatcher here to avoid it dispatching immediately.
            // Otherwise we deadlock the test here when we wait for the store to complete and
            // at the same time the middleware dispatches a coroutine on the dispatcher which will
            // also block on the store in SessionManager.restore().
            store.dispatch(UndoAction.RestoreRecoverableTabs).joinBlocking()
        }

        store.waitUntilIdle()

        assertEquals(0, store.state.tabs.size)
    }

    @Test
    fun `Undo History in State is written`() {
        val store = BrowserStore(
            middleware = listOf(
                UndoMiddleware(clearAfterMillis = 60000)
            ),
            initialState = BrowserState(
                tabs = listOf(
                    createTab("https://www.mozilla.org", id = "mozilla"),
                    createTab("https://getpocket.com", id = "pocket"),
                    createTab("https://reddit.com/r/firefox", id = "reddit", private = true)
                ),
                selectedTabId = "pocket"
            )
        )

        assertNull(store.state.undoHistory.selectedTabId)
        assertTrue(store.state.undoHistory.tabs.isEmpty())
        assertEquals(3, store.state.tabs.size)

        store.dispatch(
            TabListAction.RemoveAllPrivateTabsAction
        ).joinBlocking()

        assertNull(store.state.undoHistory.selectedTabId)
        assertEquals(1, store.state.undoHistory.tabs.size)
        assertEquals("https://reddit.com/r/firefox", store.state.undoHistory.tabs[0].url)
        assertEquals(2, store.state.tabs.size)

        store.dispatch(
            TabListAction.RemoveAllNormalTabsAction
        ).joinBlocking()

        assertEquals("pocket", store.state.undoHistory.selectedTabId)
        assertEquals(2, store.state.undoHistory.tabs.size)
        assertEquals("https://www.mozilla.org", store.state.undoHistory.tabs[0].url)
        assertEquals("https://getpocket.com", store.state.undoHistory.tabs[1].url)
        assertEquals(0, store.state.tabs.size)

        testDispatcher.withDispatchingPaused {
            // We need to pause the test dispatcher here to avoid it dispatching immediately.
            // Otherwise we deadlock the test here when we wait for the store to complete and
            // at the same time the middleware dispatches a coroutine on the dispatcher which will
            // also block on the store in SessionManager.restore().
            store.dispatch(UndoAction.RestoreRecoverableTabs).joinBlocking()
        }

        store.waitUntilIdle()

        assertNull(store.state.undoHistory.selectedTabId)
        assertTrue(store.state.undoHistory.tabs.isEmpty())
        assertEquals(0, store.state.undoHistory.tabs.size)
        assertEquals(2, store.state.tabs.size)
        assertEquals("https://www.mozilla.org", store.state.tabs[0].content.url)
        assertEquals("https://getpocket.com", store.state.tabs[1].content.url)
    }

    @Test
    fun `Undo History gets cleared after time`() {
        val waitDispatcher = TestCoroutineDispatcher()
        val waitScope = CoroutineScope(waitDispatcher)

        val store = BrowserStore(
            middleware = listOf(
                UndoMiddleware(clearAfterMillis = 60000, waitScope = waitScope)
            ),
            initialState = BrowserState(
                tabs = listOf(
                    createTab("https://www.mozilla.org", id = "mozilla"),
                    createTab("https://getpocket.com", id = "pocket"),
                    createTab("https://reddit.com/r/firefox", id = "reddit", private = true)
                ),
                selectedTabId = "pocket"
            )
        )
        assertEquals(3, store.state.tabs.size)
        assertEquals("https://getpocket.com", store.state.selectedTab!!.content.url)

        store.dispatch(
            TabListAction.RemoveAllNormalTabsAction
        ).joinBlocking()

        assertEquals(1, store.state.tabs.size)
        assertEquals("https://reddit.com/r/firefox", store.state.tabs[0].content.url)
        assertEquals("pocket", store.state.undoHistory.selectedTabId)
        assertEquals(2, store.state.undoHistory.tabs.size)
        assertEquals("https://www.mozilla.org", store.state.undoHistory.tabs[0].url)
        assertEquals("https://getpocket.com", store.state.undoHistory.tabs[1].url)

        waitDispatcher.advanceTimeBy(70000)
        waitDispatcher.advanceUntilIdle()
        store.waitUntilIdle()

        assertNull(store.state.undoHistory.selectedTabId)
        assertTrue(store.state.undoHistory.tabs.isEmpty())
        assertEquals(1, store.state.tabs.size)
        assertEquals("https://reddit.com/r/firefox", store.state.tabs[0].content.url)

        testDispatcher.withDispatchingPaused {
            // We need to pause the test dispatcher here to avoid it dispatching immediately.
            // Otherwise we deadlock the test here when we wait for the store to complete and
            // at the same time the middleware dispatches a coroutine on the dispatcher which will
            // also block on the store in SessionManager.restore().
            store.dispatch(UndoAction.RestoreRecoverableTabs).joinBlocking()
        }

        assertEquals(1, store.state.tabs.size)
        assertEquals("https://reddit.com/r/firefox", store.state.tabs[0].content.url)
    }
}

private fun TestCoroutineDispatcher.withDispatchingPaused(block: () -> Unit) {
    pauseDispatcher()
    block()
    resumeDispatcher()
    advanceUntilIdle()
}
