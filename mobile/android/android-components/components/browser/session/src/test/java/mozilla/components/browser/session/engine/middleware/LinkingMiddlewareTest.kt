/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

package mozilla.components.browser.session.engine.middleware

import androidx.test.ext.junit.runners.AndroidJUnit4
import kotlinx.coroutines.CoroutineScope
import kotlinx.coroutines.Dispatchers
import kotlinx.coroutines.cancel
import kotlinx.coroutines.runBlocking
import kotlinx.coroutines.test.TestCoroutineDispatcher
import kotlinx.coroutines.test.resetMain
import kotlinx.coroutines.test.setMain
import mozilla.components.browser.session.Session
import mozilla.components.browser.state.action.EngineAction
import mozilla.components.browser.state.selector.findTab
import mozilla.components.browser.state.state.BrowserState
import mozilla.components.browser.state.state.createTab
import mozilla.components.browser.state.store.BrowserStore
import mozilla.components.concept.engine.EngineSession
import mozilla.components.support.test.any
import mozilla.components.support.test.ext.joinBlocking
import mozilla.components.support.test.libstate.ext.waitUntilIdle
import mozilla.components.support.test.mock
import mozilla.components.support.test.whenever
import org.junit.After
import org.junit.Assert.assertNotNull
import org.junit.Assert.assertNull
import org.junit.Before
import org.junit.Test
import org.junit.runner.RunWith
import org.mockito.Mockito.anyString
import org.mockito.Mockito.never
import org.mockito.Mockito.verify

@RunWith(AndroidJUnit4::class)
class LinkingMiddlewareTest {
    private lateinit var dispatcher: TestCoroutineDispatcher
    private lateinit var scope: CoroutineScope

    @Before
    fun setUp() {
        dispatcher = TestCoroutineDispatcher()
        scope = CoroutineScope(dispatcher)

        Dispatchers.setMain(dispatcher)
    }

    @After
    fun tearDown() {
        dispatcher.cleanupTestCoroutines()
        scope.cancel()

        Dispatchers.resetMain()
    }

    @Test
    fun `loads URL after linking`() {
        val middleware = LinkingMiddleware(scope) { null }

        val tab = createTab("https://www.mozilla.org", id = "1")
        val store = BrowserStore(
            initialState = BrowserState(tabs = listOf(tab)),
            middleware = listOf(middleware)
        )

        val engineSession: EngineSession = mock()
        store.dispatch(EngineAction.LinkEngineSessionAction(tab.id, engineSession)).joinBlocking()

        dispatcher.advanceUntilIdle()

        verify(engineSession).loadUrl(tab.content.url)
    }

    @Test
    fun `loads URL with parent after linking`() {
        val middleware = LinkingMiddleware(scope) { null }

        val parent = createTab("https://www.mozilla.org", id = "1")
        val child = createTab("https://www.firefox.com", id = "2", parent = parent)
        val store = BrowserStore(
            initialState = BrowserState(
                tabs = listOf(parent, child)
            ),
            middleware = listOf(middleware)
        )

        val parentEngineSession: EngineSession = mock()
        store.dispatch(EngineAction.LinkEngineSessionAction(parent.id, parentEngineSession)).joinBlocking()

        val childEngineSession: EngineSession = mock()
        store.dispatch(EngineAction.LinkEngineSessionAction(child.id, childEngineSession)).joinBlocking()

        dispatcher.advanceUntilIdle()

        verify(childEngineSession).loadUrl(child.content.url, parentEngineSession)
    }

    @Test
    fun `loads URL without parent for extension URLs`() {
        val middleware = LinkingMiddleware(scope) { null }

        val parent = createTab("https://www.mozilla.org", id = "1")
        val child = createTab("moz-extension://1234", id = "2", parent = parent)
        val store = BrowserStore(
            initialState = BrowserState(
                tabs = listOf(parent, child)
            ),
            middleware = listOf(middleware)
        )

        val parentEngineSession: EngineSession = mock()
        store.dispatch(EngineAction.LinkEngineSessionAction(parent.id, parentEngineSession)).joinBlocking()

        val childEngineSession: EngineSession = mock()
        store.dispatch(EngineAction.LinkEngineSessionAction(child.id, childEngineSession)).joinBlocking()

        dispatcher.advanceUntilIdle()

        verify(childEngineSession).loadUrl(child.content.url)
    }

    @Test
    fun `skips loading URL if specified in action`() {
        val middleware = LinkingMiddleware(scope) { null }

        val tab = createTab("https://www.mozilla.org", id = "1")
        val store = BrowserStore(
            initialState = BrowserState(tabs = listOf(tab)),
            middleware = listOf(middleware)
        )

        val engineSession: EngineSession = mock()
        store.dispatch(EngineAction.LinkEngineSessionAction(tab.id, engineSession, skipLoading = true)).joinBlocking()

        dispatcher.advanceUntilIdle()

        verify(engineSession, never()).loadUrl(tab.content.url)
    }

    @Test
    fun `does nothing if linked tab does not exist`() {
        val middleware = LinkingMiddleware(scope) { null }

        val store = BrowserStore(
            initialState = BrowserState(tabs = listOf()),
            middleware = listOf(middleware)
        )

        val engineSession: EngineSession = mock()
        store.dispatch(EngineAction.LinkEngineSessionAction("invalid", engineSession)).joinBlocking()

        dispatcher.advanceUntilIdle()

        verify(engineSession, never()).loadUrl(anyString(), any(), any(), any())
    }

    @Test
    fun `registers engine observer after linking`() = runBlocking {
        val tab1 = createTab("https://www.mozilla.org", id = "1")
        val tab2 = createTab("https://www.mozilla.org", id = "2")

        val session2: Session = mock()
        whenever(session2.id).thenReturn(tab2.id)
        val sessionLookup = { id: String -> if (id == tab2.id) session2 else null }
        val middleware = LinkingMiddleware(scope, sessionLookup)

        val store = BrowserStore(
            initialState = BrowserState(tabs = listOf(tab1, tab2)),
            middleware = listOf(middleware)
        )

        val engineSession1: EngineSession = mock()
        val engineSession2: EngineSession = mock()
        store.dispatch(EngineAction.LinkEngineSessionAction(tab1.id, engineSession1)).joinBlocking()
        store.dispatch(EngineAction.LinkEngineSessionAction(tab2.id, engineSession2)).joinBlocking()
        store.waitUntilIdle()

        // We only have a session for tab2 so we should only register an observer for tab2
        val engineObserver = store.state.findTab(tab2.id)?.engineState?.engineObserver
        assertNotNull(engineObserver)

        verify(engineSession2).register(engineObserver!!)
        engineObserver.onTitleChange("test")
        verify(session2).title = "test"
    }

    @Test
    fun `unregisters engine observer before unlinking`() = runBlocking {
        val tab1 = createTab("https://www.mozilla.org", id = "1")
        val tab2 = createTab("https://www.mozilla.org", id = "2")

        val session1: Session = mock()
        whenever(session1.id).thenReturn(tab1.id)

        val sessionLookup = { id: String -> if (id == tab1.id) session1 else null }
        val middleware = LinkingMiddleware(scope, sessionLookup)

        val store = BrowserStore(
            initialState = BrowserState(tabs = listOf(tab1, tab2)),
            middleware = listOf(middleware)
        )

        val engineSession: EngineSession = mock()
        store.dispatch(EngineAction.LinkEngineSessionAction(tab1.id, engineSession)).joinBlocking()
        store.waitUntilIdle()
        assertNotNull(store.state.findTab(tab1.id)?.engineState?.engineObserver)
        assertNull(store.state.findTab(tab2.id)?.engineState?.engineObserver)

        store.dispatch(EngineAction.UnlinkEngineSessionAction(tab1.id)).joinBlocking()
        store.dispatch(EngineAction.UnlinkEngineSessionAction(tab2.id)).joinBlocking()
        store.waitUntilIdle()
        assertNull(store.state.findTab(tab1.id)?.engineState?.engineObserver)
        assertNull(store.state.findTab(tab2.id)?.engineState?.engineObserver)
    }
}
