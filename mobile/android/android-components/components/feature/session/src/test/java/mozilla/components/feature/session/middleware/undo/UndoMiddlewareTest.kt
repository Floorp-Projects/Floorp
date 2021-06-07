/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

package mozilla.components.feature.session.middleware.undo

import kotlinx.coroutines.CoroutineScope
import kotlinx.coroutines.Dispatchers
import kotlinx.coroutines.test.TestCoroutineDispatcher
import kotlinx.coroutines.test.resetMain
import kotlinx.coroutines.test.setMain
import mozilla.components.browser.session.Session
import mozilla.components.browser.session.SessionManager
import mozilla.components.browser.state.action.UndoAction
import mozilla.components.browser.state.selector.selectedTab
import mozilla.components.browser.state.store.BrowserStore
import mozilla.components.support.test.ext.joinBlocking
import mozilla.components.support.test.libstate.ext.waitUntilIdle
import mozilla.components.support.test.mock
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
        val lookup = SessionManagerLookup()
        val store = BrowserStore(middleware = listOf(
            UndoMiddleware(lookup, clearAfterMillis = 60000)
        ))
        val manager = SessionManager(engine = mock(), store = store).apply {
            lookup.sessionManager = this
        }

        val mozilla = Session("https://www.mozilla.org")
        val pocket = Session("https://getpocket.com")

        manager.add(mozilla)
        manager.add(pocket)

        assertEquals(2, manager.size)
        assertEquals(2, store.state.tabs.size)
        assertEquals("https://www.mozilla.org", manager.selectedSessionOrThrow.url)
        assertEquals("https://www.mozilla.org", store.state.selectedTab!!.content.url)

        manager.remove(mozilla)

        assertEquals(1, manager.size)
        assertEquals(1, store.state.tabs.size)
        assertEquals("https://getpocket.com", manager.selectedSessionOrThrow.url)
        assertEquals("https://getpocket.com", store.state.selectedTab!!.content.url)

        testDispatcher.withDispatchingPaused {
            // We need to pause the test dispatcher here to avoid it dispatching immediately.
            // Otherwise we deadlock the test here when we wait for the store to complete and
            // at the same time the middleware dispatches a coroutine on the dispatcher which will
            // also block on the store in SessionManager.restore().
            store.dispatch(UndoAction.RestoreRecoverableTabs).joinBlocking()
        }

        store.waitUntilIdle()

        assertEquals(2, manager.size)
        assertEquals(2, store.state.tabs.size)
        assertEquals("https://www.mozilla.org", manager.selectedSessionOrThrow.url)
        assertEquals("https://www.mozilla.org", store.state.selectedTab!!.content.url)
    }

    @Test
    fun `Undo scenario - Removing list of tabs`() {
        val lookup = SessionManagerLookup()
        val store = BrowserStore(middleware = listOf(
            UndoMiddleware(lookup, clearAfterMillis = 60000)
        ))
        val manager = SessionManager(engine = mock(), store = store).apply {
            lookup.sessionManager = this
        }

        val mozilla = Session("https://www.mozilla.org")
        val pocket = Session("https://getpocket.com")
        val firefox = Session("https://firefox.com")

        manager.add(mozilla)
        manager.add(pocket)
        manager.add(firefox)

        assertEquals(3, manager.size)
        assertEquals(3, store.state.tabs.size)
        assertEquals("https://www.mozilla.org", manager.selectedSessionOrThrow.url)
        assertEquals("https://www.mozilla.org", store.state.selectedTab!!.content.url)

        manager.removeListOfSessions(listOf(mozilla.id, pocket.id))

        assertEquals(1, manager.size)
        assertEquals(1, store.state.tabs.size)
        assertEquals("https://firefox.com", manager.selectedSessionOrThrow.url)
        assertEquals("https://firefox.com", store.state.selectedTab!!.content.url)

        testDispatcher.withDispatchingPaused {
            // We need to pause the test dispatcher here to avoid it dispatching immediately.
            // Otherwise we deadlock the test here when we wait for the store to complete and
            // at the same time the middleware dispatches a coroutine on the dispatcher which will
            // also block on the store in SessionManager.restore().
            store.dispatch(UndoAction.RestoreRecoverableTabs).joinBlocking()
        }

        store.waitUntilIdle()

        assertEquals(3, manager.size)
        assertEquals(3, store.state.tabs.size)
        assertEquals("https://www.mozilla.org", manager.selectedSessionOrThrow.url)
        assertEquals("https://www.mozilla.org", store.state.selectedTab!!.content.url)
    }

    @Test
    fun `Undo scenario - Removing all normal tabs`() {
        val lookup = SessionManagerLookup()
        val store = BrowserStore(middleware = listOf(
            UndoMiddleware(lookup, clearAfterMillis = 60000)
        ))
        val manager = SessionManager(engine = mock(), store = store).apply {
            lookup.sessionManager = this
        }

        val mozilla = Session("https://www.mozilla.org")
        val pocket = Session("https://getpocket.com")
        val reddit = Session("https://reddit.com/r/firefox", private = true)

        manager.add(mozilla)
        manager.add(pocket)
        manager.add(reddit)
        manager.select(pocket)

        assertEquals(3, manager.size)
        assertEquals(3, store.state.tabs.size)
        assertEquals("https://getpocket.com", manager.selectedSessionOrThrow.url)
        assertEquals("https://getpocket.com", store.state.selectedTab!!.content.url)

        manager.removeNormalSessions()

        assertEquals(1, manager.size)
        assertEquals(1, store.state.tabs.size)
        assertNull(manager.selectedSession)
        assertNull(store.state.selectedTab)

        testDispatcher.withDispatchingPaused {
            // We need to pause the test dispatcher here to avoid it dispatching immediately.
            // Otherwise we deadlock the test here when we wait for the store to complete and
            // at the same time the middleware dispatches a coroutine on the dispatcher which will
            // also block on the store in SessionManager.restore().
            store.dispatch(UndoAction.RestoreRecoverableTabs).joinBlocking()
        }

        store.waitUntilIdle()

        assertEquals(3, manager.size)
        assertEquals(3, store.state.tabs.size)
        assertEquals("https://getpocket.com", manager.selectedSessionOrThrow.url)
        assertEquals("https://getpocket.com", store.state.selectedTab!!.content.url)
    }

    @Test
    fun `Undo History in State is written`() {
        val lookup = SessionManagerLookup()
        val store = BrowserStore(middleware = listOf(
            UndoMiddleware(lookup, clearAfterMillis = 60000)
        ))
        val manager = SessionManager(engine = mock(), store = store).apply {
            lookup.sessionManager = this
        }

        val mozilla = Session("https://www.mozilla.org", id = "mozilla")
        val pocket = Session("https://getpocket.com", id = "pocket")
        val reddit = Session("https://reddit.com/r/firefox", private = true, id = "reddit")

        manager.add(mozilla)
        manager.add(pocket)
        manager.add(reddit)
        manager.select(pocket)

        assertNull(store.state.undoHistory.selectedTabId)
        assertTrue(store.state.undoHistory.tabs.isEmpty())
        assertEquals(3, store.state.tabs.size)

        manager.removePrivateSessions()

        assertNull(store.state.undoHistory.selectedTabId)
        assertEquals(1, store.state.undoHistory.tabs.size)
        assertEquals("https://reddit.com/r/firefox", store.state.undoHistory.tabs[0].url)
        assertEquals(2, store.state.tabs.size)

        manager.removeNormalSessions()

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

        val lookup = SessionManagerLookup()
        val store = BrowserStore(middleware = listOf(
            UndoMiddleware(lookup, clearAfterMillis = 60000, waitScope = waitScope)
        ))
        val manager = SessionManager(engine = mock(), store = store).apply {
            lookup.sessionManager = this
        }

        val mozilla = Session("https://www.mozilla.org", id = "mozilla")
        val pocket = Session("https://getpocket.com", id = "pocket")
        val reddit = Session("https://reddit.com/r/firefox", private = true, id = "reddit")

        manager.add(mozilla)
        manager.add(pocket)
        manager.add(reddit)
        manager.select(pocket)

        assertEquals(3, manager.size)
        assertEquals(3, store.state.tabs.size)
        assertEquals("https://getpocket.com", manager.selectedSessionOrThrow.url)
        assertEquals("https://getpocket.com", store.state.selectedTab!!.content.url)

        manager.removeNormalSessions()

        assertEquals(1, manager.size)
        assertEquals("https://reddit.com/r/firefox", manager.sessions[0].url)
        assertEquals("pocket", store.state.undoHistory.selectedTabId)
        assertEquals(2, store.state.undoHistory.tabs.size)
        assertEquals("https://www.mozilla.org", store.state.undoHistory.tabs[0].url)
        assertEquals("https://getpocket.com", store.state.undoHistory.tabs[1].url)

        waitDispatcher.advanceTimeBy(70000)
        waitDispatcher.advanceUntilIdle()
        store.waitUntilIdle()

        assertNull(store.state.undoHistory.selectedTabId)
        assertTrue(store.state.undoHistory.tabs.isEmpty())
        assertEquals(1, manager.size)
        assertEquals("https://reddit.com/r/firefox", manager.sessions[0].url)

        testDispatcher.withDispatchingPaused {
            // We need to pause the test dispatcher here to avoid it dispatching immediately.
            // Otherwise we deadlock the test here when we wait for the store to complete and
            // at the same time the middleware dispatches a coroutine on the dispatcher which will
            // also block on the store in SessionManager.restore().
            store.dispatch(UndoAction.RestoreRecoverableTabs).joinBlocking()
        }

        assertEquals(1, manager.size)
        assertEquals("https://reddit.com/r/firefox", manager.sessions[0].url)
    }
}

private class SessionManagerLookup : () -> SessionManager {
    lateinit var sessionManager: SessionManager

    override operator fun invoke(): SessionManager {
        return sessionManager
    }
}

private fun TestCoroutineDispatcher.withDispatchingPaused(block: () -> Unit) {
    pauseDispatcher()
    block()
    resumeDispatcher()
    advanceUntilIdle()
}
