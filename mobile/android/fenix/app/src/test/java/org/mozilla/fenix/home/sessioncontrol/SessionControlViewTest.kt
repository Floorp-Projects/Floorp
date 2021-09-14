/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

package org.mozilla.fenix.home.sessioncontrol

import io.mockk.every
import io.mockk.mockk
import io.mockk.spyk
import mozilla.components.browser.state.state.TabSessionState
import mozilla.components.concept.storage.BookmarkNode
import mozilla.components.concept.storage.BookmarkNodeType
import mozilla.components.feature.tab.collections.TabCollection
import mozilla.components.feature.top.sites.TopSite
import mozilla.components.service.pocket.PocketRecommendedStory
import mozilla.components.support.test.robolectric.testContext
import org.junit.Assert.assertTrue
import org.junit.Test
import org.junit.runner.RunWith
import org.mozilla.fenix.ext.settings
import org.mozilla.fenix.helpers.FenixRobolectricTestRunner
import org.mozilla.fenix.historymetadata.HistoryMetadataGroup
import org.mozilla.fenix.utils.Settings

@RunWith(FenixRobolectricTestRunner::class)
class SessionControlViewTest {
    @Test
    fun `GIVEN recent Bookmarks WHEN normalModeAdapterItems is called THEN add a customize home button`() {
        val topSites = emptyList<TopSite>()
        val collections = emptyList<TabCollection>()
        val expandedCollections = emptySet<Long>()
        val recentBookmarks =
            listOf(BookmarkNode(BookmarkNodeType.ITEM, "guid", null, null, null, null, 0, null))
        val recentTabs = emptyList<TabSessionState>()
        val historyMetadata = emptyList<HistoryMetadataGroup>()
        val pocketArticles = emptyList<PocketRecommendedStory>()

        val results = normalModeAdapterItems(
            testContext,
            topSites,
            collections,
            expandedCollections,
            null,
            recentBookmarks,
            false,
            false,
            recentTabs,
            historyMetadata,
            pocketArticles
        )

        assertTrue(results[0] is AdapterItem.RecentBookmarks)
        assertTrue(results[1] is AdapterItem.CustomizeHomeButton)
    }

    @Test
    fun `GIVEN recent tabs WHEN normalModeAdapterItems is called THEN add a customize home button`() {
        val topSites = emptyList<TopSite>()
        val collections = emptyList<TabCollection>()
        val expandedCollections = emptySet<Long>()
        val recentBookmarks = listOf<BookmarkNode>()
        val recentTabs = listOf<TabSessionState>(mockk())
        val historyMetadata = emptyList<HistoryMetadataGroup>()
        val pocketArticles = emptyList<PocketRecommendedStory>()

        val results = normalModeAdapterItems(
            testContext,
            topSites,
            collections,
            expandedCollections,
            null,
            recentBookmarks,
            false,
            false,
            recentTabs,
            historyMetadata,
            pocketArticles
        )

        assertTrue(results[0] is AdapterItem.RecentTabsHeader)
        assertTrue(results[1] is AdapterItem.RecentTabItem)
        assertTrue(results[2] is AdapterItem.CustomizeHomeButton)
    }

    @Test
    fun `GIVEN history metadata WHEN normalModeAdapterItems is called THEN add a customize home button`() {
        val topSites = emptyList<TopSite>()
        val collections = emptyList<TabCollection>()
        val expandedCollections = emptySet<Long>()
        val recentBookmarks = listOf<BookmarkNode>()
        val recentTabs = emptyList<TabSessionState>()
        val historyMetadata = listOf(HistoryMetadataGroup("title", emptyList(), false))
        val pocketArticles = emptyList<PocketRecommendedStory>()

        val results = normalModeAdapterItems(
            testContext,
            topSites,
            collections,
            expandedCollections,
            null,
            recentBookmarks,
            false,
            false,
            recentTabs,
            historyMetadata,
            pocketArticles
        )

        assertTrue(results[0] is AdapterItem.HistoryMetadataHeader)
        assertTrue(results[1] is AdapterItem.HistoryMetadataGroup)
        assertTrue(results[2] is AdapterItem.CustomizeHomeButton)
    }

    @Test
    fun `GIVEN pocket articles WHEN normalModeAdapterItems is called THEN add a customize home button`() {
        val topSites = emptyList<TopSite>()
        val collections = emptyList<TabCollection>()
        val expandedCollections = emptySet<Long>()
        val recentBookmarks = listOf<BookmarkNode>()
        val recentTabs = emptyList<TabSessionState>()
        val historyMetadata = emptyList<HistoryMetadataGroup>()
        val pocketArticles = listOf(PocketRecommendedStory("", "", "", "", 0, ""))
        val context = spyk(testContext)

        val settings: Settings = mockk()
        every { settings.pocketRecommendations } returns true
        every { context.settings() } returns settings

        val results = normalModeAdapterItems(
            context,
            topSites,
            collections,
            expandedCollections,
            null,
            recentBookmarks,
            false,
            false,
            recentTabs,
            historyMetadata,
            pocketArticles
        )

        assertTrue(results[0] is AdapterItem.PocketStoriesItem)
        assertTrue(results[1] is AdapterItem.CustomizeHomeButton)
    }

    @Test
    fun `GIVEN none recentBookmarks,recentTabs, historyMetadata or pocketArticles WHEN normalModeAdapterItems is called THEN the customize home button is not added`() {
        val topSites = emptyList<TopSite>()
        val collections = emptyList<TabCollection>()
        val expandedCollections = emptySet<Long>()
        val recentBookmarks = listOf<BookmarkNode>()
        val recentTabs = emptyList<TabSessionState>()
        val historyMetadata = emptyList<HistoryMetadataGroup>()
        val pocketArticles = emptyList<PocketRecommendedStory>()
        val context = spyk(testContext)

        val settings: Settings = mockk()
        every { settings.pocketRecommendations } returns true
        every { context.settings() } returns settings

        val results = normalModeAdapterItems(
            context,
            topSites,
            collections,
            expandedCollections,
            null,
            recentBookmarks,
            false,
            false,
            recentTabs,
            historyMetadata,
            pocketArticles
        )
        assertTrue(results.isEmpty())
    }
}
