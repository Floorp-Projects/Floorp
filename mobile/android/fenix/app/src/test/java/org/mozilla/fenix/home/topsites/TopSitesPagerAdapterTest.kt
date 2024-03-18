/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

package org.mozilla.fenix.home.topsites

import io.mockk.Runs
import io.mockk.every
import io.mockk.just
import io.mockk.mockk
import io.mockk.spyk
import io.mockk.verify
import mozilla.components.feature.top.sites.TopSite
import org.junit.Assert.assertEquals
import org.junit.Before
import org.junit.Test
import org.mozilla.fenix.home.sessioncontrol.AdapterItem.TopSitePagerPayload

class TopSitesPagerAdapterTest {

    private lateinit var topSitesPagerAdapter: TopSitesPagerAdapter

    private val topSite = TopSite.Default(
        id = 1L,
        title = "Title1",
        url = "https://mozilla.org",
        null,
    )

    private val topSite2 = TopSite.Default(
        id = 2L,
        title = "Title2",
        url = "https://mozilla.org",
        null,
    )

    private val topSite3 = TopSite.Default(
        id = 3L,
        title = "Title3",
        url = "https://firefox.org",
        null,
    )
    private val topSite4 = TopSite.Default(
        id = 4L,
        title = "Title4",
        url = "https://firefox.org",
        null,
    )

    @Before
    fun setup() {
        topSitesPagerAdapter = spyk(TopSitesPagerAdapter(mockk(), mockk(), mockk()))
    }

    @Test
    fun testDiffCallback() {
        assertEquals(
            TopSitesPagerAdapter.TopSiteListDiffCallback.getChangePayload(
                listOf(topSite, topSite3),
                listOf(topSite, topSite2),
            ),
            TopSitePagerPayload(setOf(Pair(1, topSite2))),
        )
    }

    @Test
    fun `GIVEN a payload with topSites for both pages WHEN getCurrentPageChanges THEN return topSites only for current page`() {
        val payload = TopSitePagerPayload(
            setOf(
                Pair(0, topSite),
                Pair(1, topSite2),
                Pair(2, topSite3),
                Pair(8, topSite4),
            ),
        )

        val resultPage1: List<Pair<Int, TopSite>> =
            topSitesPagerAdapter.getCurrentPageChanges(payload, 0)
        val resultPage2: List<Pair<Int, TopSite>> =
            topSitesPagerAdapter.getCurrentPageChanges(payload, 1)

        assertEquals(
            listOf(
                Pair(0, topSite),
                Pair(1, topSite2),
                Pair(2, topSite3),
            ),
            resultPage1,
        )

        assertEquals(
            listOf(Pair(8, topSite4)),
            resultPage2,
        )
    }

    @Test
    fun `WHEN update is called to delete the 1st of 4 topSites THEN submitList will update 3 topSites`() {
        val currentList = listOf(topSite, topSite2, topSite3, topSite4)
        val topSitesAdapter: TopSitesAdapter = mockk()

        every { topSitesAdapter.currentList } returns currentList
        every { topSitesAdapter.submitList(any()) } just Runs

        val removedTopSite = TopSite.Default(
            id = -1L,
            title = "REMOVED",
            url = "https://firefox.org",
            null,
        )
        val payload = TopSitePagerPayload(
            setOf(
                Pair(0, removedTopSite),
                Pair(1, topSite2),
                Pair(2, topSite3),
                Pair(3, topSite4),
            ),
        )

        topSitesPagerAdapter.update(payload, 0, topSitesAdapter)

        val expected = listOf(topSite2, topSite3, topSite4)
        verify { topSitesAdapter.submitList(expected) }
    }

    @Test
    fun `WHEN update is called to delete the 4th of 4 topSites THEN submitList will update 1 topSite`() {
        val currentList = listOf(topSite, topSite2, topSite3, topSite4)
        val topSitesAdapter: TopSitesAdapter = mockk()

        every { topSitesAdapter.currentList } returns currentList
        every { topSitesAdapter.submitList(any()) } just Runs

        val removedTopSite = TopSite.Default(
            id = -1L,
            title = "REMOVED",
            url = "https://firefox.org",
            null,
        )
        val payload = TopSitePagerPayload(
            setOf(
                Pair(3, removedTopSite),
            ),
        )

        topSitesPagerAdapter.update(payload, 0, topSitesAdapter)

        val expected = listOf(topSite, topSite2, topSite3)
        verify { topSitesAdapter.submitList(expected) }
    }

    @Test
    fun `WHEN update is called to update the 3rd of 4 topSites THEN submitList will contain 4 items`() {
        val currentList = listOf(topSite, topSite2, topSite3, topSite4)
        val topSitesAdapter: TopSitesAdapter = mockk()

        every { topSitesAdapter.currentList } returns currentList
        every { topSitesAdapter.submitList(any()) } just Runs

        val changedTopSite = TopSite.Default(
            id = 3L,
            title = "CHANGED",
            url = "https://firefox.org",
            null,
        )
        val payload = TopSitePagerPayload(
            setOf(
                Pair(2, changedTopSite),
            ),
        )

        topSitesPagerAdapter.update(payload, 0, topSitesAdapter)

        val expected = listOf(topSite, topSite2, changedTopSite, topSite4)
        verify { topSitesAdapter.submitList(expected) }
    }
}
