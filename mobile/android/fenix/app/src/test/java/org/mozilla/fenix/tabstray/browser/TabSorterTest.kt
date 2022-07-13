/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

package org.mozilla.fenix.tabstray.browser

import io.mockk.every
import io.mockk.mockk
import mozilla.components.browser.state.state.createTab
import mozilla.components.support.test.libstate.ext.waitUntilIdle
import org.junit.Assert.assertEquals
import org.junit.Before
import org.junit.Test
import org.mozilla.fenix.tabstray.TabsTrayStore
import org.mozilla.fenix.utils.Settings

class TabSorterTest {
    private val settings: Settings = mockk()
    private var inactiveTimestamp = 0L
    private val tabsTrayStore = TabsTrayStore()

    @Before
    fun setUp() {
        every { settings.inactiveTabsAreEnabled }.answers { true }
        every { settings.searchTermTabGroupsAreEnabled }.answers { true }
    }

    @Test
    fun `WHEN updated with one normal tab THEN adapter have only one normal tab`() {
        val tabSorter = TabSorter(settings, tabsTrayStore)

        tabSorter.updateTabs(
            listOf(
                createTab(url = "url", id = "tab1", lastAccess = System.currentTimeMillis())
            ),
            null,
            selectedTabId = "tab1"
        )

        tabsTrayStore.waitUntilIdle()

        assertEquals(tabsTrayStore.state.inactiveTabs.size, 0)
        assertEquals(tabsTrayStore.state.normalTabs.size, 1)
    }

    @Test
    fun `WHEN updated with one normal tab and one inactive tab THEN adapter have normal tab and inactive tab`() {
        val tabSorter = TabSorter(settings, tabsTrayStore)

        tabSorter.updateTabs(
            listOf(
                createTab(url = "url", id = "tab1", lastAccess = System.currentTimeMillis()),
                createTab(url = "url", id = "tab2", lastAccess = inactiveTimestamp, createdAt = inactiveTimestamp),
            ),
            tabPartition = null,
            selectedTabId = "tab1"
        )

        tabsTrayStore.waitUntilIdle()

        assertEquals(tabsTrayStore.state.inactiveTabs.size, 1)
        assertEquals(tabsTrayStore.state.normalTabs.size, 1)
    }

    @Test
    fun `WHEN inactive tabs is off THEN adapter have no inactive tab`() {
        every { settings.inactiveTabsAreEnabled }.answers { false }
        val tabSorter = TabSorter(settings, tabsTrayStore)

        tabSorter.updateTabs(
            listOf(
                createTab(url = "url", id = "tab1", lastAccess = System.currentTimeMillis()),
                createTab(
                    url = "url",
                    id = "tab2",
                    lastAccess = inactiveTimestamp,
                    createdAt = inactiveTimestamp
                ),
            ),
            tabPartition = null,
            selectedTabId = "tab1"
        )

        tabsTrayStore.waitUntilIdle()

        assertEquals(tabsTrayStore.state.inactiveTabs.size, 0)
        assertEquals(tabsTrayStore.state.normalTabs.size, 2)
    }

    @Test
    fun `WHEN inactive tabs are disabled THEN adapter have only normal tabs`() {
        every { settings.inactiveTabsAreEnabled }.answers { false }
        every { settings.searchTermTabGroupsAreEnabled }.answers { false }
        val tabSorter = TabSorter(settings, tabsTrayStore)

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
            tabPartition = null,
            selectedTabId = "tab1"
        )

        tabsTrayStore.waitUntilIdle()

        assertEquals(tabsTrayStore.state.inactiveTabs.size, 0)
        assertEquals(tabsTrayStore.state.normalTabs.size, 4)
    }
}
