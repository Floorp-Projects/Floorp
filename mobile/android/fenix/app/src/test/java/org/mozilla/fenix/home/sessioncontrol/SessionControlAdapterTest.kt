/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

package org.mozilla.fenix.home.sessioncontrol

import io.mockk.every
import io.mockk.mockk
import mozilla.components.feature.top.sites.TopSite
import org.junit.Assert.assertEquals
import org.junit.Assert.assertNull
import org.junit.Test
import org.mozilla.fenix.home.sessioncontrol.AdapterItem.CollectionItem
import org.mozilla.fenix.home.sessioncontrol.AdapterItem.TopSitePager
import org.mozilla.fenix.home.sessioncontrol.AdapterItem.TopSitePagerPayload

class SessionControlAdapterTest {

    @Test
    fun `WHEN getChangePayload called with wrong type THEN return null`() {
        val newItem: AdapterItem = CollectionItem(mockk(), mockk(relaxed = true))

        val result = TopSitePager(mockk()).getChangePayload(newItem)

        assertNull(result)
    }

    @Test
    fun `GIVEN topSitePager with 5 topSites WHEN getChangePayload with 10 items THEN return null`() {
        val newItem = TopSitePager(mockk(relaxed = true))
        val topSitePager = TopSitePager(mockk(relaxed = true))
        every { topSitePager.topSites.size } returns 5
        every { newItem.topSites.size } returns 10

        val result = topSitePager.getChangePayload(newItem)

        assertNull(result)
    }

    @Test
    fun `GIVEN topSitePager with 10 topSites WHEN getChangePayload with 5 items THEN return null`() {
        val newItem = TopSitePager(mockk(relaxed = true))
        val topSitePager = TopSitePager(mockk(relaxed = true))
        every { topSitePager.topSites.size } returns 10
        every { newItem.topSites.size } returns 5

        val result = topSitePager.getChangePayload(newItem)

        assertNull(result)
    }

    @Test
    fun `GIVEN topSitePager with 3 topSites WHEN getChangePayload with 5 items THEN return null`() {
        val newItem = TopSitePager(mockk(relaxed = true))
        val topSitePager = TopSitePager(mockk(relaxed = true))
        every { topSitePager.topSites.size } returns 3
        every { newItem.topSites.size } returns 5

        val result = topSitePager.getChangePayload(newItem)

        assertNull(result)
    }

    @Test
    fun `GIVEN two topSites WHEN getChangePayload called with one changed item THEN return TopSitePagerPayload with changes`() {
        val topSite0 = TopSite.Frecent(-1, "topSite0", "", 0)
        val topSite1 = TopSite.Frecent(-1, "topSite1", "", 0)
        val topSiteChanged = TopSite.Frecent(-1, "changed", "", 0)
        val topSitePager = TopSitePager(listOf(topSite0, topSite1))
        val newItem = TopSitePager(listOf(topSite0, topSiteChanged))

        val result = topSitePager.getChangePayload(newItem)

        assertEquals(TopSitePagerPayload(setOf(Pair(1, topSiteChanged))), result)
    }

    @Test
    fun `GIVEN two topSites WHEN getChangePayload called with one removed THEN return TopSitePagerPayload with removed item`() {
        val topSite0 = TopSite.Frecent(-1, "topSite0", "", 0)
        val topSite1 = TopSite.Frecent(-1, "topSite1", "", 0)
        val topSiteRemoved = TopSite.Frecent(-1, "REMOVED", "", 0)
        val topSitePager = TopSitePager(listOf(topSite0, topSite1))
        val newItem = TopSitePager(listOf(topSite0))

        val result = topSitePager.getChangePayload(newItem)

        assertEquals(TopSitePagerPayload(setOf(Pair(1, topSiteRemoved))), result)
    }
}
