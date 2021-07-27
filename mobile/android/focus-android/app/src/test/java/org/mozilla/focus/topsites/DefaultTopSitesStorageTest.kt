/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

package org.mozilla.focus.topsites

import kotlinx.coroutines.ExperimentalCoroutinesApi
import kotlinx.coroutines.test.runBlockingTest
import mozilla.components.feature.top.sites.PinnedSiteStorage
import mozilla.components.feature.top.sites.TopSite
import mozilla.components.support.test.mock
import mozilla.components.support.test.whenever
import org.junit.Assert.assertEquals
import org.junit.Assert.assertTrue
import org.junit.Test
import org.junit.runner.RunWith
import org.mockito.Mockito.verify
import org.robolectric.RobolectricTestRunner

@ExperimentalCoroutinesApi
@RunWith(RobolectricTestRunner::class)
class DefaultTopSitesStorageTest {

    private val pinnedSitesStorage: PinnedSiteStorage = mock()

    @Test
    fun `WHEN a top site is added THEN the pinned sites storage is called`() = runBlockingTest {
        val defaultTopSitesStorage = DefaultTopSitesStorage(
            pinnedSitesStorage,
            coroutineContext
        )

        defaultTopSitesStorage.addTopSite("Mozilla", "https://mozilla.com", isDefault = false)

        verify(pinnedSitesStorage).addPinnedSite(
            "Mozilla",
            "https://mozilla.com",
            isDefault = false
        )
    }

    @Test
    fun `WHEN a top site is removed THEN the pinned sites storage is called`() = runBlockingTest {
        val defaultTopSitesStorage = DefaultTopSitesStorage(
            pinnedSitesStorage,
            coroutineContext
        )

        val pinnedSite = TopSite(
            id = 2,
            title = "Firefox",
            url = "https://firefox.com",
            createdAt = 2,
            type = TopSite.Type.PINNED
        )

        defaultTopSitesStorage.removeTopSite(pinnedSite)

        verify(pinnedSitesStorage).removePinnedSite(pinnedSite)
    }

    @Test
    fun `WHEN a top site is updated THEN the pinned sites storage is called`() = runBlockingTest {
        val defaultTopSitesStorage = DefaultTopSitesStorage(
            pinnedSitesStorage,
            coroutineContext
        )

        val pinnedSite = TopSite(
            id = 2,
            title = "Wikipedia",
            url = "https://wikipedia.com",
            createdAt = 2,
            type = TopSite.Type.PINNED
        )
        defaultTopSitesStorage.updateTopSite(
            pinnedSite,
            "Wiki",
            "https://en.wikipedia.org/wiki/Wiki"
        )

        verify(pinnedSitesStorage).updatePinnedSite(
            pinnedSite,
            "Wiki",
            "https://en.wikipedia.org/wiki/Wiki"
        )
    }

    @Test
    fun `WHEN getTopSites is called THEN the appropriate top sites are returned`() = runBlockingTest {
        val defaultTopSitesStorage = DefaultTopSitesStorage(
            pinnedSitesStorage,
            coroutineContext
        )

        val pinnedSite1 = TopSite(
            id = 2,
            title = "Wikipedia",
            url = "https://wikipedia.com",
            createdAt = 2,
            type = TopSite.Type.PINNED
        )
        val pinnedSite2 = TopSite(
            id = 3,
            title = "Example",
            url = "https://example.com",
            createdAt = 3,
            type = TopSite.Type.PINNED
        )

        whenever(pinnedSitesStorage.getPinnedSites()).thenReturn(
            listOf(
                pinnedSite1,
                pinnedSite2
            )
        )

        var topSites = defaultTopSitesStorage.getTopSites(0, frecencyConfig = null)
        assertTrue(topSites.isEmpty())

        topSites = defaultTopSitesStorage.getTopSites(1, frecencyConfig = null)
        assertEquals(1, topSites.size)
        assertEquals(pinnedSite1, topSites[0])

        topSites = defaultTopSitesStorage.getTopSites(2, frecencyConfig = null)
        assertEquals(2, topSites.size)
        assertEquals(pinnedSite1, topSites[0])
        assertEquals(pinnedSite2, topSites[1])

        topSites = defaultTopSitesStorage.getTopSites(5, frecencyConfig = null)
        assertEquals(2, topSites.size)
        assertEquals(pinnedSite1, topSites[0])
        assertEquals(pinnedSite2, topSites[1])
    }
}
