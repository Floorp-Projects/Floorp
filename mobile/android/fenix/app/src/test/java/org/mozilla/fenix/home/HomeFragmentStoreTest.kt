/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

package org.mozilla.fenix.home

import android.content.Context
import io.mockk.every
import io.mockk.mockk
import io.mockk.mockkStatic
import io.mockk.verify
import kotlinx.coroutines.runBlocking
import mozilla.components.concept.storage.BookmarkNode
import mozilla.components.feature.tab.collections.TabCollection
import mozilla.components.feature.top.sites.TopSite
import mozilla.components.service.fxa.manager.FxaAccountManager
import mozilla.components.service.pocket.PocketRecommendedStory
import org.junit.Assert.assertEquals
import org.junit.Assert.assertFalse
import org.junit.Assert.assertSame
import org.junit.Assert.assertTrue
import org.junit.Before
import org.junit.Test
import org.mozilla.fenix.browser.browsingmode.BrowsingMode
import org.mozilla.fenix.browser.browsingmode.BrowsingModeManager
import org.mozilla.fenix.ext.components
import org.mozilla.fenix.ext.getFilteredStories
import org.mozilla.fenix.historymetadata.HistoryMetadataGroup
import org.mozilla.fenix.home.recenttabs.RecentTab
import org.mozilla.fenix.home.pocket.POCKET_STORIES_TO_SHOW_COUNT
import org.mozilla.fenix.home.pocket.PocketRecommendedStoriesCategory
import org.mozilla.fenix.home.pocket.PocketRecommendedStoriesSelectedCategory
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
        val recentTabs: List<RecentTab> = listOf(mockk(), mockk())
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
    fun `Test disbanding search group in HomeFragmentStore`() = runBlocking {
        val g1 = HistoryMetadataGroup(title = "test One")
        val g2 = HistoryMetadataGroup(title = "test two")
        val historyMetadata: List<HistoryMetadataGroup> = listOf(g1, g2)
        homeFragmentStore.dispatch(HomeFragmentAction.HistoryMetadataChange(historyMetadata)).join()
        assertEquals(historyMetadata, homeFragmentStore.state.historyMetadata)

        homeFragmentStore.dispatch(HomeFragmentAction.DisbandSearchGroupAction("Test one")).join()
        assertEquals(listOf(g2), homeFragmentStore.state.historyMetadata)
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
            val recentTabs: List<RecentTab> = listOf(mockk(), mockk())
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

    @Test
    fun `Test selecting a Pocket recommendations category`() = runBlocking {
        val otherStoriesCategory = PocketRecommendedStoriesCategory("other")
        val anotherStoriesCategory = PocketRecommendedStoriesCategory("another")
        val filteredStories = listOf(mockk<PocketRecommendedStory>())
        homeFragmentStore = HomeFragmentStore(
            HomeFragmentState(
                pocketStoriesCategories = listOf(otherStoriesCategory, anotherStoriesCategory),
                pocketStoriesCategoriesSelections = listOf(
                    PocketRecommendedStoriesSelectedCategory(otherStoriesCategory.name),
                )
            )
        )

        mockkStatic("org.mozilla.fenix.ext.HomeFragmentStateKt") {
            every { any<HomeFragmentState>().getFilteredStories(any()) } returns filteredStories

            homeFragmentStore.dispatch(HomeFragmentAction.SelectPocketStoriesCategory("another")).join()

            verify { any<HomeFragmentState>().getFilteredStories(POCKET_STORIES_TO_SHOW_COUNT) }
        }

        val selectedCategories = homeFragmentStore.state.pocketStoriesCategoriesSelections
        assertEquals(2, selectedCategories.size)
        assertTrue(otherStoriesCategory.name === selectedCategories[0].name)
        assertSame(filteredStories, homeFragmentStore.state.pocketStories)
    }

    @Test
    fun `Test deselecting a Pocket recommendations category`() = runBlocking {
        val otherStoriesCategory = PocketRecommendedStoriesCategory("other")
        val anotherStoriesCategory = PocketRecommendedStoriesCategory("another")
        val filteredStories = listOf(mockk<PocketRecommendedStory>())
        homeFragmentStore = HomeFragmentStore(
            HomeFragmentState(
                pocketStoriesCategories = listOf(otherStoriesCategory, anotherStoriesCategory),
                pocketStoriesCategoriesSelections = listOf(
                    PocketRecommendedStoriesSelectedCategory(otherStoriesCategory.name),
                    PocketRecommendedStoriesSelectedCategory(anotherStoriesCategory.name)
                )
            )
        )

        mockkStatic("org.mozilla.fenix.ext.HomeFragmentStateKt") {
            every { any<HomeFragmentState>().getFilteredStories(any()) } returns filteredStories

            homeFragmentStore.dispatch(HomeFragmentAction.DeselectPocketStoriesCategory("other")).join()

            verify { any<HomeFragmentState>().getFilteredStories(POCKET_STORIES_TO_SHOW_COUNT) }
        }

        val selectedCategories = homeFragmentStore.state.pocketStoriesCategoriesSelections
        assertEquals(1, selectedCategories.size)
        assertTrue(anotherStoriesCategory.name === selectedCategories[0].name)
        assertSame(filteredStories, homeFragmentStore.state.pocketStories)
    }

    @Test
    fun `Test updating the list of Pocket recommended stories`() = runBlocking {
        val story1 = PocketRecommendedStory("title1", "url", "imageUrl", "publisher", "category", 1, 1)
        val story2 = story1.copy("title2")
        homeFragmentStore = HomeFragmentStore(HomeFragmentState())

        homeFragmentStore.dispatch(HomeFragmentAction.PocketStoriesChange(listOf(story1, story2)))
            .join()
        assertTrue(homeFragmentStore.state.pocketStories.containsAll(listOf(story1, story2)))

        val updatedStories = listOf(story2.copy("title3"))
        homeFragmentStore.dispatch(HomeFragmentAction.PocketStoriesChange(updatedStories)).join()
        assertTrue(updatedStories.containsAll(homeFragmentStore.state.pocketStories))
    }

    @Test
    fun `Test updating the list of Pocket recommendations categories`() = runBlocking {
        val otherStoriesCategory = PocketRecommendedStoriesCategory("other")
        val anotherStoriesCategory = PocketRecommendedStoriesCategory("another")
        homeFragmentStore = HomeFragmentStore(HomeFragmentState())

        mockkStatic("org.mozilla.fenix.ext.HomeFragmentStateKt") {
            val firstFilteredStories = listOf(mockk<PocketRecommendedStory>())
            every { any<HomeFragmentState>().getFilteredStories(any()) } returns firstFilteredStories

            homeFragmentStore.dispatch(
                HomeFragmentAction.PocketStoriesCategoriesChange(listOf(otherStoriesCategory, anotherStoriesCategory))
            ).join()
            verify { any<HomeFragmentState>().getFilteredStories(POCKET_STORIES_TO_SHOW_COUNT) }
            assertTrue(
                homeFragmentStore.state.pocketStoriesCategories.containsAll(
                    listOf(otherStoriesCategory, anotherStoriesCategory)
                )
            )
            assertSame(firstFilteredStories, homeFragmentStore.state.pocketStories)

            val updatedCategories = listOf(PocketRecommendedStoriesCategory("yetAnother"))
            val secondFilteredStories = listOf(mockk<PocketRecommendedStory>())
            every { any<HomeFragmentState>().getFilteredStories(any()) } returns secondFilteredStories
            homeFragmentStore.dispatch(
                HomeFragmentAction.PocketStoriesCategoriesChange(
                    updatedCategories
                )
            ).join()
            verify(exactly = 2) { any<HomeFragmentState>().getFilteredStories(POCKET_STORIES_TO_SHOW_COUNT) }
            assertTrue(updatedCategories.containsAll(homeFragmentStore.state.pocketStoriesCategories))
            assertSame(secondFilteredStories, homeFragmentStore.state.pocketStories)
        }
    }

    @Test
    fun `Test updating the list of selected Pocket recommendations categories`() = runBlocking {
        val otherStoriesCategory = PocketRecommendedStoriesCategory("other")
        val anotherStoriesCategory = PocketRecommendedStoriesCategory("another")
        val selectedCategory = PocketRecommendedStoriesSelectedCategory("selected")
        homeFragmentStore = HomeFragmentStore(HomeFragmentState())

        mockkStatic("org.mozilla.fenix.ext.HomeFragmentStateKt") {
            val firstFilteredStories = listOf(mockk<PocketRecommendedStory>())
            every { any<HomeFragmentState>().getFilteredStories(any()) } returns firstFilteredStories

            homeFragmentStore.dispatch(
                HomeFragmentAction.PocketStoriesCategoriesSelectionsChange(
                    storiesCategories = listOf(otherStoriesCategory, anotherStoriesCategory),
                    categoriesSelected = listOf(selectedCategory)
                )
            ).join()
            verify { any<HomeFragmentState>().getFilteredStories(POCKET_STORIES_TO_SHOW_COUNT) }
            assertTrue(
                homeFragmentStore.state.pocketStoriesCategories.containsAll(
                    listOf(otherStoriesCategory, anotherStoriesCategory)
                )
            )
            assertTrue(
                homeFragmentStore.state.pocketStoriesCategoriesSelections.containsAll(listOf(selectedCategory))
            )
            assertSame(firstFilteredStories, homeFragmentStore.state.pocketStories)
        }
    }
}
