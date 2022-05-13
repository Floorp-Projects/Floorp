/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

package org.mozilla.fenix.tabstray

import io.mockk.every
import io.mockk.mockk
import mozilla.components.browser.state.state.TabGroup
import mozilla.components.browser.state.state.TabPartition
import mozilla.components.service.glean.testing.GleanTestRule
import mozilla.components.support.test.libstate.ext.waitUntilIdle
import mozilla.components.support.test.robolectric.testContext
import org.junit.Assert.assertEquals
import org.junit.Assert.assertNotNull
import org.junit.Assert.assertNull
import org.junit.Before
import org.junit.Rule
import org.junit.Test
import org.junit.runner.RunWith
import org.mozilla.fenix.GleanMetrics.Metrics
import org.mozilla.fenix.GleanMetrics.SearchTerms
import org.mozilla.fenix.GleanMetrics.TabsTray
import org.mozilla.fenix.helpers.FenixRobolectricTestRunner

@RunWith(FenixRobolectricTestRunner::class) // for gleanTestRule
class TabsTrayMiddlewareTest {

    private lateinit var store: TabsTrayStore
    private lateinit var tabsTrayMiddleware: TabsTrayMiddleware

    @get:Rule
    val gleanTestRule = GleanTestRule(testContext)

    @Before
    fun setUp() {
        tabsTrayMiddleware = TabsTrayMiddleware()
        store = TabsTrayStore(
            middlewares = listOf(tabsTrayMiddleware),
            initialState = TabsTrayState()
        )
    }

    @Test
    fun `WHEN search term groups are updated AND there is at least one group THEN report the average tabs per group`() {
        assertNull(SearchTerms.averageTabsPerGroup.testGetValue())

        store.dispatch(TabsTrayAction.UpdateTabPartitions(generateSearchTermTabGroupsForAverage()))
        store.waitUntilIdle()

        assertNotNull(SearchTerms.averageTabsPerGroup.testGetValue())
        val event = SearchTerms.averageTabsPerGroup.testGetValue()!!
        assertEquals(1, event.size)
        assertEquals("5.0", event.single().extra!!["count"])
    }

    @Test
    fun `WHEN search term groups are updated AND there is at least one group THEN report the distribution of tab sizes`() {
        assertNull(SearchTerms.groupSizeDistribution.testGetValue())

        store.dispatch(TabsTrayAction.UpdateTabPartitions(generateSearchTermTabGroupsForDistribution()))
        store.waitUntilIdle()

        assertNotNull(SearchTerms.groupSizeDistribution.testGetValue())
        val event = SearchTerms.groupSizeDistribution.testGetValue()!!.values
        // Verify the distribution correctly describes the tab group sizes
        assertEquals(mapOf(0L to 0L, 1L to 1L, 2L to 1L, 3L to 1L, 4L to 1L), event)
    }

    @Test
    fun `WHEN search term groups are updated THEN report the count of search term tab groups`() {
        assertNull(SearchTerms.numberOfSearchTermGroup.testGetValue())

        store.dispatch(TabsTrayAction.UpdateTabPartitions(null))
        store.waitUntilIdle()

        assertNotNull(SearchTerms.numberOfSearchTermGroup.testGetValue())
        val event = SearchTerms.numberOfSearchTermGroup.testGetValue()!!
        assertEquals(1, event.size)
        assertEquals("0", event.single().extra!!["count"])
    }

    @Test
    fun `WHEN inactive tabs are updated THEN report the count of inactive tabs`() {

        assertNull(TabsTray.hasInactiveTabs.testGetValue())
        assertNull(Metrics.inactiveTabsCount.testGetValue())

        store.dispatch(TabsTrayAction.UpdateInactiveTabs(emptyList()))
        store.waitUntilIdle()
        assertNotNull(TabsTray.hasInactiveTabs.testGetValue())
        assertNotNull(Metrics.inactiveTabsCount.testGetValue())
        assertEquals(0L, Metrics.inactiveTabsCount.testGetValue())
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

    @Test
    fun `WHEN multi select mode from menu is entered THEN relevant metrics are collected`() {
        assertNull(TabsTray.enterMultiselectMode.testGetValue())

        store.dispatch(TabsTrayAction.EnterSelectMode)
        store.waitUntilIdle()

        assertNotNull(TabsTray.enterMultiselectMode.testGetValue())
        val snapshot = TabsTray.enterMultiselectMode.testGetValue()!!
        assertEquals(1, snapshot.size)
        assertEquals("false", snapshot.single().extra?.getValue("tab_selected"))
    }

    @Test
    fun `WHEN multi select mode by long press is entered THEN relevant metrics are collected`() {
        store.dispatch(TabsTrayAction.AddSelectTab(mockk()))
        store.waitUntilIdle()

        assertNotNull(TabsTray.enterMultiselectMode.testGetValue())
        val snapshot = TabsTray.enterMultiselectMode.testGetValue()!!
        assertEquals(1, snapshot.size)
        assertEquals("true", snapshot.single().extra?.getValue("tab_selected"))
    }

    private fun generateSearchTermTabGroupsForAverage(): TabPartition {
        val group1 = TabGroup("", "", mockk(relaxed = true))
        val group2 = TabGroup("", "", mockk(relaxed = true))
        val group3 = TabGroup("", "", mockk(relaxed = true))

        every { group1.tabIds.size } returns 8
        every { group2.tabIds.size } returns 4
        every { group3.tabIds.size } returns 3

        return TabPartition(SEARCH_TERM_TAB_GROUPS, listOf(group1, group2, group3))
    }

    private fun generateSearchTermTabGroupsForDistribution(): TabPartition {
        val group1 = TabGroup("", "", mockk(relaxed = true))
        val group2 = TabGroup("", "", mockk(relaxed = true))
        val group3 = TabGroup("", "", mockk(relaxed = true))
        val group4 = TabGroup("", "", mockk(relaxed = true))

        every { group1.tabIds.size } returns 8
        every { group2.tabIds.size } returns 4
        every { group3.tabIds.size } returns 2
        every { group4.tabIds.size } returns 12

        return TabPartition(SEARCH_TERM_TAB_GROUPS, listOf(group1, group2, group3, group4))
    }
}
