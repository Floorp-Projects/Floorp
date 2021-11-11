/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

package org.mozilla.fenix.tabstray.browser

import io.mockk.every
import io.mockk.mockk
import io.mockk.verify
import mozilla.components.browser.state.state.createTab
import mozilla.components.support.test.libstate.ext.waitUntilIdle
import org.junit.Assert.assertEquals
import org.junit.Before
import org.junit.Test
import org.mozilla.fenix.components.metrics.Event
import org.mozilla.fenix.components.metrics.MetricController
import org.mozilla.fenix.tabstray.TabsTrayStore
import org.mozilla.fenix.utils.Settings

class TabSorterTest {
    private val settings: Settings = mockk()
    private val metrics: MetricController = mockk()
    private var inactiveTimestamp = 0L
    private val tabsTrayStore = TabsTrayStore()

    @Before
    fun setUp() {
        every { settings.inactiveTabsAreEnabled }.answers { true }
        every { settings.searchTermTabGroupsAreEnabled }.answers { true }
        every { metrics.track(any()) }.answers { } // do nothing
    }

    @Test
    fun `WHEN updated with one normal tab THEN adapter have only one normal tab and no header`() {
        val tabSorter = TabSorter(settings, metrics, tabsTrayStore)

        tabSorter.updateTabs(
            listOf(
                createTab(url = "url", id = "tab1", lastAccess = System.currentTimeMillis())
            ),
            selectedTabId = "tab1"
        )

        tabsTrayStore.waitUntilIdle()

        assertEquals(tabsTrayStore.state.inactiveTabs.size, 0)
        assertEquals(tabsTrayStore.state.searchTermGroups.size, 0)
        assertEquals(tabsTrayStore.state.normalTabs.size, 1)
    }

    @Test
    fun `WHEN updated with one normal tab and two search term tab THEN adapter have normal tab and a search group`() {
        val tabSorter = TabSorter(settings, metrics, tabsTrayStore)

        tabSorter.updateTabs(
            listOf(
                createTab(url = "url", id = "tab1", lastAccess = System.currentTimeMillis()),
                createTab(
                    url = "url",
                    id = "tab2",
                    lastAccess = System.currentTimeMillis(),
                    searchTerms = "mozilla"
                ),
                createTab(
                    url = "url",
                    id = "tab3",
                    lastAccess = System.currentTimeMillis(),
                    searchTerms = "mozilla"
                )
            ),
            selectedTabId = "tab1"
        )

        tabsTrayStore.waitUntilIdle()

        assertEquals(tabsTrayStore.state.inactiveTabs.size, 0)
        assertEquals(tabsTrayStore.state.searchTermGroups.size, 1)
        assertEquals(tabsTrayStore.state.normalTabs.size, 1)
    }

    @Test
    fun `WHEN updated with one normal tab, one inactive tab and two search term tab THEN adapter have normal tab, inactive tab and a search group`() {
        val tabSorter = TabSorter(settings, metrics, tabsTrayStore)

        tabSorter.updateTabs(
            listOf(
                createTab(url = "url", id = "tab1", lastAccess = System.currentTimeMillis()),
                createTab(
                    url = "url",
                    id = "tab2",
                    lastAccess = inactiveTimestamp,
                    createdAt = inactiveTimestamp
                ),
                createTab(
                    url = "url",
                    id = "tab3",
                    lastAccess = System.currentTimeMillis(),
                    searchTerms = "mozilla"
                ),
                createTab(
                    url = "url",
                    id = "tab4",
                    lastAccess = System.currentTimeMillis(),
                    searchTerms = "mozilla"
                )
            ),
            selectedTabId = "tab1"
        )

        tabsTrayStore.waitUntilIdle()

        assertEquals(tabsTrayStore.state.inactiveTabs.size, 1)
        assertEquals(tabsTrayStore.state.searchTermGroups.size, 1)
        assertEquals(tabsTrayStore.state.normalTabs.size, 1)
    }

    @Test
    fun `WHEN inactive tabs is off THEN adapter have no inactive tab`() {
        every { settings.inactiveTabsAreEnabled }.answers { false }
        val tabSorter = TabSorter(settings, metrics, tabsTrayStore)

        tabSorter.updateTabs(
            listOf(
                createTab(url = "url", id = "tab1", lastAccess = System.currentTimeMillis()),
                createTab(
                    url = "url",
                    id = "tab2",
                    lastAccess = inactiveTimestamp,
                    createdAt = inactiveTimestamp
                ),
                createTab(
                    url = "url",
                    id = "tab3",
                    lastAccess = System.currentTimeMillis(),
                    searchTerms = "mozilla"
                ),
                createTab(
                    url = "url",
                    id = "tab4",
                    lastAccess = System.currentTimeMillis(),
                    searchTerms = "mozilla"
                )
            ),
            selectedTabId = "tab1"
        )

        tabsTrayStore.waitUntilIdle()

        assertEquals(tabsTrayStore.state.inactiveTabs.size, 0)
        assertEquals(tabsTrayStore.state.searchTermGroups.size, 1)
        assertEquals(tabsTrayStore.state.normalTabs.size, 2)
    }

    @Test
    fun `WHEN search term tabs is off THEN adapter have no search term group`() {
        every { settings.searchTermTabGroupsAreEnabled }.answers { false }
        val tabSorter = TabSorter(settings, metrics, tabsTrayStore)

        tabSorter.updateTabs(
            listOf(
                createTab(url = "url", id = "tab1", lastAccess = System.currentTimeMillis()),
                createTab(
                    url = "url",
                    id = "tab2",
                    lastAccess = inactiveTimestamp,
                    createdAt = inactiveTimestamp
                ),
                createTab(
                    url = "url",
                    id = "tab3",
                    lastAccess = System.currentTimeMillis(),
                    searchTerms = "mozilla"
                ),
                createTab(
                    url = "url",
                    id = "tab4",
                    lastAccess = System.currentTimeMillis(),
                    searchTerms = "mozilla"
                )
            ),
            selectedTabId = "tab1"
        )

        tabsTrayStore.waitUntilIdle()

        assertEquals(tabsTrayStore.state.inactiveTabs.size, 1)
        assertEquals(tabsTrayStore.state.searchTermGroups.size, 0)
        assertEquals(tabsTrayStore.state.normalTabs.size, 3)
    }

    @Test
    fun `WHEN both inactive tabs and search term tabs are off THEN adapter have only normal tabs`() {
        every { settings.inactiveTabsAreEnabled }.answers { false }
        every { settings.searchTermTabGroupsAreEnabled }.answers { false }
        val tabSorter = TabSorter(settings, metrics, tabsTrayStore)

        tabSorter.updateTabs(
            listOf(
                createTab(url = "url", id = "tab1", lastAccess = System.currentTimeMillis()),
                createTab(
                    url = "url",
                    id = "tab2",
                    lastAccess = inactiveTimestamp
                ),
                createTab(
                    url = "url",
                    id = "tab3",
                    lastAccess = System.currentTimeMillis(),
                    searchTerms = "mozilla"
                ),
                createTab(
                    url = "url",
                    id = "tab4",
                    lastAccess = System.currentTimeMillis(),
                    searchTerms = "mozilla"
                )
            ),
            selectedTabId = "tab1"
        )

        tabsTrayStore.waitUntilIdle()

        assertEquals(tabsTrayStore.state.inactiveTabs.size, 0)
        assertEquals(tabsTrayStore.state.searchTermGroups.size, 0)
        assertEquals(tabsTrayStore.state.normalTabs.size, 4)
    }

    @Test
    fun `WHEN only one search term tab THEN there is no search group`() {
        val tabSorter = TabSorter(settings, metrics, tabsTrayStore)

        tabSorter.updateTabs(
            listOf(
                createTab(
                    url = "url", id = "tab1", lastAccess = System.currentTimeMillis(),
                    searchTerms = "mozilla"
                )
            ),
            selectedTabId = "tab1"
        )

        tabsTrayStore.waitUntilIdle()

        assertEquals(tabsTrayStore.state.inactiveTabs.size, 0)
        assertEquals(tabsTrayStore.state.searchTermGroups.size, 0)
        assertEquals(tabsTrayStore.state.normalTabs.size, 1)
    }

    @Test
    fun `WHEN remove second last one search term tab THEN search group is kept even if there's only one tab`() {
        val tabSorter = TabSorter(settings, metrics, tabsTrayStore)

        tabSorter.updateTabs(
            listOf(
                createTab(
                    url = "url", id = "tab1", lastAccess = System.currentTimeMillis(),
                    searchTerms = "mozilla"
                ),
                createTab(
                    url = "url", id = "tab2", lastAccess = System.currentTimeMillis(),
                    searchTerms = "mozilla"
                )
            ),
            selectedTabId = "tab1"
        )

        tabsTrayStore.waitUntilIdle()

        assertEquals(tabsTrayStore.state.inactiveTabs.size, 0)
        assertEquals(tabsTrayStore.state.searchTermGroups.size, 1)
        assertEquals(tabsTrayStore.state.normalTabs.size, 0)

        tabSorter.updateTabs(
            listOf(
                createTab(
                    url = "url", id = "tab1", lastAccess = System.currentTimeMillis(),
                    searchTerms = "mozilla"
                )
            ),
            selectedTabId = "tab1"
        )

        tabsTrayStore.waitUntilIdle()

        assertEquals(tabsTrayStore.state.inactiveTabs.size, 0)
        assertEquals(tabsTrayStore.state.searchTermGroups.size, 1)
        assertEquals(tabsTrayStore.state.normalTabs.size, 0)
    }

    @Test
    fun `GIVEN the inactive tabs feature is enabled WHEN the tray is opened THEN we report the number of inactive tabs`() {
        val tabSorter = TabSorter(settings, metrics, tabsTrayStore)

        tabSorter.updateTabs(
            listOf(
                createTab(url = "url", id = "tab1", createdAt = inactiveTimestamp),
                createTab(url = "url", id = "tab2", createdAt = inactiveTimestamp),
            ),
            selectedTabId = "tab2"
        )
        tabsTrayStore.waitUntilIdle()

        verify { metrics.track(Event.InactiveTabsCountUpdate(2)) }
    }

    @Test
    fun `GIVEN the inactive tabs feature is disabled WHEN the tray is opened THEN we report 0 for the number of inactive tabs`() {
        val tabSorter = TabSorter(settings, metrics, tabsTrayStore)
        every { settings.inactiveTabsAreEnabled }.answers { false }

        tabSorter.updateTabs(
            listOf(
                createTab(url = "url", id = "tab1", createdAt = inactiveTimestamp),
                createTab(url = "url", id = "tab2", createdAt = inactiveTimestamp),
            ),
            selectedTabId = "tab2"
        )
        tabsTrayStore.waitUntilIdle()

        verify { metrics.track(Event.InactiveTabsCountUpdate(0)) }
    }
}
