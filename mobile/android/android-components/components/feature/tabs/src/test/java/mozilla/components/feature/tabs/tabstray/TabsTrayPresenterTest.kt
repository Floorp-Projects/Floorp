/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

package mozilla.components.feature.tabs.tabstray

import android.view.View
import androidx.lifecycle.LifecycleOwner
import androidx.test.ext.junit.runners.AndroidJUnit4
import kotlinx.coroutines.Dispatchers
import kotlinx.coroutines.ExperimentalCoroutinesApi
import kotlinx.coroutines.test.TestCoroutineDispatcher
import kotlinx.coroutines.test.resetMain
import kotlinx.coroutines.test.setMain
import mozilla.components.browser.state.action.TabListAction
import mozilla.components.browser.state.state.BrowserState
import mozilla.components.browser.state.state.createTab
import mozilla.components.browser.state.store.BrowserStore
import mozilla.components.concept.tabstray.Tabs
import mozilla.components.concept.tabstray.TabsTray
import mozilla.components.feature.tabs.ext.toTabs
import mozilla.components.support.test.ext.joinBlocking
import mozilla.components.support.test.mock
import org.junit.After
import org.junit.Assert.assertEquals
import org.junit.Assert.assertFalse
import org.junit.Assert.assertNotNull
import org.junit.Assert.assertTrue
import org.junit.Before
import org.junit.Test
import org.junit.runner.RunWith
import org.mockito.Mockito.spy
import org.mockito.Mockito.times
import org.mockito.Mockito.verify
import org.mockito.Mockito.verifyNoMoreInteractions

@RunWith(AndroidJUnit4::class)
class TabsTrayPresenterTest {
    private val testDispatcher = TestCoroutineDispatcher()

    @Before
    fun setUp() {
        Dispatchers.setMain(testDispatcher)
    }

    @After
    @ExperimentalCoroutinesApi
    fun tearDown() {
        Dispatchers.resetMain()
        testDispatcher.cleanupTestCoroutines()
    }

    @Test
    fun `initial set of sessions will be passed to tabs tray`() {
        val store = BrowserStore(
            BrowserState(
                tabs = listOf(
                    createTab("https://www.mozilla.org", id = "a"),
                    createTab("https://getpocket.com", id = "b")
                ),
                selectedTabId = "a"
            )
        )

        val tabsTray: MockedTabsTray = spy(MockedTabsTray())
        val presenter = TabsTrayPresenter(
            tabsTray,
            store,
            tabsFilter = { true },
            closeTabsTray = mock()
        )

        verifyNoMoreInteractions(tabsTray)

        presenter.start()

        testDispatcher.advanceUntilIdle()

        assertNotNull(tabsTray.updateTabs)

        assertEquals(0, tabsTray.updateTabs!!.selectedIndex)
        assertEquals(2, tabsTray.updateTabs!!.list.size)
        assertEquals("https://www.mozilla.org", tabsTray.updateTabs!!.list[0].url)
        assertEquals("https://getpocket.com", tabsTray.updateTabs!!.list[1].url)

        presenter.stop()
    }

    @Test
    fun `tab tray will get updated if session gets added`() {
        val store = BrowserStore(
            BrowserState(
                tabs = listOf(
                    createTab("https://www.mozilla.org", id = "a"),
                    createTab("https://getpocket.com", id = "b")
                ),
                selectedTabId = "a"
            )
        )

        val tabsTray: MockedTabsTray = spy(MockedTabsTray())
        val presenter = TabsTrayPresenter(
            tabsTray,
            store,
            tabsFilter = { true },
            closeTabsTray = mock()
        )

        presenter.start()

        testDispatcher.advanceUntilIdle()

        assertEquals(2, tabsTray.updateTabs!!.list.size)

        store.dispatch(
            TabListAction.AddTabAction(
                createTab("https://developer.mozilla.org/")
            )
        ).joinBlocking()

        assertEquals(3, tabsTray.updateTabs!!.list.size)

        verify(tabsTray).onTabsInserted(2, 1)

        presenter.stop()
    }

    @Test
    fun `tabs tray will get updated if session gets removed`() {
        val store = BrowserStore(
            BrowserState(
                tabs = listOf(
                    createTab("https://www.mozilla.org", id = "a"),
                    createTab("https://getpocket.com", id = "b")
                ),
                selectedTabId = "a"
            )
        )

        val tabsTray: MockedTabsTray = spy(MockedTabsTray())
        val presenter = TabsTrayPresenter(
            tabsTray,
            store,
            tabsFilter = { true },
            closeTabsTray = mock()
        )

        presenter.start()

        testDispatcher.advanceUntilIdle()

        assertEquals(2, tabsTray.updateTabs!!.list.size)

        store.dispatch(TabListAction.RemoveTabAction("a")).joinBlocking()
        testDispatcher.advanceUntilIdle()

        assertEquals(1, tabsTray.updateTabs!!.list.size)
        verify(tabsTray).onTabsRemoved(0, 1)

        store.dispatch(TabListAction.RemoveTabAction("b")).joinBlocking()
        testDispatcher.advanceUntilIdle()

        assertEquals(0, tabsTray.updateTabs!!.list.size)
        verify(tabsTray, times(2)).onTabsRemoved(0, 1)

        presenter.stop()
    }

    @Test
    fun `tabs tray will get updated if all sessions get removed`() {
        val store = BrowserStore(
            BrowserState(
                tabs = listOf(
                    createTab("https://www.mozilla.org", id = "a"),
                    createTab("https://getpocket.com", id = "b")
                ),
                selectedTabId = "a"
            )
        )

        val tabsTray: MockedTabsTray = spy(MockedTabsTray())
        val presenter = TabsTrayPresenter(
            tabsTray,
            store,
            tabsFilter = { true },
            closeTabsTray = mock()
        )

        presenter.start()

        testDispatcher.advanceUntilIdle()

        assertEquals(2, tabsTray.updateTabs!!.list.size)

        store.dispatch(TabListAction.RemoveAllTabsAction()).joinBlocking()
        testDispatcher.advanceUntilIdle()

        assertEquals(0, tabsTray.updateTabs!!.list.size)

        verify(tabsTray).onTabsRemoved(0, 2)

        presenter.stop()
    }

    @Test
    fun `tabs tray will get updated if selection changes`() {
        val store = BrowserStore(
            BrowserState(
                tabs = listOf(
                    createTab("https://www.mozilla.org", id = "a"),
                    createTab("https://getpocket.com", id = "b"),
                    createTab("https://developer.mozilla.org", id = "c"),
                    createTab("https://www.firefox.com", id = "d"),
                    createTab("https://www.google.com", id = "e")
                ),
                selectedTabId = "a"
            )
        )

        val tabsTray: MockedTabsTray = spy(MockedTabsTray())
        val presenter = TabsTrayPresenter(
            tabsTray,
            store,
            tabsFilter = { true },
            closeTabsTray = mock()
        )

        presenter.start()
        testDispatcher.advanceUntilIdle()

        assertEquals(5, tabsTray.updateTabs!!.list.size)
        assertEquals(0, tabsTray.updateTabs!!.selectedIndex)

        store.dispatch(TabListAction.SelectTabAction("d")).joinBlocking()
        testDispatcher.advanceUntilIdle()

        println("Selection: " + store.state.selectedTabId)
        assertEquals(3, tabsTray.updateTabs!!.selectedIndex)

        verify(tabsTray).onTabsChanged(0, 1)
        verify(tabsTray).onTabsChanged(3, 1)
    }

    @Test
    fun `presenter will close tabs tray when all sessions get removed`() {
        val store = BrowserStore(
            BrowserState(
                tabs = listOf(
                    createTab("https://www.mozilla.org", id = "a"),
                    createTab("https://getpocket.com", id = "b"),
                    createTab("https://developer.mozilla.org", id = "c"),
                    createTab("https://www.firefox.com", id = "d"),
                    createTab("https://www.google.com", id = "e")
                ),
                selectedTabId = "a"
            )
        )

        var closed = false

        val tabsTray: MockedTabsTray = spy(MockedTabsTray())
        val presenter = TabsTrayPresenter(
            tabsTray,
            store,
            tabsFilter = { true },
            closeTabsTray = { closed = true })

        presenter.start()
        testDispatcher.advanceUntilIdle()

        assertFalse(closed)

        store.dispatch(TabListAction.RemoveAllTabsAction()).joinBlocking()
        testDispatcher.advanceUntilIdle()

        assertTrue(closed)

        presenter.stop()
    }

    @Test
    fun `presenter will close tabs tray when last session gets removed`() {
        val store = BrowserStore(
            BrowserState(
                tabs = listOf(
                    createTab("https://www.mozilla.org", id = "a"),
                    createTab("https://getpocket.com", id = "b")
                ),
                selectedTabId = "a"
            )
        )

        var closed = false

        val tabsTray: MockedTabsTray = spy(MockedTabsTray())
        val presenter = TabsTrayPresenter(
            tabsTray,
            store,
            tabsFilter = { true },
            closeTabsTray = { closed = true })

        presenter.start()
        testDispatcher.advanceUntilIdle()

        assertFalse(closed)

        store.dispatch(TabListAction.RemoveTabAction("a")).joinBlocking()
        testDispatcher.advanceUntilIdle()

        assertFalse(closed)

        store.dispatch(TabListAction.RemoveTabAction("b")).joinBlocking()
        testDispatcher.advanceUntilIdle()

        assertTrue(closed)

        presenter.stop()
    }

    @Test
    fun `presenter calls update and display sessions when calculating diff`() {
        val store = BrowserStore(
            BrowserState(
                tabs = listOf(
                    createTab("https://www.mozilla.org", id = "a"),
                    createTab("https://getpocket.com", id = "b")
                ),
                selectedTabId = "a"
            )
        )

        val tabsTray: MockedTabsTray = spy(MockedTabsTray())
        val presenter = TabsTrayPresenter(
            tabsTray,
            store,
            tabsFilter = { true },
            closeTabsTray = mock()
        )

        presenter.start()
        testDispatcher.advanceUntilIdle()

        val tabs = BrowserState(
            tabs = listOf(
                createTab("https://www.firefox.com", id = "c")
            )
        ).toTabs()

        presenter.updateTabs(tabs)
        testDispatcher.advanceUntilIdle()

        verify(tabsTray).updateTabs(tabs)
    }

    @Test
    fun `presenter invokes session filtering`() {
        val store = BrowserStore(
            BrowserState(
                tabs = listOf(
                    createTab("https://www.mozilla.org", id = "a"),
                    createTab("https://getpocket.com", id = "b", private = true)
                ),
                selectedTabId = "a"
            )
        )

        val tabsTray: MockedTabsTray = spy(MockedTabsTray())
        val presenter = TabsTrayPresenter(
            tabsTray,
            store,
            tabsFilter = { it.content.private },
            closeTabsTray = mock()
        )

        presenter.start()
        testDispatcher.advanceUntilIdle()

        assertTrue(tabsTray.updateTabs?.list?.size == 1)
    }

    @Test
    fun `tabs tray should not invoke the close callback on start`() {
        val store = BrowserStore(
            BrowserState(
                tabs = emptyList(),
                selectedTabId = null
            )
        )

        var invoked = false
        val tabsTray: MockedTabsTray = spy(MockedTabsTray())
        val presenter = TabsTrayPresenter(
            tabsTray,
            store,
            tabsFilter = { it.content.private },
            closeTabsTray = { invoked = true })

        presenter.start()
        testDispatcher.advanceUntilIdle()

        assertFalse(invoked)
    }
}

private class MockedTabsTray : TabsTray {
    var updateTabs: Tabs? = null

    override fun updateTabs(tabs: Tabs) {
        updateTabs = tabs
    }

    override fun onTabsInserted(position: Int, count: Int) {}

    override fun onTabsRemoved(position: Int, count: Int) {}

    override fun onTabsMoved(fromPosition: Int, toPosition: Int) {}

    override fun onTabsChanged(position: Int, count: Int) {}

    override fun register(observer: TabsTray.Observer) {}

    override fun register(observer: TabsTray.Observer, owner: LifecycleOwner, autoPause: Boolean) {}

    override fun register(observer: TabsTray.Observer, view: View) {}

    override fun unregister(observer: TabsTray.Observer) {}

    override fun unregisterObservers() {}

    override fun notifyObservers(block: TabsTray.Observer.() -> Unit) {}

    override fun notifyAtLeastOneObserver(block: TabsTray.Observer.() -> Unit) {}

    override fun <R> wrapConsumers(block: TabsTray.Observer.(R) -> Boolean): List<(R) -> Boolean> = emptyList()

    override fun isObserved(): Boolean = false

    override fun pauseObserver(observer: TabsTray.Observer) {}

    override fun resumeObserver(observer: TabsTray.Observer) {}

    override fun isTabSelected(tabs: Tabs, position: Int): Boolean = false
}
