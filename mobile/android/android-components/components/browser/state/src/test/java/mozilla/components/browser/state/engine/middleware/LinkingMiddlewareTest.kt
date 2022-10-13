/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

package mozilla.components.browser.state.engine.middleware

import androidx.test.ext.junit.runners.AndroidJUnit4
import mozilla.components.browser.state.action.EngineAction
import mozilla.components.browser.state.action.TabListAction
import mozilla.components.browser.state.selector.findTab
import mozilla.components.browser.state.state.BrowserState
import mozilla.components.browser.state.state.createTab
import mozilla.components.browser.state.store.BrowserStore
import mozilla.components.concept.engine.EngineSession
import mozilla.components.support.test.any
import mozilla.components.support.test.ext.joinBlocking
import mozilla.components.support.test.libstate.ext.waitUntilIdle
import mozilla.components.support.test.mock
import mozilla.components.support.test.rule.MainCoroutineRule
import mozilla.components.support.test.rule.runTestOnMain
import org.junit.Assert.assertEquals
import org.junit.Assert.assertNotNull
import org.junit.Assert.assertNull
import org.junit.Rule
import org.junit.Test
import org.junit.runner.RunWith
import org.mockito.Mockito.anyString
import org.mockito.Mockito.never
import org.mockito.Mockito.verify

@RunWith(AndroidJUnit4::class)
class LinkingMiddlewareTest {
    @get:Rule
    val coroutinesTestRule = MainCoroutineRule()
    private val dispatcher = coroutinesTestRule.testDispatcher
    private val scope = coroutinesTestRule.scope

    @Test
    fun `loads URL after linking`() {
        val middleware = LinkingMiddleware(scope)

        val tab = createTab("https://www.mozilla.org", id = "1")
        val store = BrowserStore(
            initialState = BrowserState(tabs = listOf(tab)),
            middleware = listOf(middleware),
        )

        val engineSession: EngineSession = mock()
        store.dispatch(EngineAction.LinkEngineSessionAction(tab.id, engineSession)).joinBlocking()

        dispatcher.scheduler.advanceUntilIdle()

        verify(engineSession).loadUrl(tab.content.url)
    }

    @Test
    fun `loads URL with parent after linking`() {
        val middleware = LinkingMiddleware(scope)

        val parent = createTab("https://www.mozilla.org", id = "1")
        val child = createTab("https://www.firefox.com", id = "2", parent = parent)
        val store = BrowserStore(
            initialState = BrowserState(
                tabs = listOf(parent, child),
            ),
            middleware = listOf(middleware),
        )

        val parentEngineSession: EngineSession = mock()
        store.dispatch(EngineAction.LinkEngineSessionAction(parent.id, parentEngineSession)).joinBlocking()

        val childEngineSession: EngineSession = mock()
        store.dispatch(EngineAction.LinkEngineSessionAction(child.id, childEngineSession)).joinBlocking()

        dispatcher.scheduler.advanceUntilIdle()

        verify(childEngineSession).loadUrl(child.content.url, parentEngineSession)
    }

    @Test
    fun `loads URL without parent for extension URLs`() {
        val middleware = LinkingMiddleware(scope)

        val parent = createTab("https://www.mozilla.org", id = "1")
        val child = createTab("moz-extension://1234", id = "2", parent = parent)
        val store = BrowserStore(
            initialState = BrowserState(
                tabs = listOf(parent, child),
            ),
            middleware = listOf(middleware),
        )

        val parentEngineSession: EngineSession = mock()
        store.dispatch(EngineAction.LinkEngineSessionAction(parent.id, parentEngineSession)).joinBlocking()

        val childEngineSession: EngineSession = mock()
        store.dispatch(EngineAction.LinkEngineSessionAction(child.id, childEngineSession)).joinBlocking()

        dispatcher.scheduler.advanceUntilIdle()

        verify(childEngineSession).loadUrl(child.content.url)
    }

    @Test
    fun `skips loading URL if specified in action`() {
        val middleware = LinkingMiddleware(scope)

        val tab = createTab("https://www.mozilla.org", id = "1")
        val store = BrowserStore(
            initialState = BrowserState(tabs = listOf(tab)),
            middleware = listOf(middleware),
        )

        val engineSession: EngineSession = mock()
        store.dispatch(EngineAction.LinkEngineSessionAction(tab.id, engineSession, skipLoading = true)).joinBlocking()

        dispatcher.scheduler.advanceUntilIdle()

        verify(engineSession, never()).loadUrl(tab.content.url)
    }

    @Test
    fun `does nothing if linked tab does not exist`() {
        val middleware = LinkingMiddleware(scope)

        val store = BrowserStore(
            initialState = BrowserState(tabs = listOf()),
            middleware = listOf(middleware),
        )

        val engineSession: EngineSession = mock()
        store.dispatch(EngineAction.LinkEngineSessionAction("invalid", engineSession)).joinBlocking()

        dispatcher.scheduler.advanceUntilIdle()

        verify(engineSession, never()).loadUrl(anyString(), any(), any(), any())
    }

    @Test
    fun `registers engine observer after linking`() = runTestOnMain {
        val tab1 = createTab("https://www.mozilla.org", id = "1")
        val tab2 = createTab("https://www.mozilla.org", id = "2")

        val middleware = LinkingMiddleware(scope)

        val store = BrowserStore(
            initialState = BrowserState(tabs = listOf(tab1, tab2)),
            middleware = listOf(middleware),
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

        store.waitUntilIdle()

        assertEquals("test", store.state.tabs[1].content.title)
    }

    @Test
    fun `unregisters engine observer before unlinking`() = runTestOnMain {
        val tab1 = createTab("https://www.mozilla.org", id = "1")
        val tab2 = createTab("https://www.mozilla.org", id = "2")

        val middleware = LinkingMiddleware(scope)

        val store = BrowserStore(
            initialState = BrowserState(tabs = listOf(tab1, tab2)),
            middleware = listOf(middleware),
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

    @Test
    fun `registers engine observer when tab is added with engine session`() = runTestOnMain {
        val engineSession: EngineSession = mock()
        val tab1 = createTab("https://www.mozilla.org", id = "1")
        val tab2 = createTab("https://www.mozilla.org", id = "2", engineSession = engineSession)

        val middleware = LinkingMiddleware(scope)

        val store = BrowserStore(
            initialState = BrowserState(),
            middleware = listOf(middleware),
        )

        store.dispatch(TabListAction.AddTabAction(tab1)).joinBlocking()
        store.dispatch(TabListAction.AddTabAction(tab2)).joinBlocking()
        store.waitUntilIdle()

        // We only have a session for tab2 so we should only register an observer for tab2
        val engineObserver = store.state.findTab(tab2.id)?.engineState?.engineObserver
        assertNotNull(engineObserver)
        verify(engineSession).register(engineObserver!!)
        engineObserver.onTitleChange("test")
        store.waitUntilIdle()

        assertEquals("test", store.state.tabs[1].content.title)
    }
}
