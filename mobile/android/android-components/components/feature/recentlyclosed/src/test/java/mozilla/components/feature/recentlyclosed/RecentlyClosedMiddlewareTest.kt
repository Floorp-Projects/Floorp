/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

package mozilla.components.feature.recentlyclosed

import androidx.test.ext.junit.runners.AndroidJUnit4
import kotlinx.coroutines.ExperimentalCoroutinesApi
import kotlinx.coroutines.flow.flow
import mozilla.components.browser.state.action.RecentlyClosedAction
import mozilla.components.browser.state.action.TabListAction
import mozilla.components.browser.state.action.UndoAction
import mozilla.components.browser.state.state.BrowserState
import mozilla.components.browser.state.state.createTab
import mozilla.components.browser.state.state.recover.RecoverableTab
import mozilla.components.browser.state.state.recover.TabState
import mozilla.components.browser.state.store.BrowserStore
import mozilla.components.concept.engine.Engine
import mozilla.components.feature.session.middleware.undo.UndoMiddleware
import mozilla.components.support.test.argumentCaptor
import mozilla.components.support.test.eq
import mozilla.components.support.test.ext.joinBlocking
import mozilla.components.support.test.libstate.ext.waitUntilIdle
import mozilla.components.support.test.mock
import mozilla.components.support.test.rule.MainCoroutineRule
import mozilla.components.support.test.rule.runTestOnMain
import mozilla.components.support.test.whenever
import org.junit.Assert.assertEquals
import org.junit.Before
import org.junit.Rule
import org.junit.Test
import org.junit.runner.RunWith
import org.mockito.Mockito.times
import org.mockito.Mockito.verify
import org.mockito.Mockito.verifyNoMoreInteractions

@ExperimentalCoroutinesApi
@RunWith(AndroidJUnit4::class)
class RecentlyClosedMiddlewareTest {
    lateinit var store: BrowserStore
    lateinit var engine: Engine

    @get:Rule
    val coroutinesTestRule = MainCoroutineRule()
    private val dispatcher = coroutinesTestRule.testDispatcher
    private val scope = coroutinesTestRule.scope

    @Before
    fun setup() {
        store = mock()
        engine = mock()
    }

    // Test tab
    private val closedTab = RecoverableTab(
        engineSessionState = null,
        state = TabState(
            id = "tab-id",
            title = "Mozilla",
            url = "https://mozilla.org",
            lastAccess = 1234,
        ),
    )

    @Test
    fun `closed tab storage stores the provided tab on add tab action`() = runTestOnMain {
        val storage = mockStorage()
        val middleware = RecentlyClosedMiddleware(lazy { storage }, 5, scope)

        val store = BrowserStore(
            initialState = BrowserState(),
            middleware = listOf(middleware),
        )

        store.dispatch(RecentlyClosedAction.AddClosedTabsAction(listOf(closedTab))).joinBlocking()
        dispatcher.scheduler.advanceUntilIdle()
        store.waitUntilIdle()

        verify(storage).addTabsToCollectionWithMax(
            listOf(closedTab),
            5,
        )
    }

    @Test
    fun `closed tab storage adds normal tabs removed with TabListAction`() = runTestOnMain {
        val storage = mockStorage()
        val middleware = RecentlyClosedMiddleware(lazy { storage }, 5, scope)

        val tab = createTab("https://www.mozilla.org", private = false, id = "1234")
        val tab2 = createTab("https://www.firefox.com", private = false, id = "5678")

        val store = BrowserStore(
            initialState = BrowserState(
                tabs = listOf(tab, tab2),
            ),
            middleware = listOf(UndoMiddleware(mainScope = scope), middleware),
        )

        store.dispatch(TabListAction.RemoveTabsAction(listOf("1234", "5678"))).joinBlocking()
        store.dispatch(UndoAction.ClearRecoverableTabs(store.state.undoHistory.tag)).joinBlocking()

        dispatcher.scheduler.advanceUntilIdle()
        store.waitUntilIdle()

        val closedTabCaptor = argumentCaptor<List<RecoverableTab>>()
        verify(storage).addTabsToCollectionWithMax(
            closedTabCaptor.capture(),
            eq(5),
        )
        assertEquals(2, closedTabCaptor.value.size)
        assertEquals(tab.content.title, closedTabCaptor.value[0].state.title)
        assertEquals(tab.content.url, closedTabCaptor.value[0].state.url)
        assertEquals(tab2.content.title, closedTabCaptor.value[1].state.title)
        assertEquals(tab2.content.url, closedTabCaptor.value[1].state.url)
        assertEquals(
            tab.engineState.engineSessionState,
            closedTabCaptor.value[0].engineSessionState,
        )
        assertEquals(
            tab2.engineState.engineSessionState,
            closedTabCaptor.value[1].engineSessionState,
        )
    }

    @Test
    fun `closed tab storage adds a normal tab removed with TabListAction`() = runTestOnMain {
        val storage = mockStorage()
        val middleware = RecentlyClosedMiddleware(lazy { storage }, 5, scope)

        val tab = createTab("https://www.mozilla.org", private = false, id = "1234")

        val store = BrowserStore(
            initialState = BrowserState(
                tabs = listOf(tab),
            ),
            middleware = listOf(UndoMiddleware(mainScope = scope), middleware),
        )

        store.dispatch(TabListAction.RemoveTabAction("1234")).joinBlocking()
        store.dispatch(UndoAction.ClearRecoverableTabs(store.state.undoHistory.tag)).joinBlocking()

        dispatcher.scheduler.advanceUntilIdle()
        store.waitUntilIdle()

        val closedTabCaptor = argumentCaptor<List<RecoverableTab>>()
        verify(storage).addTabsToCollectionWithMax(
            closedTabCaptor.capture(),
            eq(5),
        )
        assertEquals(1, closedTabCaptor.value.size)
        assertEquals(tab.content.title, closedTabCaptor.value[0].state.title)
        assertEquals(tab.content.url, closedTabCaptor.value[0].state.url)
        assertEquals(
            tab.engineState.engineSessionState,
            closedTabCaptor.value[0].engineSessionState,
        )
    }

    @Test
    fun `closed tab storage does not add a private tab removed with TabListAction`() = runTestOnMain {
        val storage = mockStorage()
        val middleware = RecentlyClosedMiddleware(lazy { storage }, 5, scope)

        val tab = createTab("https://www.mozilla.org", private = true, id = "1234")

        val store = BrowserStore(
            initialState = BrowserState(
                tabs = listOf(tab),
            ),
            middleware = listOf(middleware),
        )

        store.dispatch(TabListAction.RemoveTabAction("1234")).joinBlocking()
        dispatcher.scheduler.advanceUntilIdle()
        store.waitUntilIdle()

        verify(storage).getTabs()
        verifyNoMoreInteractions(storage)
    }

    @Test
    fun `closed tab storage adds all normals tab removed with TabListAction RemoveAllNormalTabsAction`() = runTestOnMain {
        val storage = mockStorage()
        val middleware = RecentlyClosedMiddleware(lazy { storage }, 5, scope)

        val tab = createTab("https://www.mozilla.org", private = false, id = "1234")
        val tab2 = createTab("https://www.firefox.com", private = true, id = "3456")

        val store = BrowserStore(
            initialState = BrowserState(
                tabs = listOf(tab, tab2),
            ),
            middleware = listOf(UndoMiddleware(mainScope = scope), middleware),
        )

        store.dispatch(TabListAction.RemoveAllNormalTabsAction).joinBlocking()
        store.dispatch(UndoAction.ClearRecoverableTabs(store.state.undoHistory.tag)).joinBlocking()

        dispatcher.scheduler.advanceUntilIdle()
        store.waitUntilIdle()

        val closedTabCaptor = argumentCaptor<List<RecoverableTab>>()
        verify(storage).addTabsToCollectionWithMax(
            closedTabCaptor.capture(),
            eq(5),
        )
        assertEquals(1, closedTabCaptor.value.size)
        assertEquals(tab.content.title, closedTabCaptor.value[0].state.title)
        assertEquals(tab.content.url, closedTabCaptor.value[0].state.url)
        assertEquals(
            tab.engineState.engineSessionState,
            closedTabCaptor.value[0].engineSessionState,
        )
    }

    @Test
    fun `closed tab storage adds all normal tabs and no private tabs removed with TabListAction RemoveAllTabsAction`() = runTestOnMain {
        val storage = mockStorage()
        val middleware = RecentlyClosedMiddleware(lazy { storage }, 5, scope)

        val tab = createTab("https://www.mozilla.org", private = false, id = "1234")
        val tab2 = createTab("https://www.firefox.com", private = true, id = "3456")

        val store = BrowserStore(
            initialState = BrowserState(
                tabs = listOf(tab, tab2),
            ),
            middleware = listOf(UndoMiddleware(mainScope = scope), middleware),
        )

        store.dispatch(TabListAction.RemoveAllTabsAction()).joinBlocking()
        store.dispatch(UndoAction.ClearRecoverableTabs(store.state.undoHistory.tag)).joinBlocking()

        dispatcher.scheduler.advanceUntilIdle()
        store.waitUntilIdle()

        val closedTabCaptor = argumentCaptor<List<RecoverableTab>>()
        verify(storage).addTabsToCollectionWithMax(
            closedTabCaptor.capture(),
            eq(5),
        )
        assertEquals(1, closedTabCaptor.value.size)
        assertEquals(tab.content.title, closedTabCaptor.value[0].state.title)
        assertEquals(tab.content.url, closedTabCaptor.value[0].state.url)
        assertEquals(
            tab.engineState.engineSessionState,
            closedTabCaptor.value[0].engineSessionState,
        )
    }

    @Test
    fun `closed tabs storage adds tabs closed one after the other without clear actions in between`() = runTestOnMain {
        val storage = mockStorage()
        val middleware = RecentlyClosedMiddleware(lazy { storage }, 5, scope)

        val store = BrowserStore(
            middleware = listOf(UndoMiddleware(mainScope = scope), middleware),
        )

        store.dispatch(TabListAction.AddTabAction(createTab("https://www.mozilla.org", id = "tab1"))).joinBlocking()
        store.dispatch(TabListAction.AddTabAction(createTab("https://www.firefox.com", id = "tab2"))).joinBlocking()
        store.dispatch(TabListAction.AddTabAction(createTab("https://getpocket.com", id = "tab3"))).joinBlocking()
        store.dispatch(TabListAction.AddTabAction(createTab("https://theverge.com", id = "tab4"))).joinBlocking()
        store.dispatch(TabListAction.AddTabAction(createTab("https://www.google.com", id = "tab5"))).joinBlocking()
        assertEquals(5, store.state.tabs.size)

        store.dispatch(TabListAction.RemoveTabAction("tab2")).joinBlocking()
        store.dispatch(TabListAction.RemoveTabAction("tab3")).joinBlocking()
        store.dispatch(TabListAction.RemoveTabAction("tab1")).joinBlocking()
        store.dispatch(TabListAction.RemoveTabAction("tab5")).joinBlocking()

        store.dispatch(UndoAction.ClearRecoverableTabs(store.state.undoHistory.tag)).joinBlocking()

        assertEquals(1, store.state.tabs.size)
        assertEquals("tab4", store.state.selectedTabId)

        dispatcher.scheduler.advanceUntilIdle()
        store.waitUntilIdle()

        val closedTabCaptor = argumentCaptor<List<RecoverableTab>>()

        verify(storage, times(4)).addTabsToCollectionWithMax(
            closedTabCaptor.capture(),
            eq(5),
        )

        val tabs = closedTabCaptor.allValues
        assertEquals(4, tabs.size)

        tabs[0].also { tab ->
            assertEquals(1, tab.size)
            assertEquals("tab2", tab[0].state.id)
        }
        tabs[1].also { tab ->
            assertEquals(1, tab.size)
            assertEquals("tab3", tab[0].state.id)
        }
        tabs[2].also { tab ->
            assertEquals(1, tab.size)
            assertEquals("tab1", tab[0].state.id)
        }
        tabs[3].also { tab ->
            assertEquals(1, tab.size)
            assertEquals("tab5", tab[0].state.id)
        }
        Unit
    }

    @Test
    fun `fetch the tabs from the recently closed storage and load into browser state on initialize tab state action`() = runTestOnMain {
        val storage = mockStorage(tabs = listOf(closedTab.state))

        val middleware = RecentlyClosedMiddleware(lazy { storage }, 5, scope)
        val store = BrowserStore(initialState = BrowserState(), middleware = listOf(middleware))

        // Wait for Init action of store to be processed
        store.waitUntilIdle()

        // Now wait for Middleware to process Init action and store to process action from middleware
        dispatcher.scheduler.advanceUntilIdle()
        store.waitUntilIdle()

        verify(storage).getTabs()
        assertEquals(closedTab.state, store.state.closedTabs[0])
    }

    @Test
    fun `recently closed storage removes the provided tab on remove tab action`() = runTestOnMain {
        val storage = mockStorage()
        val middleware = RecentlyClosedMiddleware(lazy { storage }, 5, scope)

        val store = BrowserStore(
            initialState = BrowserState(
                closedTabs = listOf(
                    closedTab.state,
                ),
            ),
            middleware = listOf(middleware),
        )

        store.dispatch(RecentlyClosedAction.RemoveClosedTabAction(closedTab.state)).joinBlocking()
        dispatcher.scheduler.advanceUntilIdle()
        store.waitUntilIdle()

        dispatcher.scheduler.advanceUntilIdle()
        store.waitUntilIdle()
        verify(storage).removeTab(closedTab.state)
    }

    @Test
    fun `recently closed storage removes all tabs on remove all tabs action`() = runTestOnMain {
        val storage = mockStorage()
        val middleware = RecentlyClosedMiddleware(lazy { storage }, 5, scope)
        val store = BrowserStore(
            initialState = BrowserState(
                closedTabs = listOf(
                    closedTab.state,
                ),
            ),
            middleware = listOf(middleware),
        )

        store.dispatch(RecentlyClosedAction.RemoveAllClosedTabAction).joinBlocking()
        dispatcher.scheduler.advanceUntilIdle()
        store.waitUntilIdle()

        dispatcher.scheduler.advanceUntilIdle()
        store.waitUntilIdle()
        verify(storage).removeAllTabs()
    }
}

private suspend fun mockStorage(
    tabs: List<TabState> = emptyList(),
): RecentlyClosedMiddleware.Storage {
    val storage: RecentlyClosedMiddleware.Storage = mock()

    whenever(storage.getTabs()).thenReturn(
        flow {
            emit(tabs)
        },
    )

    return storage
}
