/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

package org.mozilla.fenix.tabstray

import io.mockk.every
import io.mockk.mockk
import io.mockk.verify
import mozilla.components.support.test.libstate.ext.waitUntilIdle
import org.junit.Assert.assertEquals
import org.junit.Before
import org.junit.Test
import org.mozilla.fenix.components.metrics.Event
import org.mozilla.fenix.components.metrics.MetricController
import org.mozilla.fenix.tabstray.browser.TabGroup

class TabsTrayMiddlewareTest {

    private lateinit var store: TabsTrayStore
    private lateinit var tabsTrayMiddleware: TabsTrayMiddleware
    private lateinit var metrics: MetricController

    @Before
    fun setUp() {
        metrics = mockk(relaxed = true)
        tabsTrayMiddleware = TabsTrayMiddleware(
            metrics
        )
        store = TabsTrayStore(
            middlewares = listOf(tabsTrayMiddleware),
            initialState = TabsTrayState()
        )
    }

    @Test
    fun `WHEN search term groups are updated AND there is at least one group THEN report the average tabs per group`() {
        store.dispatch(TabsTrayAction.UpdateSearchGroupTabs(generateSearchTermTabGroupsForAverage()))
        store.waitUntilIdle()
        verify { metrics.track(Event.AverageTabsPerSearchTermGroup(5.0)) }
    }

    @Test
    fun `WHEN search term groups are updated AND there is at least one group THEN report the distribution of tab sizes`() {
        store.dispatch(TabsTrayAction.UpdateSearchGroupTabs(generateSearchTermTabGroupsForDistribution()))
        store.waitUntilIdle()
        verify { metrics.track(Event.SearchTermGroupSizeDistribution(listOf(3L, 2L, 1L, 4L))) }
    }

    @Test
    fun `WHEN search term groups are updated THEN report the count of search term tab groups`() {
        store.dispatch(TabsTrayAction.UpdateSearchGroupTabs(emptyList()))
        store.waitUntilIdle()
        verify { metrics.track(Event.SearchTermGroupCount(0)) }
    }

    @Test
    fun `WHEN inactive tabs are updated THEN report the count of inactive tabs`() {
        store.dispatch(TabsTrayAction.UpdateInactiveTabs(emptyList()))
        store.waitUntilIdle()
        verify { metrics.track(Event.TabsTrayHasInactiveTabs(0)) }
        verify { metrics.track(Event.InactiveTabsCountUpdate(0)) }
    }

    @Test
    fun testGenerateTabGroupSizeMappedValue() {
        assertEquals(1L, tabsTrayMiddleware.generateTabGroupSizeMappedValue(2))
        assertEquals(2L, tabsTrayMiddleware.generateTabGroupSizeMappedValue(3))
        assertEquals(2L, tabsTrayMiddleware.generateTabGroupSizeMappedValue(4))
        assertEquals(2L, tabsTrayMiddleware.generateTabGroupSizeMappedValue(5))
        assertEquals(3L, tabsTrayMiddleware.generateTabGroupSizeMappedValue(6))
        assertEquals(3L, tabsTrayMiddleware.generateTabGroupSizeMappedValue(7))
        assertEquals(3L, tabsTrayMiddleware.generateTabGroupSizeMappedValue(8))
        assertEquals(3L, tabsTrayMiddleware.generateTabGroupSizeMappedValue(9))
        assertEquals(3L, tabsTrayMiddleware.generateTabGroupSizeMappedValue(10))
        assertEquals(4L, tabsTrayMiddleware.generateTabGroupSizeMappedValue(11))
        assertEquals(4L, tabsTrayMiddleware.generateTabGroupSizeMappedValue(12))
        assertEquals(4L, tabsTrayMiddleware.generateTabGroupSizeMappedValue(20))
        assertEquals(4L, tabsTrayMiddleware.generateTabGroupSizeMappedValue(50))
    }

    private fun generateSearchTermTabGroupsForAverage(): List<TabGroup> {
        val group1 = TabGroup("", mockk(relaxed = true), 0L)
        val group2 = TabGroup("", mockk(relaxed = true), 0L)
        val group3 = TabGroup("", mockk(relaxed = true), 0L)

        every { group1.tabs.size } returns 8
        every { group2.tabs.size } returns 4
        every { group3.tabs.size } returns 3

        return listOf(group1, group2, group3)
    }

    private fun generateSearchTermTabGroupsForDistribution(): List<TabGroup> {
        val group1 = TabGroup("", mockk(relaxed = true), 0L)
        val group2 = TabGroup("", mockk(relaxed = true), 0L)
        val group3 = TabGroup("", mockk(relaxed = true), 0L)
        val group4 = TabGroup("", mockk(relaxed = true), 0L)

        every { group1.tabs.size } returns 8
        every { group2.tabs.size } returns 4
        every { group3.tabs.size } returns 2
        every { group4.tabs.size } returns 12

        return listOf(group1, group2, group3, group4)
    }
}
