/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

package mozilla.components.feature.top.sites

import android.net.Uri
import androidx.test.ext.junit.runners.AndroidJUnit4
import kotlinx.coroutines.ExperimentalCoroutinesApi
import mozilla.components.browser.storage.sync.PlacesHistoryStorage
import mozilla.components.concept.storage.FrecencyThresholdOption
import mozilla.components.concept.storage.TopFrecentSiteInfo
import mozilla.components.feature.top.sites.ext.toTopSite
import mozilla.components.support.test.any
import mozilla.components.support.test.mock
import mozilla.components.support.test.rule.MainCoroutineRule
import mozilla.components.support.test.rule.runTestOnMain
import mozilla.components.support.test.whenever
import org.junit.Assert.assertEquals
import org.junit.Assert.assertTrue
import org.junit.Rule
import org.junit.Test
import org.junit.runner.RunWith
import org.mockito.Mockito.anyInt
import org.mockito.Mockito.never
import org.mockito.Mockito.verify

@ExperimentalCoroutinesApi
@RunWith(AndroidJUnit4::class)
class DefaultTopSitesStorageTest {

    @get:Rule
    val coroutinesTestRule = MainCoroutineRule()

    private val pinnedSitesStorage: PinnedSiteStorage = mock()
    private val historyStorage: PlacesHistoryStorage = mock()
    private val topSitesProvider: TopSitesProvider = mock()

    @Test
    fun `default top sites are added to pinned site storage on init`() = runTestOnMain {
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
    fun `addPinnedSite`() = runTestOnMain {
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
    fun `removeTopSite`() = runTestOnMain {
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
    fun `updateTopSite`() = runTestOnMain {
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
    fun `GIVEN frecencyConfig and providerConfig are null WHEN getTopSites is called THEN only default and pinned sites are returned`() = runTestOnMain {
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

        var topSites = defaultTopSitesStorage.getTopSites(totalSites = 0)

        assertTrue(topSites.isEmpty())
        assertEquals(defaultTopSitesStorage.cachedTopSites, topSites)

        topSites = defaultTopSitesStorage.getTopSites(totalSites = 1)

        assertEquals(1, topSites.size)
        assertEquals(defaultSite, topSites[0])
        assertEquals(defaultTopSitesStorage.cachedTopSites, topSites)

        topSites = defaultTopSitesStorage.getTopSites(totalSites = 2)

        assertEquals(2, topSites.size)
        assertEquals(defaultSite, topSites[0])
        assertEquals(pinnedSite, topSites[1])
        assertEquals(defaultTopSitesStorage.cachedTopSites, topSites)

        topSites = defaultTopSitesStorage.getTopSites(totalSites = 5)

        assertEquals(2, topSites.size)
        assertEquals(defaultSite, topSites[0])
        assertEquals(pinnedSite, topSites[1])
        assertEquals(defaultTopSitesStorage.cachedTopSites, topSites)
    }

    @Test
    fun `GIVEN providerConfig is specified WHEN getTopSites is called THEN default, pinned and provided top sites are returned`() = runTestOnMain {
        val defaultTopSitesStorage = DefaultTopSitesStorage(
            pinnedSitesStorage = pinnedSitesStorage,
            historyStorage = historyStorage,
            topSitesProvider = topSitesProvider,
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
        val providedSite = TopSite.Provided(
            id = 3,
            title = "Mozilla",
            url = "https://mozilla.com",
            clickUrl = "https://mozilla.com/click",
            imageUrl = "https://test.com/image2.jpg",
            impressionUrl = "https://example.com",
            createdAt = 3
        )

        whenever(pinnedSitesStorage.getPinnedSites()).thenReturn(
            listOf(
                defaultSite,
                pinnedSite
            )
        )
        whenever(topSitesProvider.getTopSites()).thenReturn(listOf(providedSite))

        var topSites = defaultTopSitesStorage.getTopSites(totalSites = 0)

        assertTrue(topSites.isEmpty())
        assertEquals(defaultTopSitesStorage.cachedTopSites, topSites)

        topSites = defaultTopSitesStorage.getTopSites(
            totalSites = 1,
            providerConfig = TopSitesProviderConfig(
                showProviderTopSites = true
            )
        )

        assertEquals(1, topSites.size)
        assertEquals(defaultSite, topSites[0])
        assertEquals(defaultTopSitesStorage.cachedTopSites, topSites)

        topSites = defaultTopSitesStorage.getTopSites(
            totalSites = 2,
            providerConfig = TopSitesProviderConfig(
                showProviderTopSites = true
            )
        )

        assertEquals(2, topSites.size)
        assertEquals(defaultSite, topSites[0])
        assertEquals(pinnedSite, topSites[1])
        assertEquals(defaultTopSitesStorage.cachedTopSites, topSites)

        topSites = defaultTopSitesStorage.getTopSites(
            totalSites = 5,
            providerConfig = TopSitesProviderConfig(
                showProviderTopSites = true
            )
        )

        assertEquals(3, topSites.size)
        assertEquals(providedSite, topSites[0])
        assertEquals(defaultSite, topSites[1])
        assertEquals(pinnedSite, topSites[2])
        assertEquals(defaultTopSitesStorage.cachedTopSites, topSites)

        topSites = defaultTopSitesStorage.getTopSites(
            totalSites = 5,
            providerConfig = TopSitesProviderConfig(
                showProviderTopSites = false
            )
        )

        assertEquals(2, topSites.size)
        assertEquals(defaultSite, topSites[0])
        assertEquals(pinnedSite, topSites[1])
        assertEquals(defaultTopSitesStorage.cachedTopSites, topSites)

        topSites = defaultTopSitesStorage.getTopSites(
            totalSites = 5,
            providerConfig = TopSitesProviderConfig(
                showProviderTopSites = true,
                maxThreshold = 8
            )
        )

        assertEquals(3, topSites.size)
        assertEquals(providedSite, topSites[0])
        assertEquals(defaultSite, topSites[1])
        assertEquals(pinnedSite, topSites[2])
        assertEquals(defaultTopSitesStorage.cachedTopSites, topSites)

        topSites = defaultTopSitesStorage.getTopSites(
            totalSites = 5,
            providerConfig = TopSitesProviderConfig(
                showProviderTopSites = true,
                maxThreshold = 2
            )
        )

        assertEquals(2, topSites.size)
        assertEquals(defaultSite, topSites[0])
        assertEquals(pinnedSite, topSites[1])
        assertEquals(defaultTopSitesStorage.cachedTopSites, topSites)
    }

    @Test
    fun `GIVEN providerConfig with maxThreshold is specified WHEN getTopSites is called THEN the correct number of provided top sites are returned`() = runTestOnMain {
        val defaultTopSitesStorage = DefaultTopSitesStorage(
            pinnedSitesStorage = pinnedSitesStorage,
            historyStorage = historyStorage,
            topSitesProvider = topSitesProvider,
            defaultTopSites = listOf(),
            coroutineContext = coroutineContext
        )

        val defaultSite = TopSite.Default(
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
        val providedSite1 = TopSite.Provided(
            id = 4,
            title = "Mozilla",
            url = "https://mozilla.com",
            clickUrl = "https://mozilla.com/click",
            imageUrl = "https://test.com/image2.jpg",
            impressionUrl = "https://example.com",
            createdAt = 3
        )
        val providedSite2 = TopSite.Provided(
            id = 5,
            title = "Pocket",
            url = "https://pocket.com",
            clickUrl = "https://mozilla.com/click",
            imageUrl = "https://test.com/image2.jpg",
            impressionUrl = "https://example.com",
            createdAt = 3
        )

        whenever(pinnedSitesStorage.getPinnedSites()).thenReturn(
            listOf(
                defaultSite,
                pinnedSite1,
                pinnedSite2,
                defaultSite,
                pinnedSite1,
                pinnedSite2
            )
        )
        whenever(topSitesProvider.getTopSites()).thenReturn(listOf(providedSite1, providedSite2))

        var topSites = defaultTopSitesStorage.getTopSites(
            totalSites = 8,
            providerConfig = TopSitesProviderConfig(
                showProviderTopSites = true,
                maxThreshold = 8
            )
        )

        assertEquals(8, topSites.size)
        assertEquals(providedSite1, topSites[0])
        assertEquals(providedSite2, topSites[1])
        assertEquals(defaultSite, topSites[2])
        assertEquals(pinnedSite1, topSites[3])
        assertEquals(pinnedSite2, topSites[4])
        assertEquals(defaultSite, topSites[5])
        assertEquals(pinnedSite1, topSites[6])
        assertEquals(pinnedSite2, topSites[7])
        assertEquals(defaultTopSitesStorage.cachedTopSites, topSites)

        whenever(pinnedSitesStorage.getPinnedSites()).thenReturn(
            listOf(
                defaultSite,
                pinnedSite1,
                pinnedSite2,
                defaultSite,
                pinnedSite1,
                pinnedSite2,
                defaultSite
            )
        )

        topSites = defaultTopSitesStorage.getTopSites(
            totalSites = 8,
            providerConfig = TopSitesProviderConfig(
                showProviderTopSites = true,
                maxThreshold = 8
            )
        )

        assertEquals(8, topSites.size)
        assertEquals(providedSite1, topSites[0])
        assertEquals(defaultSite, topSites[1])
        assertEquals(pinnedSite1, topSites[2])
        assertEquals(pinnedSite2, topSites[3])
        assertEquals(defaultSite, topSites[4])
        assertEquals(pinnedSite1, topSites[5])
        assertEquals(pinnedSite2, topSites[6])
        assertEquals(defaultSite, topSites[7])
        assertEquals(defaultTopSitesStorage.cachedTopSites, topSites)

        whenever(pinnedSitesStorage.getPinnedSites()).thenReturn(
            listOf(
                defaultSite,
                pinnedSite1,
                pinnedSite2,
                defaultSite,
                pinnedSite1,
                pinnedSite2,
                defaultSite,
                pinnedSite1
            )
        )

        topSites = defaultTopSitesStorage.getTopSites(
            totalSites = 8,
            providerConfig = TopSitesProviderConfig(
                showProviderTopSites = true,
                maxThreshold = 8
            )
        )

        assertEquals(8, topSites.size)
        assertEquals(defaultSite, topSites[0])
        assertEquals(pinnedSite1, topSites[1])
        assertEquals(pinnedSite2, topSites[2])
        assertEquals(defaultSite, topSites[3])
        assertEquals(pinnedSite1, topSites[4])
        assertEquals(pinnedSite2, topSites[5])
        assertEquals(defaultSite, topSites[6])
        assertEquals(pinnedSite1, topSites[7])
        assertEquals(defaultTopSitesStorage.cachedTopSites, topSites)
    }

    @Test
    fun `GIVEN frecencyConfig and providerConfig are specified WHEN getTopSites is called THEN default, pinned, provided and frecent top sites are returned`() = runTestOnMain {
        val defaultTopSitesStorage = DefaultTopSitesStorage(
            pinnedSitesStorage = pinnedSitesStorage,
            historyStorage = historyStorage,
            topSitesProvider = topSitesProvider,
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
        val providedSite = TopSite.Provided(
            id = 3,
            title = "Mozilla",
            url = "https://mozilla.com",
            clickUrl = "https://mozilla.com/click",
            imageUrl = "https://test.com/image2.jpg",
            impressionUrl = "https://example.com",
            createdAt = 3
        )

        whenever(pinnedSitesStorage.getPinnedSites()).thenReturn(
            listOf(
                defaultSite,
                pinnedSite
            )
        )
        whenever(topSitesProvider.getTopSites()).thenReturn(listOf(providedSite))

        val frecentSite1 = TopFrecentSiteInfo("https://getpocket.com", "Pocket")
        whenever(historyStorage.getTopFrecentSites(anyInt(), any())).thenReturn(listOf(frecentSite1))

        var topSites = defaultTopSitesStorage.getTopSites(
            totalSites = 0,
            frecencyConfig = TopSitesFrecencyConfig(
                frecencyTresholdOption = FrecencyThresholdOption.NONE
            ),
            providerConfig = TopSitesProviderConfig(
                showProviderTopSites = true
            )
        )

        assertTrue(topSites.isEmpty())

        topSites = defaultTopSitesStorage.getTopSites(
            totalSites = 1,
            frecencyConfig = TopSitesFrecencyConfig(
                frecencyTresholdOption = FrecencyThresholdOption.NONE
            ),
            providerConfig = TopSitesProviderConfig(
                showProviderTopSites = true
            )
        )

        assertEquals(1, topSites.size)
        assertEquals(defaultSite, topSites[0])
        assertEquals(defaultTopSitesStorage.cachedTopSites, topSites)

        topSites = defaultTopSitesStorage.getTopSites(
            totalSites = 2,
            frecencyConfig = TopSitesFrecencyConfig(
                frecencyTresholdOption = FrecencyThresholdOption.NONE
            ),
            providerConfig = TopSitesProviderConfig(
                showProviderTopSites = true
            )
        )

        assertEquals(2, topSites.size)
        assertEquals(defaultSite, topSites[0])
        assertEquals(pinnedSite, topSites[1])
        assertEquals(defaultTopSitesStorage.cachedTopSites, topSites)

        topSites = defaultTopSitesStorage.getTopSites(
            totalSites = 3,
            frecencyConfig = TopSitesFrecencyConfig(
                frecencyTresholdOption = FrecencyThresholdOption.NONE
            ),
            providerConfig = TopSitesProviderConfig(
                showProviderTopSites = true
            )
        )

        assertEquals(3, topSites.size)
        assertEquals(providedSite, topSites[0])
        assertEquals(defaultSite, topSites[1])
        assertEquals(pinnedSite, topSites[2])
        assertEquals(defaultTopSitesStorage.cachedTopSites, topSites)

        topSites = defaultTopSitesStorage.getTopSites(
            totalSites = 5,
            frecencyConfig = TopSitesFrecencyConfig(
                frecencyTresholdOption = FrecencyThresholdOption.NONE
            ),
            providerConfig = TopSitesProviderConfig(
                showProviderTopSites = true
            )
        )

        assertEquals(4, topSites.size)
        assertEquals(providedSite, topSites[0])
        assertEquals(defaultSite, topSites[1])
        assertEquals(pinnedSite, topSites[2])
        assertEquals(frecentSite1.toTopSite(), topSites[3])
        assertEquals(defaultTopSitesStorage.cachedTopSites, topSites)
    }

    @Test
    fun `getTopSites returns pinned and frecent sites when frecencyConfig is specified`() = runTestOnMain {
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

        var topSites = defaultTopSitesStorage.getTopSites(
            totalSites = 0,
            frecencyConfig = TopSitesFrecencyConfig(
                frecencyTresholdOption = FrecencyThresholdOption.NONE
            )
        )

        assertTrue(topSites.isEmpty())

        topSites = defaultTopSitesStorage.getTopSites(
            totalSites = 1,
            frecencyConfig = TopSitesFrecencyConfig(
                frecencyTresholdOption = FrecencyThresholdOption.NONE
            )
        )

        assertEquals(1, topSites.size)
        assertEquals(defaultSite, topSites[0])
        assertEquals(defaultTopSitesStorage.cachedTopSites, topSites)

        topSites = defaultTopSitesStorage.getTopSites(
            totalSites = 2,
            frecencyConfig = TopSitesFrecencyConfig(
                frecencyTresholdOption = FrecencyThresholdOption.NONE
            )
        )

        assertEquals(2, topSites.size)
        assertEquals(defaultSite, topSites[0])
        assertEquals(pinnedSite, topSites[1])
        assertEquals(defaultTopSitesStorage.cachedTopSites, topSites)

        topSites = defaultTopSitesStorage.getTopSites(
            totalSites = 5,
            frecencyConfig = TopSitesFrecencyConfig(
                frecencyTresholdOption = FrecencyThresholdOption.NONE
            )
        )

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

        topSites = defaultTopSitesStorage.getTopSites(
            totalSites = 5,
            frecencyConfig = TopSitesFrecencyConfig(
                frecencyTresholdOption = FrecencyThresholdOption.NONE
            )
        )

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

        topSites = defaultTopSitesStorage.getTopSites(
            totalSites = 5,
            frecencyConfig = TopSitesFrecencyConfig(
                frecencyTresholdOption = FrecencyThresholdOption.NONE
            )
        )

        assertEquals(5, topSites.size)
        assertEquals(defaultSite, topSites[0])
        assertEquals(pinnedSite, topSites[1])
        assertEquals(frecentSite1.toTopSite(), topSites[2])
        assertEquals(frecentSite2.toTopSite(), topSites[3])
        assertEquals(frecentSite3.toTopSite(), topSites[4])
        assertEquals(defaultTopSitesStorage.cachedTopSites, topSites)
    }

    @Test
    fun `getTopSites filters out frecent sites that already exist in pinned sites`() = runTestOnMain {
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

        val topSites = defaultTopSitesStorage.getTopSites(
            totalSites = 5,
            frecencyConfig = TopSitesFrecencyConfig(
                frecencyTresholdOption = FrecencyThresholdOption.NONE
            )
        )

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

    @Test
    fun `GIVEN providerFilter is set WHEN getTopSites is called THEN the provided top sites are filtered`() = runTestOnMain {
        val defaultTopSitesStorage = DefaultTopSitesStorage(
            pinnedSitesStorage = pinnedSitesStorage,
            historyStorage = historyStorage,
            topSitesProvider = topSitesProvider,
            coroutineContext = coroutineContext
        )

        val filteredUrl = "https://test.com"

        val providerConfig = TopSitesProviderConfig(
            showProviderTopSites = true,
            providerFilter = { topSite -> topSite.url != filteredUrl }
        )

        val defaultSite = TopSite.Default(
            id = 1,
            title = "Firefox",
            url = "https://firefox.com",
            createdAt = 1
        )
        val pinnedSite = TopSite.Pinned(
            id = 2,
            title = "Test",
            url = filteredUrl,
            createdAt = 2
        )
        val providedSite = TopSite.Provided(
            id = 3,
            title = "Mozilla",
            url = "https://mozilla.com",
            clickUrl = "https://mozilla.com/click",
            imageUrl = "https://test.com/image2.jpg",
            impressionUrl = "https://example.com",
            createdAt = 3
        )
        val providedFilteredSite = TopSite.Provided(
            id = 3,
            title = "Filtered",
            url = filteredUrl,
            clickUrl = "https://test.com/click",
            imageUrl = "https://test.com/image2.jpg",
            impressionUrl = "https://example.com",
            createdAt = 3
        )

        whenever(pinnedSitesStorage.getPinnedSites()).thenReturn(
            listOf(
                defaultSite,
                pinnedSite
            )
        )
        whenever(topSitesProvider.getTopSites()).thenReturn(listOf(providedSite, providedFilteredSite))

        val frecentSite1 = TopFrecentSiteInfo("https://getpocket.com", "Pocket")
        whenever(historyStorage.getTopFrecentSites(anyInt(), any())).thenReturn(listOf(frecentSite1))

        var topSites = defaultTopSitesStorage.getTopSites(
            totalSites = 3,
            frecencyConfig = TopSitesFrecencyConfig(
                frecencyTresholdOption = FrecencyThresholdOption.NONE
            ),
            providerConfig = providerConfig
        )

        assertEquals(3, topSites.size)
        assertEquals(providedSite, topSites[0])
        assertEquals(defaultSite, topSites[1])
        assertEquals(pinnedSite, topSites[2])
        assertEquals(defaultTopSitesStorage.cachedTopSites, topSites)

        topSites = defaultTopSitesStorage.getTopSites(
            totalSites = 4,
            frecencyConfig = TopSitesFrecencyConfig(
                frecencyTresholdOption = FrecencyThresholdOption.NONE
            ),
            providerConfig = providerConfig
        )

        assertEquals(4, topSites.size)
        assertEquals(providedSite, topSites[0])
        assertEquals(defaultSite, topSites[1])
        assertEquals(pinnedSite, topSites[2])
        assertEquals(frecentSite1.toTopSite(), topSites[3])
        assertEquals(defaultTopSitesStorage.cachedTopSites, topSites)
    }

    @Test
    fun `GIVEN frecent top sites exist as a pinned or provided site WHEN top sites are retrieved THEN filters out frecent sites that already exist in pinned or provided sites`() = runTestOnMain {
        val defaultTopSitesStorage = DefaultTopSitesStorage(
            pinnedSitesStorage = pinnedSitesStorage,
            historyStorage = historyStorage,
            topSitesProvider = topSitesProvider,
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
        val providedSite = TopSite.Provided(
            id = 3,
            title = "Firefox",
            url = "https://getfirefox.com",
            clickUrl = "https://getfirefox.com/click",
            imageUrl = "https://test.com/image2.jpg",
            impressionUrl = "https://example.com",
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
        whenever(topSitesProvider.getTopSites()).thenReturn(listOf(providedSite))

        val frecentSiteWithNoTitle = TopFrecentSiteInfo("https://mozilla.com", "")
        val frecentSiteFirefox = TopFrecentSiteInfo("https://firefox.com", "Firefox")
        val frecentSite1 = TopFrecentSiteInfo("https://getpocket.com", "Pocket")
        val frecentSite2 = TopFrecentSiteInfo("https://www.example.com", "Example")
        val frecentSite3 = TopFrecentSiteInfo("https://www.getfirefox.com", "Firefox")

        whenever(historyStorage.getTopFrecentSites(anyInt(), any())).thenReturn(
            listOf(
                frecentSiteWithNoTitle,
                frecentSiteFirefox,
                frecentSite1,
                frecentSite2,
                frecentSite3
            )
        )

        val topSites = defaultTopSitesStorage.getTopSites(
            totalSites = 10,
            frecencyConfig = TopSitesFrecencyConfig(
                frecencyTresholdOption = FrecencyThresholdOption.NONE
            ),
            providerConfig = TopSitesProviderConfig(
                showProviderTopSites = true
            )
        )

        verify(historyStorage).getTopFrecentSites(10, frecencyThreshold = FrecencyThresholdOption.NONE)

        assertEquals(6, topSites.size)
        assertEquals(providedSite, topSites[0])
        assertEquals(defaultSiteFirefox, topSites[1])
        assertEquals(pinnedSite1, topSites[2])
        assertEquals(pinnedSite2, topSites[3])
        assertEquals(frecentSiteWithNoTitle.toTopSite(), topSites[4])
        assertEquals(frecentSite1.toTopSite(), topSites[5])
        assertEquals("mozilla.com", frecentSiteWithNoTitle.toTopSite().title)
        assertEquals(defaultTopSitesStorage.cachedTopSites, topSites)
    }

    @Test
    fun `GIVEN frecencyFilter is set WHEN getTopSites is called THEN the frecent top sites are filtered`() = runTestOnMain {
        val defaultTopSitesStorage = DefaultTopSitesStorage(
            pinnedSitesStorage = pinnedSitesStorage,
            historyStorage = historyStorage,
            topSitesProvider = topSitesProvider,
            coroutineContext = coroutineContext
        )

        val filterMethod: ((TopSite) -> Boolean) = { topSite ->
            val uri = Uri.parse(topSite.url)
            if (!uri.queryParameterNames.contains("key")) {
                true
            } else {
                uri.getQueryParameter("key") != "value"
            }
        }

        val filteredUrl = "https://test.com/?key=value"

        val frecencyConfig = TopSitesFrecencyConfig(
            frecencyTresholdOption = FrecencyThresholdOption.NONE,
            frecencyFilter = filterMethod
        )

        val defaultSite = TopSite.Default(
            id = 1,
            title = "Firefox",
            url = "https://firefox.com",
            createdAt = 1
        )
        val pinnedSite = TopSite.Pinned(
            id = 2,
            title = "Test",
            url = "https://test.com",
            createdAt = 2
        )

        whenever(pinnedSitesStorage.getPinnedSites()).thenReturn(
            listOf(
                defaultSite,
                pinnedSite
            )
        )

        val providedFilteredSite = TopSite.Provided(
            id = 3,
            title = "Filtered",
            url = "https://test.com",
            clickUrl = "https://test.com/click",
            imageUrl = "https://test.com/image2.jpg",
            impressionUrl = "https://example.com",
            createdAt = 3
        )

        whenever(topSitesProvider.getTopSites()).thenReturn(
            listOf(
                providedFilteredSite
            )
        )

        val frecentSite = TopFrecentSiteInfo("https://getpocket.com", "Pocket")

        val frecentFilteredSite = TopFrecentSiteInfo(filteredUrl, "testSearch")

        whenever(historyStorage.getTopFrecentSites(anyInt(), any())).thenReturn(
            listOf(
                frecentSite,
                frecentFilteredSite
            )
        )

        var topSites = defaultTopSitesStorage.getTopSites(
            totalSites = 4,
            frecencyConfig = frecencyConfig,
            providerConfig = TopSitesProviderConfig(showProviderTopSites = true)
        )

        assertEquals(4, topSites.size)
        assertTrue(topSites.contains(frecentSite.toTopSite()))
        assertTrue(topSites.contains(providedFilteredSite))
        assertTrue(topSites.contains(defaultSite))
        assertTrue(topSites.contains(pinnedSite))
        assertEquals(defaultTopSitesStorage.cachedTopSites, topSites)

        topSites = defaultTopSitesStorage.getTopSites(
            totalSites = 5,
            frecencyConfig = frecencyConfig,
            providerConfig = TopSitesProviderConfig(showProviderTopSites = true)
        )

        assertEquals(4, topSites.size)
        assertTrue(topSites.contains(frecentSite.toTopSite()))
        assertTrue(topSites.contains(providedFilteredSite))
        assertTrue(topSites.contains(defaultSite))
        assertTrue(topSites.contains(pinnedSite))
        assertEquals(defaultTopSitesStorage.cachedTopSites, topSites)
    }
}
