/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

package org.mozilla.fenix.home.sessioncontrol

import androidx.recyclerview.widget.RecyclerView
import io.mockk.every
import io.mockk.mockk
import io.mockk.verify
import mozilla.components.feature.tab.collections.TabCollection
import mozilla.components.feature.top.sites.TopSite
import mozilla.components.service.pocket.PocketRecommendedStory
import mozilla.components.support.test.robolectric.testContext
import org.junit.Assert.assertEquals
import org.junit.Assert.assertFalse
import org.junit.Assert.assertTrue
import org.junit.Test
import org.junit.runner.RunWith
import org.mozilla.fenix.components.appstate.AppState
import org.mozilla.fenix.ext.components
import org.mozilla.fenix.gleanplumb.Message
import org.mozilla.fenix.helpers.FenixRobolectricTestRunner
import org.mozilla.fenix.home.recentbookmarks.RecentBookmark
import org.mozilla.fenix.home.recenttabs.RecentTab
import org.mozilla.fenix.home.recentvisits.RecentlyVisitedItem.RecentHistoryGroup
import org.mozilla.fenix.utils.Settings

@RunWith(FenixRobolectricTestRunner::class)
class SessionControlViewTest {

    @Test
    fun `GIVEN recent Bookmarks WHEN calling shouldShowHomeOnboardingDialog THEN show the dialog `() {
        val recentBookmarks = listOf(RecentBookmark())
        val settings: Settings = mockk()

        every { settings.hasShownHomeOnboardingDialog } returns false

        val state = AppState(recentBookmarks = recentBookmarks)

        assertTrue(state.shouldShowHomeOnboardingDialog(settings))
    }

    @Test
    fun `GIVEN recentTabs WHEN calling shouldShowHomeOnboardingDialog THEN show the dialog `() {
        val recentTabs = listOf<RecentTab>(mockk())
        val settings: Settings = mockk()

        every { settings.hasShownHomeOnboardingDialog } returns false

        val state = AppState(recentTabs = recentTabs)

        assertTrue(state.shouldShowHomeOnboardingDialog(settings))
    }

    @Test
    fun `GIVEN historyMetadata WHEN calling shouldShowHomeOnboardingDialog THEN show the dialog `() {
        val historyMetadata = listOf(RecentHistoryGroup("title", emptyList()))
        val settings: Settings = mockk()

        every { settings.hasShownHomeOnboardingDialog } returns false

        val state = AppState(recentHistory = historyMetadata)

        assertTrue(state.shouldShowHomeOnboardingDialog(settings))
    }

    @Test
    fun `GIVEN pocketArticles WHEN calling shouldShowHomeOnboardingDialog THEN show the dialog `() {
        val pocketArticles = listOf(PocketRecommendedStory("", "", "", "", "", 0, 0))
        val settings: Settings = mockk()

        every { settings.hasShownHomeOnboardingDialog } returns false

        val state = AppState(pocketStories = pocketArticles)

        assertTrue(state.shouldShowHomeOnboardingDialog(settings))
    }

    @Test
    fun `GIVEN the home onboading dialog has been shown before WHEN calling shouldShowHomeOnboardingDialog THEN DO NOT showthe dialog `() {
        val pocketArticles = listOf(PocketRecommendedStory("", "", "", "", "", 0, 0))
        val settings: Settings = mockk()

        every { settings.hasShownHomeOnboardingDialog } returns true

        val state = AppState(pocketStories = pocketArticles)

        assertFalse(state.shouldShowHomeOnboardingDialog(settings))
    }

    @Test
    fun `GIVENs updates WHEN sections recentTabs, recentBookmarks, historyMetadata or pocketArticles are available THEN show the dialog`() {
        every { testContext.components.settings } returns mockk(relaxed = true)
        val interactor = mockk<SessionControlInteractor>(relaxed = true)
        val view = RecyclerView(testContext)
        val controller = SessionControlView(
            mockk(relaxed = true),
            view,
            mockk(relaxed = true),
            interactor
        )
        val recentTabs = listOf<RecentTab>(mockk(relaxed = true))

        val state = AppState(recentTabs = recentTabs)

        controller.update(state)

        verify {
            interactor.showOnboardingDialog()
        }
    }

    @Test
    fun `GIVENs updates WHEN sections recentTabs, recentBookmarks, historyMetadata or pocketArticles are NOT available THEN DO NOT show the dialog`() {
        every { testContext.components.settings } returns mockk(relaxed = true)
        val interactor = mockk<SessionControlInteractor>(relaxed = true)
        val view = RecyclerView(testContext)
        val controller = SessionControlView(
            mockk(relaxed = true),
            view,
            mockk(relaxed = true),
            interactor
        )

        val state = AppState()

        controller.update(state)

        verify(exactly = 0) {
            interactor.showOnboardingDialog()
        }
    }

    @Test
    fun `GIVEN recent Bookmarks WHEN normalModeAdapterItems is called THEN add a customize home button`() {
        val settings: Settings = mockk()
        val topSites = emptyList<TopSite>()
        val collections = emptyList<TabCollection>()
        val expandedCollections = emptySet<Long>()
        val recentBookmarks = listOf(RecentBookmark())
        val recentTabs = emptyList<RecentTab.Tab>()
        val historyMetadata = emptyList<RecentHistoryGroup>()
        val pocketArticles = emptyList<PocketRecommendedStory>()

        every { settings.showTopSitesFeature } returns true
        every { settings.showRecentTabsFeature } returns true
        every { settings.showRecentBookmarksFeature } returns true
        every { settings.historyMetadataUIFeature } returns true
        every { settings.showPocketRecommendationsFeature } returns true

        val results = normalModeAdapterItems(
            settings,
            topSites,
            collections,
            expandedCollections,
            recentBookmarks,
            false,
            null,
            recentTabs,
            historyMetadata,
            pocketArticles
        )

        assertTrue(results[0] is AdapterItem.TopPlaceholderItem)
        assertTrue(results[1] is AdapterItem.RecentBookmarksHeader)
        assertTrue(results[2] is AdapterItem.RecentBookmarks)
        assertTrue(results[3] is AdapterItem.CustomizeHomeButton)
    }

    @Test
    fun `GIVEN a nimbusMessageCard WHEN normalModeAdapterItems is called THEN add a NimbusMessageCard`() {
        val settings: Settings = mockk()
        val topSites = emptyList<TopSite>()
        val collections = emptyList<TabCollection>()
        val expandedCollections = emptySet<Long>()
        val recentBookmarks = listOf(RecentBookmark())
        val recentTabs = emptyList<RecentTab.Tab>()
        val historyMetadata = emptyList<RecentHistoryGroup>()
        val pocketArticles = emptyList<PocketRecommendedStory>()
        val nimbusMessageCard: Message = mockk()

        every { settings.showTopSitesFeature } returns true
        every { settings.showRecentTabsFeature } returns true
        every { settings.showRecentBookmarksFeature } returns true
        every { settings.historyMetadataUIFeature } returns true
        every { settings.showPocketRecommendationsFeature } returns true

        val results = normalModeAdapterItems(
            settings,
            topSites,
            collections,
            expandedCollections,
            recentBookmarks,
            false,
            nimbusMessageCard,
            recentTabs,
            historyMetadata,
            pocketArticles
        )

        assertTrue(results.contains(AdapterItem.NimbusMessageCard(nimbusMessageCard)))
    }

    @Test
    fun `GIVEN recent tabs WHEN normalModeAdapterItems is called THEN add a customize home button`() {
        val settings: Settings = mockk()
        val topSites = emptyList<TopSite>()
        val collections = emptyList<TabCollection>()
        val expandedCollections = emptySet<Long>()
        val recentBookmarks = listOf<RecentBookmark>()
        val recentTabs = listOf<RecentTab.Tab>(mockk())
        val historyMetadata = emptyList<RecentHistoryGroup>()
        val pocketArticles = emptyList<PocketRecommendedStory>()

        every { settings.showTopSitesFeature } returns true
        every { settings.showRecentTabsFeature } returns true
        every { settings.showRecentBookmarksFeature } returns true
        every { settings.historyMetadataUIFeature } returns true
        every { settings.showPocketRecommendationsFeature } returns true

        val results = normalModeAdapterItems(
            settings,
            topSites,
            collections,
            expandedCollections,
            recentBookmarks,
            false,
            null,
            recentTabs,
            historyMetadata,
            pocketArticles
        )

        assertTrue(results[0] is AdapterItem.TopPlaceholderItem)
        assertTrue(results[1] is AdapterItem.RecentTabsHeader)
        assertTrue(results[2] is AdapterItem.RecentTabItem)
        assertTrue(results[3] is AdapterItem.CustomizeHomeButton)
    }

    @Test
    fun `GIVEN history metadata WHEN normalModeAdapterItems is called THEN add a customize home button`() {
        val settings: Settings = mockk()
        val topSites = emptyList<TopSite>()
        val collections = emptyList<TabCollection>()
        val expandedCollections = emptySet<Long>()
        val recentBookmarks = listOf<RecentBookmark>()
        val recentTabs = emptyList<RecentTab.Tab>()
        val historyMetadata = listOf(RecentHistoryGroup("title", emptyList()))
        val pocketArticles = emptyList<PocketRecommendedStory>()

        every { settings.showTopSitesFeature } returns true
        every { settings.showRecentTabsFeature } returns true
        every { settings.showRecentBookmarksFeature } returns true
        every { settings.historyMetadataUIFeature } returns true
        every { settings.showPocketRecommendationsFeature } returns true

        val results = normalModeAdapterItems(
            settings,
            topSites,
            collections,
            expandedCollections,
            recentBookmarks,
            false,
            null,
            recentTabs,
            historyMetadata,
            pocketArticles
        )

        assertTrue(results[0] is AdapterItem.TopPlaceholderItem)
        assertTrue(results[1] is AdapterItem.RecentVisitsHeader)
        assertTrue(results[2] is AdapterItem.RecentVisitsItems)
        assertTrue(results[3] is AdapterItem.CustomizeHomeButton)
    }

    @Test
    fun `GIVEN pocket articles WHEN normalModeAdapterItems is called THEN add a customize home button`() {
        val settings: Settings = mockk()
        val topSites = emptyList<TopSite>()
        val collections = emptyList<TabCollection>()
        val expandedCollections = emptySet<Long>()
        val recentBookmarks = listOf<RecentBookmark>()
        val recentTabs = emptyList<RecentTab.Tab>()
        val historyMetadata = emptyList<RecentHistoryGroup>()
        val pocketArticles = listOf(PocketRecommendedStory("", "", "", "", "", 1, 1))

        every { settings.showTopSitesFeature } returns true
        every { settings.showRecentTabsFeature } returns true
        every { settings.showRecentBookmarksFeature } returns true
        every { settings.historyMetadataUIFeature } returns true
        every { settings.showPocketRecommendationsFeature } returns true

        val results = normalModeAdapterItems(
            settings,
            topSites,
            collections,
            expandedCollections,
            recentBookmarks,
            false,
            null,
            recentTabs,
            historyMetadata,
            pocketArticles
        )

        assertTrue(results[0] is AdapterItem.TopPlaceholderItem)
        assertTrue(results[1] is AdapterItem.PocketStoriesItem)
        assertTrue(results[2] is AdapterItem.PocketCategoriesItem)
        assertTrue(results[3] is AdapterItem.PocketRecommendationsFooterItem)
        assertTrue(results[4] is AdapterItem.CustomizeHomeButton)
    }

    @Test
    fun `GIVEN none recentBookmarks,recentTabs, historyMetadata or pocketArticles WHEN normalModeAdapterItems is called THEN the customize home button is not added`() {
        val settings: Settings = mockk()
        val topSites = emptyList<TopSite>()
        val collections = emptyList<TabCollection>()
        val expandedCollections = emptySet<Long>()
        val recentBookmarks = listOf<RecentBookmark>()
        val recentTabs = emptyList<RecentTab.Tab>()
        val historyMetadata = emptyList<RecentHistoryGroup>()
        val pocketArticles = emptyList<PocketRecommendedStory>()

        every { settings.showTopSitesFeature } returns true
        every { settings.showRecentTabsFeature } returns true
        every { settings.showRecentBookmarksFeature } returns true
        every { settings.historyMetadataUIFeature } returns true
        every { settings.showPocketRecommendationsFeature } returns true

        val results = normalModeAdapterItems(
            settings,
            topSites,
            collections,
            expandedCollections,
            recentBookmarks,
            false,
            null,
            recentTabs,
            historyMetadata,
            pocketArticles
        )
        assertEquals(results.size, 2)
        assertTrue(results[0] is AdapterItem.TopPlaceholderItem)
    }

    @Test
    fun `GIVEN all items THEN top placeholder item is always the first item`() {
        val settings: Settings = mockk()
        val collection = mockk<TabCollection> {
            every { id } returns 123L
        }
        val topSites = listOf<TopSite>(mockk())
        val collections = listOf(collection)
        val expandedCollections = emptySet<Long>()
        val recentBookmarks = listOf<RecentBookmark>(mockk())
        val recentTabs = listOf<RecentTab.Tab>(mockk())
        val historyMetadata = listOf<RecentHistoryGroup>(mockk())
        val pocketArticles = listOf<PocketRecommendedStory>(mockk())

        every { settings.showTopSitesFeature } returns true
        every { settings.showRecentTabsFeature } returns true
        every { settings.showRecentBookmarksFeature } returns true
        every { settings.historyMetadataUIFeature } returns true
        every { settings.showPocketRecommendationsFeature } returns true

        val results = normalModeAdapterItems(
            settings,
            topSites,
            collections,
            expandedCollections,
            recentBookmarks,
            false,
            null,
            recentTabs,
            historyMetadata,
            pocketArticles
        )

        assertTrue(results[0] is AdapterItem.TopPlaceholderItem)
    }
}
