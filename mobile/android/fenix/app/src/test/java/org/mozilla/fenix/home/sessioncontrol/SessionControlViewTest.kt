/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

package org.mozilla.fenix.home.sessioncontrol

import io.mockk.every
import io.mockk.mockk
import mozilla.components.feature.tab.collections.TabCollection
import mozilla.components.feature.top.sites.TopSite
import mozilla.components.service.nimbus.messaging.Message
import mozilla.components.service.pocket.PocketStory
import mozilla.components.service.pocket.PocketStory.PocketRecommendedStory
import org.junit.Assert.assertEquals
import org.junit.Assert.assertTrue
import org.junit.Test
import org.junit.runner.RunWith
import org.mozilla.fenix.helpers.FenixRobolectricTestRunner
import org.mozilla.fenix.home.recentbookmarks.RecentBookmark
import org.mozilla.fenix.home.recentvisits.RecentlyVisitedItem.RecentHistoryGroup
import org.mozilla.fenix.utils.Settings

@RunWith(FenixRobolectricTestRunner::class)
class SessionControlViewTest {

    @Test
    fun `GIVEN recent Bookmarks WHEN normalModeAdapterItems is called THEN add a customize home button`() {
        val settings: Settings = mockk()
        val topSites = emptyList<TopSite>()
        val collections = emptyList<TabCollection>()
        val expandedCollections = emptySet<Long>()
        val recentBookmarks = listOf(RecentBookmark())
        val historyMetadata = emptyList<RecentHistoryGroup>()
        val pocketStories = emptyList<PocketStory>()

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
            false,
            showRecentSyncedTab = false,
            recentVisits = historyMetadata,
            pocketStories = pocketStories,
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
        val historyMetadata = emptyList<RecentHistoryGroup>()
        val pocketStories = emptyList<PocketStory>()
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
            false,
            showRecentSyncedTab = false,
            historyMetadata,
            pocketStories,
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
        val historyMetadata = emptyList<RecentHistoryGroup>()
        val pocketStories = emptyList<PocketStory>()

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
            true,
            showRecentSyncedTab = false,
            historyMetadata,
            pocketStories,
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
        val historyMetadata = listOf(RecentHistoryGroup("title", emptyList()))
        val pocketStories = emptyList<PocketStory>()

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
            false,
            showRecentSyncedTab = false,
            historyMetadata,
            pocketStories,
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
        val historyMetadata = emptyList<RecentHistoryGroup>()
        val pocketStories = listOf(PocketRecommendedStory("", "", "", "", "", 1, 1))

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
            false,
            showRecentSyncedTab = false,
            historyMetadata,
            pocketStories,
            true,
        )

        assertTrue(results[0] is AdapterItem.TopPlaceholderItem)
        assertTrue(results[1] is AdapterItem.PocketStoriesItem)
        assertTrue(results[2] is AdapterItem.PocketCategoriesItem)
        assertTrue(results[3] is AdapterItem.PocketRecommendationsFooterItem)
        assertTrue(results[4] is AdapterItem.CustomizeHomeButton)

        // When the first frame has not yet drawn don't add pocket.
        val results2 = normalModeAdapterItems(
            settings,
            topSites,
            collections,
            expandedCollections,
            recentBookmarks,
            false,
            null,
            false,
            showRecentSyncedTab = false,
            historyMetadata,
            pocketStories,
            false,
        )

        assertTrue(results2[0] is AdapterItem.TopPlaceholderItem)
        assertTrue(results2[1] is AdapterItem.BottomSpacer)
    }

    @Test
    fun `GIVEN none recentBookmarks,recentTabs, historyMetadata or pocketArticles WHEN normalModeAdapterItems is called THEN the customize home button is not added`() {
        val settings: Settings = mockk()
        val topSites = emptyList<TopSite>()
        val collections = emptyList<TabCollection>()
        val expandedCollections = emptySet<Long>()
        val recentBookmarks = listOf<RecentBookmark>()
        val historyMetadata = emptyList<RecentHistoryGroup>()
        val pocketStories = emptyList<PocketStory>()

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
            false,
            showRecentSyncedTab = false,
            historyMetadata,
            pocketStories,
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
        val historyMetadata = listOf<RecentHistoryGroup>(mockk())
        val pocketStories = listOf<PocketStory>(mockk())

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
            true,
            showRecentSyncedTab = true,
            historyMetadata,
            pocketStories,
        )

        assertTrue(results[0] is AdapterItem.TopPlaceholderItem)
    }
}
