/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

package org.mozilla.fenix.home

import android.content.Context
import io.mockk.every
import io.mockk.mockk
import kotlinx.coroutines.runBlocking
import mozilla.components.browser.state.state.TabSessionState
import mozilla.components.concept.storage.BookmarkNode
import mozilla.components.concept.storage.DocumentType
import mozilla.components.concept.storage.HistoryMetadata
import mozilla.components.concept.storage.HistoryMetadataKey
import mozilla.components.feature.tab.collections.TabCollection
import mozilla.components.feature.top.sites.TopSite
import mozilla.components.service.fxa.manager.FxaAccountManager
import org.junit.Assert.assertEquals
import org.junit.Assert.assertFalse
import org.junit.Assert.assertTrue
import org.junit.Before
import org.junit.Test
import org.mozilla.fenix.browser.browsingmode.BrowsingMode
import org.mozilla.fenix.browser.browsingmode.BrowsingModeManager
import org.mozilla.fenix.ext.components
import org.mozilla.fenix.historymetadata.HistoryMetadataGroup
import org.mozilla.fenix.onboarding.FenixOnboarding

class HomeFragmentStoreTest {
    private lateinit var context: Context
    private lateinit var accountManager: FxaAccountManager
    private lateinit var onboarding: FenixOnboarding
    private lateinit var browsingModeManager: BrowsingModeManager
    private lateinit var currentMode: CurrentMode
    private lateinit var homeFragmentState: HomeFragmentState
    private lateinit var homeFragmentStore: HomeFragmentStore

    @Before
    fun setup() {
        context = mockk(relaxed = true)
        accountManager = mockk(relaxed = true)
        onboarding = mockk(relaxed = true)
        browsingModeManager = mockk(relaxed = true)

        every { context.components.backgroundServices.accountManager } returns accountManager
        every { onboarding.userHasBeenOnboarded() } returns true
        every { browsingModeManager.mode } returns BrowsingMode.Normal

        currentMode = CurrentMode(
            context,
            onboarding,
            browsingModeManager
        ) {}

        homeFragmentState = HomeFragmentState(
            collections = emptyList(),
            expandedCollections = emptySet(),
            mode = currentMode.getCurrentMode(),
            topSites = emptyList(),
            showCollectionPlaceholder = true,
            showSetAsDefaultBrowserCard = true,
            recentTabs = emptyList()
        )

        homeFragmentStore = HomeFragmentStore(homeFragmentState)
    }

    @Test
    fun `Test toggling the mode in HomeFragmentStore`() = runBlocking {
        // Verify that the default mode and tab states of the HomeFragment are correct.
        assertEquals(Mode.Normal, homeFragmentStore.state.mode)

        // Change the HomeFragmentStore to Private mode.
        homeFragmentStore.dispatch(HomeFragmentAction.ModeChange(Mode.Private)).join()
        assertEquals(Mode.Private, homeFragmentStore.state.mode)

        // Change the HomeFragmentStore back to Normal mode.
        homeFragmentStore.dispatch(HomeFragmentAction.ModeChange(Mode.Normal)).join()
        assertEquals(Mode.Normal, homeFragmentStore.state.mode)
    }

    @Test
    fun `Test changing the collections in HomeFragmentStore`() = runBlocking {
        assertEquals(0, homeFragmentStore.state.collections.size)

        // Add 2 TabCollections to the HomeFragmentStore.
        val tabCollections: List<TabCollection> = listOf(mockk(), mockk())
        homeFragmentStore.dispatch(HomeFragmentAction.CollectionsChange(tabCollections)).join()

        assertEquals(tabCollections, homeFragmentStore.state.collections)
    }

    @Test
    fun `Test changing the top sites in HomeFragmentStore`() = runBlocking {
        assertEquals(0, homeFragmentStore.state.topSites.size)

        // Add 2 TopSites to the HomeFragmentStore.
        val topSites: List<TopSite> = listOf(mockk(), mockk())
        homeFragmentStore.dispatch(HomeFragmentAction.TopSitesChange(topSites)).join()

        assertEquals(topSites, homeFragmentStore.state.topSites)
    }

    @Test
    fun `Test changing the recent tabs in HomeFragmentStore`() = runBlocking {
        assertEquals(0, homeFragmentStore.state.recentTabs.size)

        // Add 2 TabSessionState to the HomeFragmentStore.
        val recentTabs: List<TabSessionState> = listOf(mockk(), mockk())
        homeFragmentStore.dispatch(HomeFragmentAction.RecentTabsChange(recentTabs)).join()

        assertEquals(recentTabs, homeFragmentStore.state.recentTabs)
    }

    @Test
    fun `Test changing the history metadata in HomeFragmentStore`() = runBlocking {
        assertEquals(0, homeFragmentStore.state.historyMetadata.size)

        val historyMetadata: List<HistoryMetadataGroup> = listOf(mockk(), mockk())
        homeFragmentStore.dispatch(HomeFragmentAction.HistoryMetadataChange(historyMetadata)).join()

        assertEquals(historyMetadata, homeFragmentStore.state.historyMetadata)
    }

    @Test
    fun `Test changing the expanded history metadata in HomeFragmentStore`() = runBlocking {
        val historyEntry = HistoryMetadata(
            key = HistoryMetadataKey("http://www.mozilla.com", "mozilla", null),
            title = "mozilla",
            createdAt = System.currentTimeMillis(),
            updatedAt = System.currentTimeMillis(),
            totalViewTime = 10,
            documentType = DocumentType.Regular
        )
        val historyGroup = HistoryMetadataGroup(
            title = "mozilla",
            historyMetadata = listOf(historyEntry),
            expanded = false
        )

        val historyMetadata = listOf(historyGroup)
        homeFragmentStore.dispatch(HomeFragmentAction.HistoryMetadataChange(historyMetadata)).join()

        assertEquals(historyMetadata, homeFragmentStore.state.historyMetadata)

        homeFragmentStore.dispatch(HomeFragmentAction.HistoryMetadataExpanded(historyGroup)).join()

        assertTrue(homeFragmentStore.state.historyMetadata.find { it.title == historyEntry.key.searchTerm }!!.expanded)
    }

    @Test
    fun `Test changing hiding collections placeholder`() = runBlocking {
        assertTrue(homeFragmentStore.state.showCollectionPlaceholder)

        homeFragmentStore.dispatch(HomeFragmentAction.RemoveCollectionsPlaceholder).join()

        assertFalse(homeFragmentStore.state.showCollectionPlaceholder)
    }

    @Test
    fun `Test changing the expanded collections in HomeFragmentStore`() = runBlocking {
        val collection: TabCollection = mockk<TabCollection>().apply {
            every { id } returns 0
        }

        // Expand the given collection.
        homeFragmentStore.dispatch(HomeFragmentAction.CollectionsChange(listOf(collection))).join()
        homeFragmentStore.dispatch(HomeFragmentAction.CollectionExpanded(collection, true)).join()

        assertTrue(homeFragmentStore.state.expandedCollections.contains(collection.id))
        assertEquals(1, homeFragmentStore.state.expandedCollections.size)
    }

    @Test
    fun `Test changing the collections, mode, recent tabs and bookmarks, history metadata and top sites in the HomeFragmentStore`() =
        runBlocking {
            // Verify that the default state of the HomeFragment is correct.
            assertEquals(0, homeFragmentStore.state.collections.size)
            assertEquals(0, homeFragmentStore.state.topSites.size)
            assertEquals(0, homeFragmentStore.state.recentTabs.size)
            assertEquals(0, homeFragmentStore.state.recentBookmarks.size)
            assertEquals(0, homeFragmentStore.state.historyMetadata.size)
            assertEquals(Mode.Normal, homeFragmentStore.state.mode)

            val collections: List<TabCollection> = listOf(mockk())
            val topSites: List<TopSite> = listOf(mockk(), mockk())
            val recentTabs: List<TabSessionState> = listOf(mockk(), mockk())
            val recentBookmarks: List<BookmarkNode> = listOf(mockk(), mockk())
            val historyMetadata: List<HistoryMetadataGroup> = listOf(mockk(), mockk())

            homeFragmentStore.dispatch(
                HomeFragmentAction.Change(
                    collections = collections,
                    mode = Mode.Private,
                    topSites = topSites,
                    showCollectionPlaceholder = true,
                    recentTabs = recentTabs,
                    recentBookmarks = recentBookmarks,
                    historyMetadata = historyMetadata
                )
            ).join()

            assertEquals(1, homeFragmentStore.state.collections.size)
            assertEquals(2, homeFragmentStore.state.topSites.size)
            assertEquals(2, homeFragmentStore.state.recentTabs.size)
            assertEquals(2, homeFragmentStore.state.recentBookmarks.size)
            assertEquals(2, homeFragmentStore.state.historyMetadata.size)
            assertEquals(Mode.Private, homeFragmentStore.state.mode)
        }
}
