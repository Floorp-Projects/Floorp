/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

package org.mozilla.fenix.components

import android.content.Context
import io.mockk.every
import io.mockk.mockk
import io.mockk.mockkStatic
import io.mockk.verify
import kotlinx.coroutines.test.runTest
import mozilla.components.concept.sync.DeviceType
import mozilla.components.feature.tab.collections.TabCollection
import mozilla.components.feature.top.sites.TopSite
import mozilla.components.service.fxa.manager.FxaAccountManager
import mozilla.components.service.nimbus.messaging.Message
import mozilla.components.service.nimbus.messaging.MessageData
import mozilla.components.service.pocket.PocketStory
import mozilla.components.service.pocket.PocketStory.PocketRecommendedStory
import mozilla.components.service.pocket.PocketStory.PocketSponsoredStory
import mozilla.components.service.pocket.PocketStory.PocketSponsoredStoryCaps
import org.junit.Assert.assertEquals
import org.junit.Assert.assertFalse
import org.junit.Assert.assertSame
import org.junit.Assert.assertTrue
import org.junit.Before
import org.junit.Test
import org.mozilla.fenix.browser.browsingmode.BrowsingMode
import org.mozilla.fenix.components.appstate.AppAction
import org.mozilla.fenix.components.appstate.AppAction.MessagingAction.UpdateMessageToShow
import org.mozilla.fenix.components.appstate.AppState
import org.mozilla.fenix.components.appstate.filterOut
import org.mozilla.fenix.ext.components
import org.mozilla.fenix.ext.getFilteredStories
import org.mozilla.fenix.home.pocket.PocketRecommendedStoriesCategory
import org.mozilla.fenix.home.pocket.PocketRecommendedStoriesSelectedCategory
import org.mozilla.fenix.home.recentbookmarks.RecentBookmark
import org.mozilla.fenix.home.recentsyncedtabs.RecentSyncedTab
import org.mozilla.fenix.home.recentsyncedtabs.RecentSyncedTabState
import org.mozilla.fenix.home.recenttabs.RecentTab
import org.mozilla.fenix.home.recentvisits.RecentlyVisitedItem
import org.mozilla.fenix.home.recentvisits.RecentlyVisitedItem.RecentHistoryGroup
import org.mozilla.fenix.home.recentvisits.RecentlyVisitedItem.RecentHistoryHighlight
import org.mozilla.fenix.messaging.FenixMessageSurfaceId
import org.mozilla.fenix.onboarding.FenixOnboarding

class AppStoreTest {
    private lateinit var context: Context
    private lateinit var accountManager: FxaAccountManager
    private lateinit var onboarding: FenixOnboarding
    private lateinit var appState: AppState
    private lateinit var appStore: AppStore
    private lateinit var recentSyncedTabsList: List<RecentSyncedTab>

    @Before
    fun setup() {
        context = mockk(relaxed = true)
        accountManager = mockk(relaxed = true)
        onboarding = mockk(relaxed = true)
        recentSyncedTabsList = listOf(
            RecentSyncedTab(
                deviceDisplayName = "",
                deviceType = mockk(relaxed = true),
                title = "",
                url = "",
                previewImageUrl = null,
            ),
        )

        every { context.components.backgroundServices.accountManager } returns accountManager
        every { onboarding.userHasBeenOnboarded() } returns true

        appState = AppState(
            collections = emptyList(),
            expandedCollections = emptySet(),
            topSites = emptyList(),
            showCollectionPlaceholder = true,
            recentTabs = emptyList(),
            recentSyncedTabState = RecentSyncedTabState.Success(recentSyncedTabsList),
        )

        appStore = AppStore(appState)
    }

    @Test
    fun `Test toggling the mode in AppStore`() = runTest {
        // Verify that the default mode and tab states of the HomeFragment are correct.
        assertEquals(BrowsingMode.Normal, appStore.state.mode)

        // Change the AppStore to Private mode.
        appStore.dispatch(AppAction.ModeChange(BrowsingMode.Private)).join()
        assertEquals(BrowsingMode.Private, appStore.state.mode)

        // Change the AppStore back to Normal mode.
        appStore.dispatch(AppAction.ModeChange(BrowsingMode.Normal)).join()
        assertEquals(BrowsingMode.Normal, appStore.state.mode)
    }

    @Test
    fun `GIVEN a new value for messageToShow WHEN NimbusMessageChange is called THEN update the current value`() =
        runTest {
            assertTrue(appStore.state.messaging.messageToShow.isEmpty())

            val message = Message(
                "message",
                MessageData(surface = FenixMessageSurfaceId.HOMESCREEN),
                "action",
                mockk(),
                emptyList(),
                mockk(),
            )
            appStore.dispatch(UpdateMessageToShow(message)).join()

            assertFalse(appStore.state.messaging.messageToShow.isEmpty())
        }

    @Test
    fun `Test changing the collections in AppStore`() = runTest {
        assertEquals(0, appStore.state.collections.size)

        // Add 2 TabCollections to the AppStore.
        val tabCollections: List<TabCollection> = listOf(mockk(), mockk())
        appStore.dispatch(AppAction.CollectionsChange(tabCollections)).join()

        assertEquals(tabCollections, appStore.state.collections)
    }

    @Test
    fun `Test changing the top sites in AppStore`() = runTest {
        assertEquals(0, appStore.state.topSites.size)

        // Add 2 TopSites to the AppStore.
        val topSites: List<TopSite> = listOf(mockk(), mockk())
        appStore.dispatch(AppAction.TopSitesChange(topSites)).join()

        assertEquals(topSites, appStore.state.topSites)
    }

    @Test
    fun `Test changing the recent tabs in AppStore`() = runTest {
        val group1 = RecentHistoryGroup(title = "title1")
        val group2 = RecentHistoryGroup(title = "title2")
        val group3 = RecentHistoryGroup(title = "title3")
        val highlight = RecentHistoryHighlight(title = group2.title, "")
        appStore = AppStore(
            AppState(
                recentHistory = listOf(group1, group2, group3, highlight),
            ),
        )
        assertEquals(0, appStore.state.recentTabs.size)

        // Add 2 RecentTabs to the AppStore
        val recentTab1: RecentTab.Tab = mockk()
        val recentTabs: List<RecentTab> = listOf(recentTab1)
        appStore.dispatch(AppAction.RecentTabsChange(recentTabs)).join()

        assertEquals(recentTabs, appStore.state.recentTabs)
        assertEquals(listOf(group1, group2, group3, highlight), appStore.state.recentHistory)
    }

    @Test
    fun `GIVEN initial state WHEN recent synced tab state is changed THEN state updated`() = runTest {
        appStore = AppStore(
            AppState(
                recentSyncedTabState = RecentSyncedTabState.None,
            ),
        )

        val loading = RecentSyncedTabState.Loading
        appStore.dispatch(AppAction.RecentSyncedTabStateChange(loading)).join()
        assertEquals(loading, appStore.state.recentSyncedTabState)

        val recentSyncedTabs = listOf(RecentSyncedTab("device name", DeviceType.DESKTOP, "title", "url", null))
        val success = RecentSyncedTabState.Success(recentSyncedTabs)
        appStore.dispatch(AppAction.RecentSyncedTabStateChange(success)).join()
        assertEquals(success, appStore.state.recentSyncedTabState)
        assertEquals(recentSyncedTabs, (appStore.state.recentSyncedTabState as RecentSyncedTabState.Success).tabs)
    }

    @Test
    fun `Test changing the history metadata in AppStore`() = runTest {
        assertEquals(0, appStore.state.recentHistory.size)

        val historyMetadata: List<RecentHistoryGroup> = listOf(mockk(), mockk())
        appStore.dispatch(AppAction.RecentHistoryChange(historyMetadata)).join()

        assertEquals(historyMetadata, appStore.state.recentHistory)
    }

    @Test
    fun `Test removing a history highlight from AppStore`() = runTest {
        val g1 = RecentHistoryGroup(title = "group One")
        val g2 = RecentHistoryGroup(title = "grup two")
        val h1 = RecentHistoryHighlight(title = "highlight One", url = "url1")
        val h2 = RecentHistoryHighlight(title = "highlight two", url = "url2")
        val recentHistoryState = AppState(
            recentHistory = listOf(g1, g2, h1, h2),
        )
        appStore = AppStore(recentHistoryState)

        appStore.dispatch(AppAction.RemoveRecentHistoryHighlight("invalid")).join()
        assertEquals(recentHistoryState, appStore.state)

        appStore.dispatch(AppAction.RemoveRecentHistoryHighlight(h1.title)).join()
        assertEquals(recentHistoryState, appStore.state)

        appStore.dispatch(AppAction.RemoveRecentHistoryHighlight(h1.url)).join()
        assertEquals(
            recentHistoryState.copy(recentHistory = listOf(g1, g2, h2)),
            appStore.state,
        )
    }

    @Test
    fun `Test disbanding search group in AppStore`() = runTest {
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
    fun `Test changing hiding collections placeholder`() = runTest {
        assertTrue(appStore.state.showCollectionPlaceholder)

        appStore.dispatch(AppAction.RemoveCollectionsPlaceholder).join()

        assertFalse(appStore.state.showCollectionPlaceholder)
    }

    @Test
    fun `Test changing the expanded collections in AppStore`() = runTest {
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
    fun `Test changing the collections, mode, recent tabs and bookmarks, history metadata, top sites and recent synced tabs in the AppStore`() =
        runTest {
            // Verify that the default state of the HomeFragment is correct.
            assertEquals(0, appStore.state.collections.size)
            assertEquals(0, appStore.state.topSites.size)
            assertEquals(0, appStore.state.recentTabs.size)
            assertEquals(0, appStore.state.recentBookmarks.size)
            assertEquals(0, appStore.state.recentHistory.size)
            assertEquals(BrowsingMode.Normal, appStore.state.mode)
            assertEquals(
                RecentSyncedTabState.Success(recentSyncedTabsList),
                appStore.state.recentSyncedTabState,
            )

            val collections: List<TabCollection> = listOf(mockk())
            val topSites: List<TopSite> = listOf(mockk(), mockk())
            val recentTabs: List<RecentTab> = listOf(mockk(), mockk())
            val recentBookmarks: List<RecentBookmark> = listOf(mockk(), mockk())
            val group1 = RecentHistoryGroup(title = "test One")
            val group2 = RecentHistoryGroup(title = "testSearchTerm")
            val group3 = RecentHistoryGroup(title = "test two")
            val highlight = RecentHistoryHighlight(group2.title, "")
            val recentHistory: List<RecentlyVisitedItem> = listOf(group1, group2, group3, highlight)
            val recentSyncedTab = RecentSyncedTab(
                deviceDisplayName = "device1",
                deviceType = mockk(relaxed = true),
                title = "1",
                url = "",
                previewImageUrl = null,
            )
            val recentSyncedTabState: RecentSyncedTabState =
                RecentSyncedTabState.Success(recentSyncedTabsList + recentSyncedTab)

            appStore.dispatch(
                AppAction.Change(
                    collections = collections,
                    mode = BrowsingMode.Private,
                    topSites = topSites,
                    showCollectionPlaceholder = true,
                    recentTabs = recentTabs,
                    recentBookmarks = recentBookmarks,
                    recentHistory = recentHistory,
                    recentSyncedTabState = recentSyncedTabState,
                ),
            ).join()

            assertEquals(collections, appStore.state.collections)
            assertEquals(topSites, appStore.state.topSites)
            assertEquals(recentTabs, appStore.state.recentTabs)
            assertEquals(recentBookmarks, appStore.state.recentBookmarks)
            assertEquals(listOf(group1, group2, group3, highlight), appStore.state.recentHistory)
            assertEquals(BrowsingMode.Private, appStore.state.mode)
            assertEquals(
                recentSyncedTabState,
                appStore.state.recentSyncedTabState,
            )
        }

    @Test
    fun `Test selecting a Pocket recommendations category`() = runTest {
        val otherStoriesCategory = PocketRecommendedStoriesCategory("other")
        val anotherStoriesCategory = PocketRecommendedStoriesCategory("another")
        val filteredStories = listOf(mockk<PocketStory>())
        appStore = AppStore(
            AppState(
                pocketStoriesCategories = listOf(otherStoriesCategory, anotherStoriesCategory),
                pocketStoriesCategoriesSelections = listOf(
                    PocketRecommendedStoriesSelectedCategory(otherStoriesCategory.name),
                ),
            ),
        )

        mockkStatic("org.mozilla.fenix.ext.AppStateKt") {
            every { any<AppState>().getFilteredStories() } returns filteredStories

            appStore.dispatch(AppAction.SelectPocketStoriesCategory("another")).join()

            verify { any<AppState>().getFilteredStories() }
        }

        val selectedCategories = appStore.state.pocketStoriesCategoriesSelections
        assertEquals(2, selectedCategories.size)
        assertTrue(otherStoriesCategory.name === selectedCategories[0].name)
        assertSame(filteredStories, appStore.state.pocketStories)
    }

    @Test
    fun `Test deselecting a Pocket recommendations category`() = runTest {
        val otherStoriesCategory = PocketRecommendedStoriesCategory("other")
        val anotherStoriesCategory = PocketRecommendedStoriesCategory("another")
        val filteredStories = listOf(mockk<PocketStory>())
        appStore = AppStore(
            AppState(
                pocketStoriesCategories = listOf(otherStoriesCategory, anotherStoriesCategory),
                pocketStoriesCategoriesSelections = listOf(
                    PocketRecommendedStoriesSelectedCategory(otherStoriesCategory.name),
                    PocketRecommendedStoriesSelectedCategory(anotherStoriesCategory.name),
                ),
            ),
        )

        mockkStatic("org.mozilla.fenix.ext.AppStateKt") {
            every { any<AppState>().getFilteredStories() } returns filteredStories

            appStore.dispatch(AppAction.DeselectPocketStoriesCategory("other")).join()

            verify { any<AppState>().getFilteredStories() }
        }

        val selectedCategories = appStore.state.pocketStoriesCategoriesSelections
        assertEquals(1, selectedCategories.size)
        assertTrue(anotherStoriesCategory.name === selectedCategories[0].name)
        assertSame(filteredStories, appStore.state.pocketStories)
    }

    @Test
    fun `Test cleaning the list of Pocket stories`() = runTest {
        appStore = AppStore(
            AppState(
                pocketStoriesCategories = listOf(mockk()),
                pocketStoriesCategoriesSelections = listOf(mockk()),
                pocketStories = listOf(mockk()),
                pocketSponsoredStories = listOf(mockk()),
            ),
        )

        appStore.dispatch(AppAction.PocketStoriesClean)
            .join()

        assertTrue(appStore.state.pocketStoriesCategories.isEmpty())
        assertTrue(appStore.state.pocketStoriesCategoriesSelections.isEmpty())
        assertTrue(appStore.state.pocketStories.isEmpty())
        assertTrue(appStore.state.pocketSponsoredStories.isEmpty())
    }

    @Test
    fun `Test updating the list of Pocket sponsored stories also updates the list of stories to show`() = runTest {
        val story1 = PocketSponsoredStory(
            id = 3,
            title = "title",
            url = "url",
            imageUrl = "imageUrl",
            sponsor = "sponsor",
            shim = mockk(),
            priority = 33,
            caps = mockk(),
        )
        val story2 = story1.copy(imageUrl = "imageUrl2")

        appStore = AppStore(AppState())

        mockkStatic("org.mozilla.fenix.ext.AppStateKt") {
            val firstFilteredStories = listOf(mockk<PocketSponsoredStory>())
            every { any<AppState>().getFilteredStories() } returns firstFilteredStories
            appStore.dispatch(AppAction.PocketSponsoredStoriesChange(listOf(story1, story2))).join()
            assertTrue(appStore.state.pocketSponsoredStories.containsAll(listOf(story1, story2)))
            assertEquals(firstFilteredStories, appStore.state.pocketStories)

            val secondFilteredStories = firstFilteredStories + mockk<PocketRecommendedStory>()
            every { any<AppState>().getFilteredStories() } returns secondFilteredStories
            val updatedStories = listOf(story2.copy(title = "title3"))
            appStore.dispatch(AppAction.PocketSponsoredStoriesChange(updatedStories)).join()
            assertTrue(updatedStories.containsAll(appStore.state.pocketSponsoredStories))
            assertEquals(secondFilteredStories, appStore.state.pocketStories)
        }
    }

    @Test
    fun `Test updating sponsored Pocket stories after being shown to the user`() = runTest {
        val story1 = PocketSponsoredStory(
            id = 3,
            title = "title",
            url = "url",
            imageUrl = "imageUrl",
            sponsor = "sponsor",
            shim = mockk(),
            priority = 33,
            caps = PocketSponsoredStoryCaps(
                currentImpressions = listOf(1, 2),
                lifetimeCount = 11,
                flightCount = 2,
                flightPeriod = 11,
            ),
        )
        val story2 = story1.copy(id = 22)
        val story3 = story1.copy(id = 33)
        val story4 = story1.copy(id = 44)
        appStore = AppStore(
            AppState(
                pocketSponsoredStories = listOf(story1, story2, story3, story4),
            ),
        )

        appStore.dispatch(AppAction.PocketStoriesShown(listOf(story1, story3))).join()

        assertEquals(4, appStore.state.pocketSponsoredStories.size)
        assertEquals(3, appStore.state.pocketSponsoredStories[0].caps.currentImpressions.size)
        assertEquals(2, appStore.state.pocketSponsoredStories[1].caps.currentImpressions.size)
        assertEquals(3, appStore.state.pocketSponsoredStories[2].caps.currentImpressions.size)
        assertEquals(2, appStore.state.pocketSponsoredStories[3].caps.currentImpressions.size)
    }

    @Test
    fun `Test updating the list of Pocket recommendations categories`() = runTest {
        val otherStoriesCategory = PocketRecommendedStoriesCategory("other")
        val anotherStoriesCategory = PocketRecommendedStoriesCategory("another")
        appStore = AppStore(AppState())

        mockkStatic("org.mozilla.fenix.ext.AppStateKt") {
            val firstFilteredStories = listOf(mockk<PocketStory>())
            every { any<AppState>().getFilteredStories() } returns firstFilteredStories

            appStore.dispatch(
                AppAction.PocketStoriesCategoriesChange(listOf(otherStoriesCategory, anotherStoriesCategory)),
            ).join()
            verify { any<AppState>().getFilteredStories() }
            assertTrue(
                appStore.state.pocketStoriesCategories.containsAll(
                    listOf(otherStoriesCategory, anotherStoriesCategory),
                ),
            )
            assertSame(firstFilteredStories, appStore.state.pocketStories)

            val updatedCategories = listOf(PocketRecommendedStoriesCategory("yetAnother"))
            val secondFilteredStories = listOf(mockk<PocketStory>())
            every { any<AppState>().getFilteredStories() } returns secondFilteredStories
            appStore.dispatch(
                AppAction.PocketStoriesCategoriesChange(
                    updatedCategories,
                ),
            ).join()
            verify(exactly = 2) { any<AppState>().getFilteredStories() }
            assertTrue(updatedCategories.containsAll(appStore.state.pocketStoriesCategories))
            assertSame(secondFilteredStories, appStore.state.pocketStories)
        }
    }

    @Test
    fun `Test updating the list of selected Pocket recommendations categories`() = runTest {
        val otherStoriesCategory = PocketRecommendedStoriesCategory("other")
        val anotherStoriesCategory = PocketRecommendedStoriesCategory("another")
        val selectedCategory = PocketRecommendedStoriesSelectedCategory("selected")
        appStore = AppStore(AppState())

        mockkStatic("org.mozilla.fenix.ext.AppStateKt") {
            val firstFilteredStories = listOf(mockk<PocketStory>())
            every { any<AppState>().getFilteredStories() } returns firstFilteredStories

            appStore.dispatch(
                AppAction.PocketStoriesCategoriesSelectionsChange(
                    storiesCategories = listOf(otherStoriesCategory, anotherStoriesCategory),
                    categoriesSelected = listOf(selectedCategory),
                ),
            ).join()
            verify { any<AppState>().getFilteredStories() }
            assertTrue(
                appStore.state.pocketStoriesCategories.containsAll(
                    listOf(otherStoriesCategory, anotherStoriesCategory),
                ),
            )
            assertTrue(
                appStore.state.pocketStoriesCategoriesSelections.containsAll(listOf(selectedCategory)),
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
