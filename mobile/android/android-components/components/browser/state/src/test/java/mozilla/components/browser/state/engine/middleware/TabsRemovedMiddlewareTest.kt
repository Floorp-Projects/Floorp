/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

package mozilla.components.browser.state.engine.middleware

import androidx.test.ext.junit.runners.AndroidJUnit4
import kotlinx.coroutines.CoroutineScope
import kotlinx.coroutines.runBlocking
import kotlinx.coroutines.test.TestCoroutineDispatcher
import mozilla.components.browser.state.action.BrowserAction
import mozilla.components.browser.state.action.CustomTabListAction
import mozilla.components.browser.state.action.EngineAction
import mozilla.components.browser.state.action.TabListAction
import mozilla.components.browser.state.selector.findCustomTab
import mozilla.components.browser.state.selector.findTab
import mozilla.components.browser.state.selector.findTabOrCustomTab
import mozilla.components.browser.state.state.BrowserState
import mozilla.components.browser.state.state.createCustomTab
import mozilla.components.browser.state.state.createTab
import mozilla.components.browser.state.store.BrowserStore
import mozilla.components.concept.engine.EngineSession
import mozilla.components.lib.state.Middleware
import mozilla.components.lib.state.MiddlewareContext
import mozilla.components.support.test.ext.joinBlocking
import mozilla.components.support.test.libstate.ext.waitUntilIdle
import mozilla.components.support.test.mock
import org.junit.After
import org.junit.Assert.assertNotNull
import org.junit.Assert.assertNull
import org.junit.Test
import org.junit.runner.RunWith
import org.mockito.Mockito.never
import org.mockito.Mockito.verify

@RunWith(AndroidJUnit4::class)
class TabsRemovedMiddlewareTest {

    private val dispatcher = TestCoroutineDispatcher()
    private val scope = CoroutineScope(dispatcher)

    @After
    fun tearDown() {
        dispatcher.cleanupTestCoroutines()
    }

    @Test
    fun `closes and unlinks engine session when tab is removed`() = runBlocking {
        val middleware = TabsRemovedMiddleware(scope)

        val tab = createTab("https://www.mozilla.org", id = "1")
        val store = BrowserStore(
            initialState = BrowserState(tabs = listOf(tab)),
            middleware = listOf(middleware, ConsumeRemoveTabActionsMiddleware())
        )

        val engineSession = linkEngineSession(store, tab.id)
        store.dispatch(TabListAction.RemoveTabAction(tab.id)).joinBlocking()
        store.waitUntilIdle()
        dispatcher.advanceUntilIdle()

        assertNull(store.state.findTab(tab.id)?.engineState?.engineSession)
        verify(engineSession).close()
    }

    @Test
    fun `closes and unlinks engine session when list of tabs are removed`() = runBlocking {
        val middleware = TabsRemovedMiddleware(scope)

        val tab1 = createTab("https://www.mozilla.org", id = "1", private = false)
        val tab2 = createTab("https://www.firefox.com", id = "2", private = false)
        val tab3 = createTab("https://www.getpocket.com", id = "3", private = false)

        val store = BrowserStore(
            initialState = BrowserState(tabs = listOf(tab1, tab2, tab3)),
            middleware = listOf(middleware, ConsumeRemoveTabActionsMiddleware())
        )

        val engineSession1 = linkEngineSession(store, tab1.id)
        val engineSession2 = linkEngineSession(store, tab2.id)
        val engineSession3 = linkEngineSession(store, tab3.id)

        store.dispatch(TabListAction.RemoveTabsAction(listOf(tab1.id, tab2.id))).joinBlocking()
        store.waitUntilIdle()
        dispatcher.advanceUntilIdle()

        assertNull(store.state.findTab(tab1.id)?.engineState?.engineSession)
        assertNull(store.state.findTab(tab2.id)?.engineState?.engineSession)
        assertNotNull(store.state.findTab(tab3.id)?.engineState?.engineSession)
        verify(engineSession1).close()
        verify(engineSession2).close()
        verify(engineSession3, never()).close()
    }

    @Test
    fun `closes and unlinks engine session when all normal tabs are removed`() = runBlocking {
        val middleware = TabsRemovedMiddleware(scope)

        val tab1 = createTab("https://www.mozilla.org", id = "1", private = false)
        val tab2 = createTab("https://www.firefox.com", id = "2", private = false)
        val tab3 = createTab("https://www.getpocket.com", id = "3", private = true)
        val store = BrowserStore(
            initialState = BrowserState(tabs = listOf(tab1, tab2, tab3)),
            middleware = listOf(middleware, ConsumeRemoveTabActionsMiddleware())
        )

        val engineSession1 = linkEngineSession(store, tab1.id)
        val engineSession2 = linkEngineSession(store, tab2.id)
        val engineSession3 = linkEngineSession(store, tab3.id)

        store.dispatch(TabListAction.RemoveAllNormalTabsAction).joinBlocking()
        store.waitUntilIdle()
        dispatcher.advanceUntilIdle()

        assertNull(store.state.findTab(tab1.id)?.engineState?.engineSession)
        assertNull(store.state.findTab(tab2.id)?.engineState?.engineSession)
        assertNotNull(store.state.findTab(tab3.id)?.engineState?.engineSession)
        verify(engineSession1).close()
        verify(engineSession2).close()
        verify(engineSession3, never()).close()
    }

    @Test
    fun `closes and unlinks engine session when all private tabs are removed`() = runBlocking {
        val middleware = TabsRemovedMiddleware(scope)

        val tab1 = createTab("https://www.mozilla.org", id = "1", private = true)
        val tab2 = createTab("https://www.firefox.com", id = "2", private = true)
        val tab3 = createTab("https://www.getpocket.com", id = "3", private = false)
        val store = BrowserStore(
            initialState = BrowserState(tabs = listOf(tab1, tab2, tab3)),
            middleware = listOf(middleware, ConsumeRemoveTabActionsMiddleware())
        )

        val engineSession1 = linkEngineSession(store, tab1.id)
        val engineSession2 = linkEngineSession(store, tab2.id)
        val engineSession3 = linkEngineSession(store, tab3.id)

        store.dispatch(TabListAction.RemoveAllPrivateTabsAction).joinBlocking()
        store.waitUntilIdle()
        dispatcher.advanceUntilIdle()

        assertNull(store.state.findTab(tab1.id)?.engineState?.engineSession)
        assertNull(store.state.findTab(tab2.id)?.engineState?.engineSession)
        assertNotNull(store.state.findTab(tab3.id)?.engineState?.engineSession)
        verify(engineSession1).close()
        verify(engineSession2).close()
        verify(engineSession3, never()).close()
    }

    @Test
    fun `closes and unlinks engine session when all tabs are removed`() = runBlocking {
        val middleware = TabsRemovedMiddleware(scope)

        val tab1 = createTab("https://www.mozilla.org", id = "1", private = true)
        val tab2 = createTab("https://www.firefox.com", id = "2", private = false)
        val tab3 = createCustomTab("https://www.getpocket.com", id = "3")
        val store = BrowserStore(
            initialState = BrowserState(tabs = listOf(tab1, tab2), customTabs = listOf(tab3)),
            middleware = listOf(middleware, ConsumeRemoveTabActionsMiddleware())
        )

        val engineSession1 = linkEngineSession(store, tab1.id)
        val engineSession2 = linkEngineSession(store, tab2.id)
        val engineSession3 = linkEngineSession(store, tab3.id)

        store.dispatch(TabListAction.RemoveAllTabsAction()).joinBlocking()
        store.waitUntilIdle()
        dispatcher.advanceUntilIdle()

        assertNull(store.state.findTab(tab1.id)?.engineState?.engineSession)
        assertNull(store.state.findTab(tab2.id)?.engineState?.engineSession)
        assertNotNull(store.state.findCustomTab(tab3.id)?.engineState?.engineSession)
        verify(engineSession1).close()
        verify(engineSession2).close()
        verify(engineSession3, never()).close()
    }

    @Test
    fun `closes and unlinks engine session when custom tab is removed`() = runBlocking {
        val middleware = TabsRemovedMiddleware(scope)

        val tab = createCustomTab("https://www.mozilla.org", id = "1")
        val store = BrowserStore(
            initialState = BrowserState(customTabs = listOf(tab)),
            middleware = listOf(middleware, ConsumeRemoveTabActionsMiddleware())
        )

        val engineSession = linkEngineSession(store, tab.id)
        store.dispatch(CustomTabListAction.RemoveCustomTabAction(tab.id)).joinBlocking()
        store.waitUntilIdle()
        dispatcher.advanceUntilIdle()

        assertNull(store.state.findTab(tab.id)?.engineState?.engineSession)
        verify(engineSession).close()
    }

    @Test
    fun `closes and unlinks engine session when all custom tabs are removed`() = runBlocking {
        val middleware = TabsRemovedMiddleware(scope)

        val tab1 = createCustomTab("https://www.mozilla.org", id = "1")
        val tab2 = createCustomTab("https://www.firefox.com", id = "2")
        val tab3 = createTab("https://www.getpocket.com", id = "3")
        val store = BrowserStore(
            initialState = BrowserState(customTabs = listOf(tab1, tab2), tabs = listOf(tab3)),
            middleware = listOf(middleware, ConsumeRemoveTabActionsMiddleware())
        )

        val engineSession1 = linkEngineSession(store, tab1.id)
        val engineSession2 = linkEngineSession(store, tab2.id)
        val engineSession3 = linkEngineSession(store, tab3.id)

        store.dispatch(CustomTabListAction.RemoveAllCustomTabsAction).joinBlocking()
        store.waitUntilIdle()
        dispatcher.advanceUntilIdle()

        assertNull(store.state.findCustomTab(tab1.id)?.engineState?.engineSession)
        assertNull(store.state.findCustomTab(tab2.id)?.engineState?.engineSession)
        assertNotNull(store.state.findTab(tab3.id)?.engineState?.engineSession)
        verify(engineSession1).close()
        verify(engineSession2).close()
        verify(engineSession3, never()).close()
    }

    private fun linkEngineSession(store: BrowserStore, tabId: String): EngineSession {
        val engineSession: EngineSession = mock()
        store.dispatch(EngineAction.LinkEngineSessionAction(tabId, engineSession)).joinBlocking()
        assertNotNull(store.state.findTabOrCustomTab(tabId)?.engineState?.engineSession)
        return engineSession
    }

    // This is to consume remove tab actions so we can assert that we properly unlink tabs
    // before they get removed. If we didn't do this the tab would already be gone once
    // TabsRemovedMiddleware processed the action.
    private class ConsumeRemoveTabActionsMiddleware : Middleware<BrowserState, BrowserAction> {
        override fun invoke(
            context: MiddlewareContext<BrowserState, BrowserAction>,
            next: (BrowserAction) -> Unit,
            action: BrowserAction
        ) {
            when (action) {
                is TabListAction.RemoveAllNormalTabsAction,
                is TabListAction.RemoveAllPrivateTabsAction,
                is TabListAction.RemoveAllTabsAction,
                is TabListAction.RemoveTabAction,
                is CustomTabListAction.RemoveAllCustomTabsAction,
                is CustomTabListAction.RemoveCustomTabAction -> return
            }

            next(action)
        }
    }
}
