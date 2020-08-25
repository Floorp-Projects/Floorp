/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

package mozilla.components.feature.top.sites

import androidx.test.ext.junit.runners.AndroidJUnit4
import kotlinx.coroutines.ExperimentalCoroutinesApi
import kotlinx.coroutines.test.runBlockingTest
import mozilla.components.browser.storage.sync.PlacesHistoryStorage
import mozilla.components.concept.storage.TopFrecentSiteInfo
import mozilla.components.feature.top.sites.ext.toTopSite
import mozilla.components.support.test.mock
import mozilla.components.support.test.whenever
import org.junit.Assert.assertEquals
import org.junit.Assert.assertTrue
import org.junit.Test
import org.junit.runner.RunWith
import org.mockito.Mockito.anyInt
import org.mockito.Mockito.verify

@ExperimentalCoroutinesApi
@RunWith(AndroidJUnit4::class)
class DefaultTopSitesStorageTest {

    private val pinnedSitesStorage: PinnedSiteStorage = mock()
    private val historyStorage: PlacesHistoryStorage = mock()

    @Test
    fun `default top sites are added to pinned site storage on init`() = runBlockingTest {
        val defaultTopSites = listOf(
            Pair("Mozilla", "https://mozilla.com"),
            Pair("Firefox", "https://firefox.com")
        )

        DefaultTopSitesStorage(
            pinnedSitesStorage,
            historyStorage,
            defaultTopSites,
            coroutineContext
        )

        verify(pinnedSitesStorage).addPinnedSite(
            "Mozilla",
            "https://mozilla.com",
            isDefault = true
        )

        verify(pinnedSitesStorage).addPinnedSite(
            "Firefox",
            "https://firefox.com",
            isDefault = true
        )
    }

    @Test
    fun `addPinnedSite`() = runBlockingTest {
        val defaultTopSitesStorage = DefaultTopSitesStorage(
            pinnedSitesStorage,
            historyStorage,
            listOf(),
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
    fun `removeTopSite`() = runBlockingTest {
        val defaultTopSitesStorage = DefaultTopSitesStorage(
            pinnedSitesStorage,
            historyStorage,
            listOf(),
            coroutineContext
        )

        val frecentSite = TopSite(
            id = 1,
            title = "Mozilla",
            url = "https://mozilla.com",
            createdAt = 1,
            type = TopSite.Type.FRECENT
        )
        defaultTopSitesStorage.removeTopSite(frecentSite)

        verify(historyStorage).deleteVisitsFor(frecentSite.url)

        val pinnedSite = TopSite(
            id = 2,
            title = "Firefox",
            url = "https://firefox.com",
            createdAt = 2,
            type = TopSite.Type.PINNED
        )
        defaultTopSitesStorage.removeTopSite(pinnedSite)

        verify(pinnedSitesStorage).removePinnedSite(pinnedSite)

        val defaultSite = TopSite(
            id = 3,
            title = "Wikipedia",
            url = "https://wikipedia.com",
            createdAt = 3,
            type = TopSite.Type.DEFAULT
        )
        defaultTopSitesStorage.removeTopSite(defaultSite)

        verify(pinnedSitesStorage).removePinnedSite(defaultSite)
    }

    @Test
    fun `getTopSites returns only default and pinned sites when includeFrecent is false`() = runBlockingTest {
        val defaultTopSitesStorage = DefaultTopSitesStorage(
            pinnedSitesStorage,
            historyStorage,
            listOf(),
            coroutineContext
        )

        val defaultSite = TopSite(
            id = 1,
            title = "Firefox",
            url = "https://firefox.com",
            createdAt = 1,
            type = TopSite.Type.DEFAULT
        )
        val pinnedSite = TopSite(
            id = 2,
            title = "Wikipedia",
            url = "https://wikipedia.com",
            createdAt = 2,
            type = TopSite.Type.PINNED
        )
        whenever(pinnedSitesStorage.getPinnedSites()).thenReturn(
            listOf(
                defaultSite,
                pinnedSite
            )
        )

        var topSites = defaultTopSitesStorage.getTopSites(0, false)
        assertTrue(topSites.isEmpty())
        assertEquals(defaultTopSitesStorage.cachedTopSites, topSites)

        topSites = defaultTopSitesStorage.getTopSites(1, false)
        assertEquals(1, topSites.size)
        assertEquals(defaultSite, topSites[0])
        assertEquals(defaultTopSitesStorage.cachedTopSites, topSites)

        topSites = defaultTopSitesStorage.getTopSites(2, false)
        assertEquals(2, topSites.size)
        assertEquals(defaultSite, topSites[0])
        assertEquals(pinnedSite, topSites[1])
        assertEquals(defaultTopSitesStorage.cachedTopSites, topSites)

        topSites = defaultTopSitesStorage.getTopSites(5, false)
        assertEquals(2, topSites.size)
        assertEquals(defaultSite, topSites[0])
        assertEquals(pinnedSite, topSites[1])
        assertEquals(defaultTopSitesStorage.cachedTopSites, topSites)
    }

    @Test
    fun `getTopSites returns pinned and frecent sites when includeFrecent is true`() = runBlockingTest {
        val defaultTopSitesStorage = DefaultTopSitesStorage(
            pinnedSitesStorage,
            historyStorage,
            listOf(),
            coroutineContext
        )

        val defaultSite = TopSite(
            id = 1,
            title = "Firefox",
            url = "https://firefox.com",
            createdAt = 1,
            type = TopSite.Type.DEFAULT
        )
        val pinnedSite = TopSite(
            id = 2,
            title = "Wikipedia",
            url = "https://wikipedia.com",
            createdAt = 2,
            type = TopSite.Type.PINNED
        )
        whenever(pinnedSitesStorage.getPinnedSites()).thenReturn(
            listOf(
                defaultSite,
                pinnedSite
            )
        )

        val frecentSite1 = TopFrecentSiteInfo("https://mozilla.com", "Mozilla")
        whenever(historyStorage.getTopFrecentSites(anyInt())).thenReturn(listOf(frecentSite1))

        var topSites = defaultTopSitesStorage.getTopSites(0, true)
        assertTrue(topSites.isEmpty())

        topSites = defaultTopSitesStorage.getTopSites(1, true)
        assertEquals(1, topSites.size)
        assertEquals(defaultSite, topSites[0])
        assertEquals(defaultTopSitesStorage.cachedTopSites, topSites)

        topSites = defaultTopSitesStorage.getTopSites(2, true)
        assertEquals(2, topSites.size)
        assertEquals(defaultSite, topSites[0])
        assertEquals(pinnedSite, topSites[1])
        assertEquals(defaultTopSitesStorage.cachedTopSites, topSites)

        topSites = defaultTopSitesStorage.getTopSites(5, true)
        assertEquals(3, topSites.size)
        assertEquals(defaultSite, topSites[0])
        assertEquals(pinnedSite, topSites[1])
        assertEquals(frecentSite1.toTopSite(), topSites[2])
        assertEquals(defaultTopSitesStorage.cachedTopSites, topSites)

        val frecentSite2 = TopFrecentSiteInfo("https://example.com", "Example")
        val frecentSite3 = TopFrecentSiteInfo("https://getpocket.com", "Pocket")
        whenever(historyStorage.getTopFrecentSites(anyInt())).thenReturn(
            listOf(
                frecentSite1,
                frecentSite2,
                frecentSite3
            )
        )

        topSites = defaultTopSitesStorage.getTopSites(5, true)
        assertEquals(5, topSites.size)
        assertEquals(defaultSite, topSites[0])
        assertEquals(pinnedSite, topSites[1])
        assertEquals(frecentSite1.toTopSite(), topSites[2])
        assertEquals(frecentSite2.toTopSite(), topSites[3])
        assertEquals(frecentSite3.toTopSite(), topSites[4])
        assertEquals(defaultTopSitesStorage.cachedTopSites, topSites)

        val frecentSite4 = TopFrecentSiteInfo("https://example2.com", "Example2")
        whenever(historyStorage.getTopFrecentSites(anyInt())).thenReturn(
            listOf(
                frecentSite1,
                frecentSite2,
                frecentSite3,
                frecentSite4
            )
        )

        topSites = defaultTopSitesStorage.getTopSites(5, true)
        assertEquals(5, topSites.size)
        assertEquals(defaultSite, topSites[0])
        assertEquals(pinnedSite, topSites[1])
        assertEquals(frecentSite1.toTopSite(), topSites[2])
        assertEquals(frecentSite2.toTopSite(), topSites[3])
        assertEquals(frecentSite3.toTopSite(), topSites[4])
        assertEquals(defaultTopSitesStorage.cachedTopSites, topSites)
    }

    @Test
    fun `getTopSites filters out frecent sites that already exist in pinned sites`() = runBlockingTest {
        val defaultTopSitesStorage = DefaultTopSitesStorage(
            pinnedSitesStorage,
            historyStorage,
            listOf(),
            coroutineContext
        )

        val defaultSiteFirefox = TopSite(
            id = 1,
            title = "Firefox",
            url = "https://firefox.com",
            createdAt = 1,
            type = TopSite.Type.DEFAULT
        )
        val pinnedSite = TopSite(
            id = 2,
            title = "Wikipedia",
            url = "https://wikipedia.com",
            createdAt = 2,
            type = TopSite.Type.PINNED
        )
        whenever(pinnedSitesStorage.getPinnedSites()).thenReturn(
            listOf(
                defaultSiteFirefox,
                pinnedSite
            )
        )

        val frecentSiteWithNoTitle = TopFrecentSiteInfo("https://mozilla.com", "")
        val frecentSiteFirefox = TopFrecentSiteInfo("https://firefox.com", "Firefox")
        val frecentSite = TopFrecentSiteInfo("https://getpocket.com", "Pocket")
        whenever(historyStorage.getTopFrecentSites(anyInt())).thenReturn(
            listOf(
                frecentSiteWithNoTitle,
                frecentSiteFirefox,
                frecentSite
            )
        )

        val topSites = defaultTopSitesStorage.getTopSites(5, true)
        assertEquals(4, topSites.size)
        assertEquals(defaultSiteFirefox, topSites[0])
        assertEquals(pinnedSite, topSites[1])
        assertEquals(frecentSiteWithNoTitle.toTopSite(), topSites[2])
        assertEquals(frecentSite.toTopSite(), topSites[3])
        assertEquals("mozilla.com", frecentSiteWithNoTitle.toTopSite().title)
        assertEquals(defaultTopSitesStorage.cachedTopSites, topSites)
    }
}
