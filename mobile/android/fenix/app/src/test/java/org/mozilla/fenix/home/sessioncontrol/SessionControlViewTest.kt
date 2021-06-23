/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

package org.mozilla.fenix.home.sessioncontrol

import io.mockk.mockk
import mozilla.components.browser.state.state.TabSessionState
import org.junit.Assert.assertEquals
import org.junit.Assert.assertSame
import org.junit.Assert.assertTrue
import org.junit.Test
import org.mozilla.fenix.home.recenttabs.view.RecentTabsItemPosition

class SessionControlViewTest {
    @Test
    fun `GIVEN two recent tabs WHEN showRecentTabs is called THEN add the header, and two recent items to be shown`() {
        val recentTab: TabSessionState = mockk()
        val mediaTab: TabSessionState = mockk()
        val items = mutableListOf<AdapterItem>()

        showRecentTabs(listOf(recentTab, mediaTab), items)

        assertEquals(3, items.size)
        assertTrue(items[0] is AdapterItem.RecentTabsHeader)
        assertEquals(recentTab, (items[1] as AdapterItem.RecentTabItem).tab)
        assertEquals(mediaTab, (items[2] as AdapterItem.RecentTabItem).tab)
    }

    @Test
    fun `GIVEN one recent tab WHEN showRecentTabs is called THEN add the header and the recent tab to items shown`() {
        val recentTab: TabSessionState = mockk()
        val items = mutableListOf<AdapterItem>()

        showRecentTabs(listOf(recentTab), items)

        assertEquals(2, items.size)
        assertTrue(items[0] is AdapterItem.RecentTabsHeader)
        assertEquals(recentTab, (items[1] as AdapterItem.RecentTabItem).tab)
    }

    @Test
    fun `GIVEN only one recent tab and no media tab WHEN showRecentTabs is called THEN add the recent item as a single one to be shown`() {
        val recentTab: TabSessionState = mockk()
        val items = mutableListOf<AdapterItem>()

        showRecentTabs(listOf(recentTab), items)

        assertEquals(recentTab, (items[1] as AdapterItem.RecentTabItem).tab)
        assertSame(RecentTabsItemPosition.SINGLE, (items[1] as AdapterItem.RecentTabItem).position)
    }

    @Test
    fun `GIVEN two recent tabs WHEN showRecentTabs is called THEN add one item as top and one as bottom to be shown`() {
        val recentTab: TabSessionState = mockk()
        val mediaTab: TabSessionState = mockk()
        val items = mutableListOf<AdapterItem>()

        showRecentTabs(listOf(recentTab, mediaTab), items)

        assertEquals(recentTab, (items[1] as AdapterItem.RecentTabItem).tab)
        assertSame(RecentTabsItemPosition.TOP, (items[1] as AdapterItem.RecentTabItem).position)
        assertEquals(mediaTab, (items[2] as AdapterItem.RecentTabItem).tab)
        assertSame(RecentTabsItemPosition.BOTTOM, (items[2] as AdapterItem.RecentTabItem).position)
    }

    @Test
    fun `GIVEN three recent tabs WHEN showRecentTabs is called THEN add one recent item as top, one as middle and one as bottom to be shown`() {
        val recentTab1: TabSessionState = mockk()
        val recentTab2: TabSessionState = mockk()
        val mediaTab: TabSessionState = mockk()
        val items = mutableListOf<AdapterItem>()

        showRecentTabs(listOf(recentTab1, recentTab2, mediaTab), items)

        assertEquals(recentTab1, (items[1] as AdapterItem.RecentTabItem).tab)
        assertSame(RecentTabsItemPosition.TOP, (items[1] as AdapterItem.RecentTabItem).position)
        assertEquals(recentTab2, (items[2] as AdapterItem.RecentTabItem).tab)
        assertSame(RecentTabsItemPosition.MIDDLE, (items[2] as AdapterItem.RecentTabItem).position)
        assertEquals(mediaTab, (items[3] as AdapterItem.RecentTabItem).tab)
        assertSame(RecentTabsItemPosition.BOTTOM, (items[3] as AdapterItem.RecentTabItem).position)
    }
}
