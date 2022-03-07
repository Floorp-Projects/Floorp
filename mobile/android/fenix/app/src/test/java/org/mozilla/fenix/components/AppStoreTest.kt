/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

package org.mozilla.fenix.components

import android.content.Context
import io.mockk.every
import io.mockk.mockk
import io.mockk.mockkStatic
import io.mockk.verify
import kotlinx.coroutines.runBlocking
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
import org.mozilla.fenix.components.appstate.AppAction
import org.mozilla.fenix.components.appstate.AppState
import org.mozilla.fenix.components.appstate.filterOut
import org.mozilla.fenix.ext.components
import org.mozilla.fenix.ext.getFilteredStories
import org.mozilla.fenix.home.CurrentMode
import org.mozilla.fenix.home.Mode
import org.mozilla.fenix.home.pocket.POCKET_STORIES_TO_SHOW_COUNT
import org.mozilla.fenix.home.pocket.PocketRecommendedStoriesCategory
import org.mozilla.fenix.home.pocket.PocketRecommendedStoriesSelectedCategory
import org.mozilla.fenix.home.recentbookmarks.RecentBookmark
import org.mozilla.fenix.home.recenttabs.RecentTab
import org.mozilla.fenix.home.recentvisits.RecentlyVisitedItem
import org.mozilla.fenix.home.recentvisits.RecentlyVisitedItem.RecentHistoryGroup
import org.mozilla.fenix.home.recentvisits.RecentlyVisitedItem.RecentHistoryHighlight
import org.mozilla.fenix.onboarding.FenixOnboarding

class AppStoreTest {
    private lateinit var context: Context
    private lateinit var accountManager: FxaAccountManager
    private lateinit var onboarding: FenixOnboarding
    private lateinit var browsingModeManager: BrowsingModeManager
    private lateinit var currentMode: CurrentMode
    private lateinit var appState: AppState
    private lateinit var appStore: AppStore

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

        appState = AppState(
            collections = emptyList(),
            expandedCollections = emptySet(),
            mode = currentMode.getCurrentMode(),
            topSites = emptyList(),
            showCollectionPlaceholder = true,
            showSetAsDefaultBrowserCard = true,
            recentTabs = emptyList()
        )

        appStore = AppStore(appState)
    }

    @Test
    fun `Test toggling the mode in AppStore`() = runBlocking {
        // Verify that the default mode and tab states of the HomeFragment are correct.
        assertEquals(Mode.Normal, appStore.state.mode)

        // Change the AppStore to Private mode.
        appStore.dispatch(AppAction.ModeChange(Mode.Private)).join()
        assertEquals(Mode.Private, appStore.state.mode)

        // Change the AppStore back to Normal mode.
        appStore.dispatch(AppAction.ModeChange(Mode.Normal)).join()
        assertEquals(Mode.Normal, appStore.state.mode)
    }

    @Test
    fun `Test changing the collections in AppStore`() = runBlocking {
        assertEquals(0, appStore.state.collections.size)

        // Add 2 TabCollections to the AppStore.
        val tabCollections: List<TabCollection> = listOf(mockk(), mockk())
        appStore.dispatch(AppAction.CollectionsChange(tabCollections)).join()

        assertEquals(tabCollections, appStore.state.collections)
    }

    @Test
    fun `Test changing the top sites in AppStore`() = runBlocking {
        assertEquals(0, appStore.state.topSites.size)

        // Add 2 TopSites to the AppStore.
        val topSites: List<TopSite> = listOf(mockk(), mockk())
        appStore.dispatch(AppAction.TopSitesChange(topSites)).join()

        assertEquals(topSites, appStore.state.topSites)
    }

    @Test
    fun `Test changing the recent tabs in AppStore`() = runBlocking {
        val group1 = RecentHistoryGroup(title = "title1")
        val group2 = RecentHistoryGroup(title = "title2")
        val group3 = RecentHistoryGroup(title = "title3")
        val highlight = RecentHistoryHighlight(title = group2.title, "")
        appStore = AppStore(
            AppState(
                recentHistory = listOf(group1, group2, group3, highlight)
            )
        )
        assertEquals(0, appStore.state.recentTabs.size)

        // Add 2 RecentTabs to the AppStore
        // A new SearchGroup already shown in history should hide the HistoryGroup.
        val recentTab1: RecentTab.Tab = mockk()
        val recentTab2 = RecentTab.SearchGroup(group2.title, "tabId", "url", null, 2)
        val recentTabs: List<RecentTab> = listOf(recentTab1, recentTab2)
        appStore.dispatch(AppAction.RecentTabsChange(recentTabs)).join()

        assertEquals(recentTabs, appStore.state.recentTabs)
        assertEquals(listOf(group1, group3, highlight), appStore.state.recentHistory)
    }

    @Test
    fun `Test changing the history metadata in AppStore`() = runBlocking {
        assertEquals(0, appStore.state.recentHistory.size)

        val historyMetadata: List<RecentHistoryGroup> = listOf(mockk(), mockk())
        appStore.dispatch(AppAction.RecentHistoryChange(historyMetadata)).join()

        assertEquals(historyMetadata, appStore.state.recentHistory)
    }

    @Test
    fun `Test removing a history highlight from AppStore`() = runBlocking {
        val g1 = RecentHistoryGroup(title = "group One")
        val g2 = RecentHistoryGroup(title = "grup two")
        val h1 = RecentHistoryHighlight(title = "highlight One", url = "url1")
        val h2 = RecentHistoryHighlight(title = "highlight two", url = "url2")
        val recentHistoryState = AppState(
            recentHistory = listOf(g1, g2, h1, h2)
        )
        appStore = AppStore(recentHistoryState)

        appStore.dispatch(AppAction.RemoveRecentHistoryHighlight("invalid")).join()
        assertEquals(recentHistoryState, appStore.state)

        appStore.dispatch(AppAction.RemoveRecentHistoryHighlight(h1.title)).join()
        assertEquals(recentHistoryState, appStore.state)

        appStore.dispatch(AppAction.RemoveRecentHistoryHighlight(h1.url)).join()
        assertEquals(
            recentHistoryState.copy(recentHistory = listOf(g1, g2, h2)),
            appStore.state
        )
    }

    @Test
    fun `Test disbanding search group in AppStore`() = runBlocking {
        val g1 = RecentHistoryGroup(title = "test One")
        val g2 = RecentHistoryGroup(title = "test two")
        val h1 = RecentHistoryHighlight(title = "highlight One", url = "url1")
        val h2 = RecentHistoryHighlight(title = "highlight two", url = "url2")
        val recentHistory: List<RecentlyVisitedItem> = listOf(g1, g2, h1, h2)
        appStore.dispatch(AppAction.RecentHistoryChange(recentHistory)).join()
        assertEquals(recentHistory, appStore.state.recentHistory)

        appStore.dispatch(AppAction.DisbandSearchGroupAction("Test one")).join()
        assertEquals(listOf(g2, h1, h2), appStore.state.recentHistory)
    }

    @Test
    fun `Test changing hiding collections placeholder`() = runBlocking {
        assertTrue(appStore.state.showCollectionPlaceholder)

        appStore.dispatch(AppAction.RemoveCollectionsPlaceholder).join()

        assertFalse(appStore.state.showCollectionPlaceholder)
    }

    @Test
    fun `Test changing the expanded collections in AppStore`() = runBlocking {
        val collection: TabCollection = mockk<TabCollection>().apply {
            every { id } returns 0
        }

        // Expand the given collection.
        appStore.dispatch(AppAction.CollectionsChange(listOf(collection))).join()
        appStore.dispatch(AppAction.CollectionExpanded(collection, true)).join()

        assertTrue(appStore.state.expandedCollections.contains(collection.id))
        assertEquals(1, appStore.state.expandedCollections.size)
    }

    @Test
    fun `Test changing the collections, mode, recent tabs and bookmarks, history metadata and top sites in the AppStore`() =
        runBlocking {
            // Verify that the default state of the HomeFragment is correct.
            assertEquals(0, appStore.state.collections.size)
            assertEquals(0, appStore.state.topSites.size)
            assertEquals(0, appStore.state.recentTabs.size)
            assertEquals(0, appStore.state.recentBookmarks.size)
            assertEquals(0, appStore.state.recentHistory.size)
            assertEquals(Mode.Normal, appStore.state.mode)

            val recentGroup = RecentTab.SearchGroup("testSearchTerm", "id", "url", null, 3)
            val collections: List<TabCollection> = listOf(mockk())
            val topSites: List<TopSite> = listOf(mockk(), mockk())
            val recentTabs: List<RecentTab> = listOf(mockk(), recentGroup, mockk())
            val recentBookmarks: List<RecentBookmark> = listOf(mockk(), mockk())
            val group1 = RecentHistoryGroup(title = "test One")
            val group2 = RecentHistoryGroup(title = recentGroup.searchTerm.lowercase())
            val group3 = RecentHistoryGroup(title = "test two")
            val highlight = RecentHistoryHighlight(group2.title, "")
            val recentHistory: List<RecentlyVisitedItem> = listOf(group1, group2, group3, highlight)

            appStore.dispatch(
                AppAction.Change(
                    collections = collections,
                    mode = Mode.Private,
                    topSites = topSites,
                    showCollectionPlaceholder = true,
                    recentTabs = recentTabs,
                    recentBookmarks = recentBookmarks,
                    recentHistory = recentHistory
                )
            ).join()

            assertEquals(collections, appStore.state.collections)
            assertEquals(topSites, appStore.state.topSites)
            assertEquals(recentTabs, appStore.state.recentTabs)
            assertEquals(recentBookmarks, appStore.state.recentBookmarks)
            assertEquals(listOf(group1, group3, highlight), appStore.state.recentHistory)
            assertEquals(Mode.Private, appStore.state.mode)
        }

    @Test
    fun `Test selecting a Pocket recommendations category`() = runBlocking {
        val otherStoriesCategory = PocketRecommendedStoriesCategory("other")
        val anotherStoriesCategory = PocketRecommendedStoriesCategory("another")
        val filteredStories = listOf(mockk<PocketRecommendedStory>())
        appStore = AppStore(
            AppState(
                pocketStoriesCategories = listOf(otherStoriesCategory, anotherStoriesCategory),
                pocketStoriesCategoriesSelections = listOf(
                    PocketRecommendedStoriesSelectedCategory(otherStoriesCategory.name),
                )
            )
        )

        mockkStatic("org.mozilla.fenix.ext.AppStateKt") {
            every { any<AppState>().getFilteredStories(any()) } returns filteredStories

            appStore.dispatch(AppAction.SelectPocketStoriesCategory("another")).join()

            verify { any<AppState>().getFilteredStories(POCKET_STORIES_TO_SHOW_COUNT) }
        }

        val selectedCategories = appStore.state.pocketStoriesCategoriesSelections
        assertEquals(2, selectedCategories.size)
        assertTrue(otherStoriesCategory.name === selectedCategories[0].name)
        assertSame(filteredStories, appStore.state.pocketStories)
    }

    @Test
    fun `Test deselecting a Pocket recommendations category`() = runBlocking {
        val otherStoriesCategory = PocketRecommendedStoriesCategory("other")
        val anotherStoriesCategory = PocketRecommendedStoriesCategory("another")
        val filteredStories = listOf(mockk<PocketRecommendedStory>())
        appStore = AppStore(
            AppState(
                pocketStoriesCategories = listOf(otherStoriesCategory, anotherStoriesCategory),
                pocketStoriesCategoriesSelections = listOf(
                    PocketRecommendedStoriesSelectedCategory(otherStoriesCategory.name),
                    PocketRecommendedStoriesSelectedCategory(anotherStoriesCategory.name)
                )
            )
        )

        mockkStatic("org.mozilla.fenix.ext.AppStateKt") {
            every { any<AppState>().getFilteredStories(any()) } returns filteredStories

            appStore.dispatch(AppAction.DeselectPocketStoriesCategory("other")).join()

            verify { any<AppState>().getFilteredStories(POCKET_STORIES_TO_SHOW_COUNT) }
        }

        val selectedCategories = appStore.state.pocketStoriesCategoriesSelections
        assertEquals(1, selectedCategories.size)
        assertTrue(anotherStoriesCategory.name === selectedCategories[0].name)
        assertSame(filteredStories, appStore.state.pocketStories)
    }

    @Test
    fun `Test updating the list of Pocket recommended stories`() = runBlocking {
        val story1 = PocketRecommendedStory("title1", "url", "imageUrl", "publisher", "category", 1, 1)
        val story2 = story1.copy("title2")
        appStore = AppStore(AppState())

        appStore.dispatch(AppAction.PocketStoriesChange(listOf(story1, story2)))
            .join()
        assertTrue(appStore.state.pocketStories.containsAll(listOf(story1, story2)))

        val updatedStories = listOf(story2.copy("title3"))
        appStore.dispatch(AppAction.PocketStoriesChange(updatedStories)).join()
        assertTrue(updatedStories.containsAll(appStore.state.pocketStories))
    }

    @Test
    fun `Test updating the list of Pocket recommendations categories`() = runBlocking {
        val otherStoriesCategory = PocketRecommendedStoriesCategory("other")
        val anotherStoriesCategory = PocketRecommendedStoriesCategory("another")
        appStore = AppStore(AppState())

        mockkStatic("org.mozilla.fenix.ext.AppStateKt") {
            val firstFilteredStories = listOf(mockk<PocketRecommendedStory>())
            every { any<AppState>().getFilteredStories(any()) } returns firstFilteredStories

            appStore.dispatch(
                AppAction.PocketStoriesCategoriesChange(listOf(otherStoriesCategory, anotherStoriesCategory))
            ).join()
            verify { any<AppState>().getFilteredStories(POCKET_STORIES_TO_SHOW_COUNT) }
            assertTrue(
                appStore.state.pocketStoriesCategories.containsAll(
                    listOf(otherStoriesCategory, anotherStoriesCategory)
                )
            )
            assertSame(firstFilteredStories, appStore.state.pocketStories)

            val updatedCategories = listOf(PocketRecommendedStoriesCategory("yetAnother"))
            val secondFilteredStories = listOf(mockk<PocketRecommendedStory>())
            every { any<AppState>().getFilteredStories(any()) } returns secondFilteredStories
            appStore.dispatch(
                AppAction.PocketStoriesCategoriesChange(
                    updatedCategories
                )
            ).join()
            verify(exactly = 2) { any<AppState>().getFilteredStories(POCKET_STORIES_TO_SHOW_COUNT) }
            assertTrue(updatedCategories.containsAll(appStore.state.pocketStoriesCategories))
            assertSame(secondFilteredStories, appStore.state.pocketStories)
        }
    }

    @Test
    fun `Test updating the list of selected Pocket recommendations categories`() = runBlocking {
        val otherStoriesCategory = PocketRecommendedStoriesCategory("other")
        val anotherStoriesCategory = PocketRecommendedStoriesCategory("another")
        val selectedCategory = PocketRecommendedStoriesSelectedCategory("selected")
        appStore = AppStore(AppState())

        mockkStatic("org.mozilla.fenix.ext.AppStateKt") {
            val firstFilteredStories = listOf(mockk<PocketRecommendedStory>())
            every { any<AppState>().getFilteredStories(any()) } returns firstFilteredStories

            appStore.dispatch(
                AppAction.PocketStoriesCategoriesSelectionsChange(
                    storiesCategories = listOf(otherStoriesCategory, anotherStoriesCategory),
                    categoriesSelected = listOf(selectedCategory)
                )
            ).join()
            verify { any<AppState>().getFilteredStories(POCKET_STORIES_TO_SHOW_COUNT) }
            assertTrue(
                appStore.state.pocketStoriesCategories.containsAll(
                    listOf(otherStoriesCategory, anotherStoriesCategory)
                )
            )
            assertTrue(
                appStore.state.pocketStoriesCategoriesSelections.containsAll(listOf(selectedCategory))
            )
            assertSame(firstFilteredStories, appStore.state.pocketStories)
        }
    }

    @Test
    fun `Test filtering out search groups`() {
        val group1 = RecentHistoryGroup("title1")
        val group2 = RecentHistoryGroup("title2")
        val group3 = RecentHistoryGroup("title3")
        val highLight1 = RecentHistoryHighlight("title1", "")
        val highLight2 = RecentHistoryHighlight("title2", "")
        val highLight3 = RecentHistoryHighlight("title3", "")
        val recentHistory = listOf(group1, highLight1, group2, highLight2, group3, highLight3)

        assertEquals(recentHistory, recentHistory.filterOut(null))
        assertEquals(recentHistory, recentHistory.filterOut(""))
        assertEquals(recentHistory, recentHistory.filterOut(" "))
        assertEquals(recentHistory - group2, recentHistory.filterOut("Title2"))
        assertEquals(recentHistory - group3, recentHistory.filterOut("title3"))
    }
}
