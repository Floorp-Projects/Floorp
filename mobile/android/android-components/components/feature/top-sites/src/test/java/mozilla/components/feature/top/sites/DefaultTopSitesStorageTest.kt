/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

package mozilla.components.feature.top.sites

import androidx.test.ext.junit.runners.AndroidJUnit4
import kotlinx.coroutines.ExperimentalCoroutinesApi
import kotlinx.coroutines.test.runBlockingTest
import mozilla.components.browser.storage.sync.PlacesHistoryStorage
import mozilla.components.concept.storage.FrecencyThresholdOption
import mozilla.components.concept.storage.TopFrecentSiteInfo
import mozilla.components.feature.top.sites.ext.toTopSite
import mozilla.components.support.test.any
import mozilla.components.support.test.mock
import mozilla.components.support.test.whenever
import org.junit.Assert.assertEquals
import org.junit.Assert.assertTrue
import org.junit.Test
import org.junit.runner.RunWith
import org.mockito.Mockito.anyInt
import org.mockito.Mockito.never
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
            pinnedSitesStorage = pinnedSitesStorage,
            historyStorage = historyStorage,
            defaultTopSites = defaultTopSites,
            coroutineContext = coroutineContext
        )

        verify(pinnedSitesStorage).addAllPinnedSites(defaultTopSites, isDefault = true)
    }

    @Test
    fun `addPinnedSite`() = runBlockingTest {
        val defaultTopSitesStorage = DefaultTopSitesStorage(
            pinnedSitesStorage = pinnedSitesStorage,
            historyStorage = historyStorage,
            defaultTopSites = listOf(),
            coroutineContext = coroutineContext
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
            pinnedSitesStorage = pinnedSitesStorage,
            historyStorage = historyStorage,
            defaultTopSites = listOf(),
            coroutineContext = coroutineContext
        )

        val frecentSite = TopSite.Frecent(
            id = 1,
            title = "Mozilla",
            url = "https://mozilla.com",
            createdAt = 1
        )
        defaultTopSitesStorage.removeTopSite(frecentSite)

        verify(historyStorage).deleteVisitsFor(frecentSite.url)

        val pinnedSite = TopSite.Pinned(
            id = 2,
            title = "Firefox",
            url = "https://firefox.com",
            createdAt = 2
        )
        defaultTopSitesStorage.removeTopSite(pinnedSite)

        verify(pinnedSitesStorage).removePinnedSite(pinnedSite)
        verify(historyStorage).deleteVisitsFor(pinnedSite.url)

        val defaultSite = TopSite.Default(
            id = 3,
            title = "Wikipedia",
            url = "https://wikipedia.com",
            createdAt = 3
        )
        defaultTopSitesStorage.removeTopSite(defaultSite)

        verify(pinnedSitesStorage).removePinnedSite(defaultSite)
        verify(historyStorage).deleteVisitsFor(defaultSite.url)
    }

    @Test
    fun `updateTopSite`() = runBlockingTest {
        val defaultTopSitesStorage = DefaultTopSitesStorage(
            pinnedSitesStorage = pinnedSitesStorage,
            historyStorage = historyStorage,
            defaultTopSites = listOf(),
            coroutineContext = coroutineContext
        )

        val defaultSite = TopSite.Default(
            id = 1,
            title = "Firefox",
            url = "https://firefox.com",
            createdAt = 1
        )
        defaultTopSitesStorage.updateTopSite(defaultSite, "Mozilla Firefox", "https://mozilla.com")

        verify(pinnedSitesStorage).updatePinnedSite(defaultSite, "Mozilla Firefox", "https://mozilla.com")

        val pinnedSite = TopSite.Pinned(
            id = 2,
            title = "Wikipedia",
            url = "https://wikipedia.com",
            createdAt = 2
        )
        defaultTopSitesStorage.updateTopSite(pinnedSite, "Wiki", "https://en.wikipedia.org/wiki/Wiki")

        verify(pinnedSitesStorage).updatePinnedSite(pinnedSite, "Wiki", "https://en.wikipedia.org/wiki/Wiki")

        val frecentSite = TopSite.Frecent(
            id = 1,
            title = "Mozilla",
            url = "https://mozilla.com",
            createdAt = 1
        )
        defaultTopSitesStorage.updateTopSite(frecentSite, "Moz", "")

        verify(pinnedSitesStorage, never()).updatePinnedSite(frecentSite, "Moz", "")
    }

    @Test
    fun `getTopSites returns only default and pinned sites when frecencyConfig is null`() = runBlockingTest {
        val defaultTopSitesStorage = DefaultTopSitesStorage(
            pinnedSitesStorage = pinnedSitesStorage,
            historyStorage = historyStorage,
            defaultTopSites = listOf(),
            coroutineContext = coroutineContext
        )

        val defaultSite = TopSite.Default(
            id = 1,
            title = "Firefox",
            url = "https://firefox.com",
            createdAt = 1
        )
        val pinnedSite = TopSite.Pinned(
            id = 2,
            title = "Wikipedia",
            url = "https://wikipedia.com",
            createdAt = 2
        )
        whenever(pinnedSitesStorage.getPinnedSites()).thenReturn(
            listOf(
                defaultSite,
                pinnedSite
            )
        )
        whenever(pinnedSitesStorage.getPinnedSitesCount()).thenReturn(2)

        var topSites = defaultTopSitesStorage.getTopSites(0, frecencyConfig = null)
        assertTrue(topSites.isEmpty())
        assertEquals(defaultTopSitesStorage.cachedTopSites, topSites)

        topSites = defaultTopSitesStorage.getTopSites(1, frecencyConfig = null)
        assertEquals(1, topSites.size)
        assertEquals(defaultSite, topSites[0])
        assertEquals(defaultTopSitesStorage.cachedTopSites, topSites)

        topSites = defaultTopSitesStorage.getTopSites(2, frecencyConfig = null)
        assertEquals(2, topSites.size)
        assertEquals(defaultSite, topSites[0])
        assertEquals(pinnedSite, topSites[1])
        assertEquals(defaultTopSitesStorage.cachedTopSites, topSites)

        topSites = defaultTopSitesStorage.getTopSites(5, frecencyConfig = null)
        assertEquals(2, topSites.size)
        assertEquals(defaultSite, topSites[0])
        assertEquals(pinnedSite, topSites[1])
        assertEquals(defaultTopSitesStorage.cachedTopSites, topSites)
    }

    @Test
    fun `getTopSites returns pinned and frecent sites when frecencyConfig is specified`() = runBlockingTest {
        val defaultTopSitesStorage = DefaultTopSitesStorage(
            pinnedSitesStorage = pinnedSitesStorage,
            historyStorage = historyStorage,
            defaultTopSites = listOf(),
            coroutineContext = coroutineContext
        )

        val defaultSite = TopSite.Default(
            id = 1,
            title = "Firefox",
            url = "https://firefox.com",
            createdAt = 1
        )
        val pinnedSite = TopSite.Pinned(
            id = 2,
            title = "Wikipedia",
            url = "https://wikipedia.com",
            createdAt = 2
        )
        whenever(pinnedSitesStorage.getPinnedSites()).thenReturn(
            listOf(
                defaultSite,
                pinnedSite
            )
        )
        whenever(pinnedSitesStorage.getPinnedSitesCount()).thenReturn(2)

        val frecentSite1 = TopFrecentSiteInfo("https://mozilla.com", "Mozilla")
        whenever(historyStorage.getTopFrecentSites(anyInt(), any())).thenReturn(listOf(frecentSite1))

        var topSites = defaultTopSitesStorage.getTopSites(0, frecencyConfig = FrecencyThresholdOption.NONE)
        assertTrue(topSites.isEmpty())

        topSites = defaultTopSitesStorage.getTopSites(1, frecencyConfig = FrecencyThresholdOption.NONE)
        assertEquals(1, topSites.size)
        assertEquals(defaultSite, topSites[0])
        assertEquals(defaultTopSitesStorage.cachedTopSites, topSites)

        topSites = defaultTopSitesStorage.getTopSites(2, frecencyConfig = FrecencyThresholdOption.NONE)
        assertEquals(2, topSites.size)
        assertEquals(defaultSite, topSites[0])
        assertEquals(pinnedSite, topSites[1])
        assertEquals(defaultTopSitesStorage.cachedTopSites, topSites)

        topSites = defaultTopSitesStorage.getTopSites(5, frecencyConfig = FrecencyThresholdOption.NONE)
        assertEquals(3, topSites.size)
        assertEquals(defaultSite, topSites[0])
        assertEquals(pinnedSite, topSites[1])
        assertEquals(frecentSite1.toTopSite(), topSites[2])
        assertEquals(defaultTopSitesStorage.cachedTopSites, topSites)

        val frecentSite2 = TopFrecentSiteInfo("https://example.com", "Example")
        val frecentSite3 = TopFrecentSiteInfo("https://getpocket.com", "Pocket")
        whenever(historyStorage.getTopFrecentSites(anyInt(), any())).thenReturn(
            listOf(
                frecentSite1,
                frecentSite2,
                frecentSite3
            )
        )

        topSites = defaultTopSitesStorage.getTopSites(5, frecencyConfig = FrecencyThresholdOption.NONE)
        assertEquals(5, topSites.size)
        assertEquals(defaultSite, topSites[0])
        assertEquals(pinnedSite, topSites[1])
        assertEquals(frecentSite1.toTopSite(), topSites[2])
        assertEquals(frecentSite2.toTopSite(), topSites[3])
        assertEquals(frecentSite3.toTopSite(), topSites[4])
        assertEquals(defaultTopSitesStorage.cachedTopSites, topSites)

        val frecentSite4 = TopFrecentSiteInfo("https://example2.com", "Example2")
        whenever(historyStorage.getTopFrecentSites(anyInt(), any())).thenReturn(
            listOf(
                frecentSite1,
                frecentSite2,
                frecentSite3,
                frecentSite4
            )
        )

        topSites = defaultTopSitesStorage.getTopSites(5, frecencyConfig = FrecencyThresholdOption.NONE)
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
            pinnedSitesStorage = pinnedSitesStorage,
            historyStorage = historyStorage,
            defaultTopSites = listOf(),
            coroutineContext = coroutineContext
        )

        val defaultSiteFirefox = TopSite.Default(
            id = 1,
            title = "Firefox",
            url = "https://firefox.com",
            createdAt = 1
        )
        val pinnedSite1 = TopSite.Pinned(
            id = 2,
            title = "Wikipedia",
            url = "https://wikipedia.com",
            createdAt = 2
        )
        val pinnedSite2 = TopSite.Pinned(
            id = 3,
            title = "Example",
            url = "https://example.com",
            createdAt = 3
        )
        whenever(pinnedSitesStorage.getPinnedSites()).thenReturn(
            listOf(
                defaultSiteFirefox,
                pinnedSite1,
                pinnedSite2
            )
        )
        whenever(pinnedSitesStorage.getPinnedSitesCount()).thenReturn(3)

        val frecentSiteWithNoTitle = TopFrecentSiteInfo("https://mozilla.com", "")
        val frecentSiteFirefox = TopFrecentSiteInfo("https://firefox.com", "Firefox")
        val frecentSite1 = TopFrecentSiteInfo("https://getpocket.com", "Pocket")
        val frecentSite2 = TopFrecentSiteInfo("https://www.example.com", "Example")
        whenever(historyStorage.getTopFrecentSites(anyInt(), any())).thenReturn(
            listOf(
                frecentSiteWithNoTitle,
                frecentSiteFirefox,
                frecentSite1,
                frecentSite2
            )
        )

        val topSites = defaultTopSitesStorage.getTopSites(5, frecencyConfig = FrecencyThresholdOption.NONE)

        verify(historyStorage).getTopFrecentSites(5, frecencyThreshold = FrecencyThresholdOption.NONE)

        assertEquals(5, topSites.size)
        assertEquals(defaultSiteFirefox, topSites[0])
        assertEquals(pinnedSite1, topSites[1])
        assertEquals(pinnedSite2, topSites[2])
        assertEquals(frecentSiteWithNoTitle.toTopSite(), topSites[3])
        assertEquals(frecentSite1.toTopSite(), topSites[4])
        assertEquals("mozilla.com", frecentSiteWithNoTitle.toTopSite().title)
        assertEquals(defaultTopSitesStorage.cachedTopSites, topSites)
    }
}
