/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

package org.mozilla.fenix.home

import io.mockk.every
import io.mockk.mockk
import io.mockk.verify
import mozilla.components.feature.tab.collections.Tab
import mozilla.components.feature.tab.collections.TabCollection
import mozilla.components.service.pocket.PocketStory
import org.junit.Before
import org.junit.Test
import org.mozilla.fenix.browser.browsingmode.BrowsingMode
import org.mozilla.fenix.components.appstate.AppState
import org.mozilla.fenix.home.pocket.PocketRecommendedStoriesCategory
import org.mozilla.fenix.home.pocket.PocketStoriesController
import org.mozilla.fenix.home.recentbookmarks.RecentBookmark
import org.mozilla.fenix.home.recentbookmarks.controller.RecentBookmarksController
import org.mozilla.fenix.home.recentsyncedtabs.RecentSyncedTab
import org.mozilla.fenix.home.recentsyncedtabs.controller.RecentSyncedTabController
import org.mozilla.fenix.home.recenttabs.controller.RecentTabController
import org.mozilla.fenix.home.recentvisits.controller.RecentVisitsController
import org.mozilla.fenix.home.sessioncontrol.DefaultSessionControlController
import org.mozilla.fenix.home.sessioncontrol.SessionControlInteractor
import org.mozilla.fenix.onboarding.controller.OnboardingController

class SessionControlInteractorTest {

    private val controller: DefaultSessionControlController = mockk(relaxed = true)
    private val recentTabController: RecentTabController = mockk(relaxed = true)
    private val recentSyncedTabController: RecentSyncedTabController = mockk(relaxed = true)
    private val recentBookmarksController: RecentBookmarksController = mockk(relaxed = true)
    private val pocketStoriesController: PocketStoriesController = mockk(relaxed = true)
    private val onboardingController: OnboardingController = mockk(relaxed = true)

    // Note: the recent visits tests are handled in [RecentVisitsInteractorTest] and [RecentVisitsControllerTest]
    private val recentVisitsController: RecentVisitsController = mockk(relaxed = true)

    private lateinit var interactor: SessionControlInteractor

    @Before
    fun setup() {
        interactor = SessionControlInteractor(
            controller,
            recentTabController,
            recentSyncedTabController,
            recentBookmarksController,
            recentVisitsController,
            pocketStoriesController,
            onboardingController,
        )
    }

    @Test
    fun onCollectionAddTabTapped() {
        val collection: TabCollection = mockk(relaxed = true)
        interactor.onCollectionAddTabTapped(collection)
        verify { controller.handleCollectionAddTabTapped(collection) }
    }

    @Test
    fun onCollectionOpenTabClicked() {
        val tab: Tab = mockk(relaxed = true)
        interactor.onCollectionOpenTabClicked(tab)
        verify { controller.handleCollectionOpenTabClicked(tab) }
    }

    @Test
    fun onCollectionOpenTabsTapped() {
        val collection: TabCollection = mockk(relaxed = true)
        interactor.onCollectionOpenTabsTapped(collection)
        verify { controller.handleCollectionOpenTabsTapped(collection) }
    }

    @Test
    fun onCollectionRemoveTab() {
        val collection: TabCollection = mockk(relaxed = true)
        val tab: Tab = mockk(relaxed = true)
        interactor.onCollectionRemoveTab(collection, tab, false)
        verify { controller.handleCollectionRemoveTab(collection, tab, false) }
    }

    @Test
    fun onCollectionShareTabsClicked() {
        val collection: TabCollection = mockk(relaxed = true)
        interactor.onCollectionShareTabsClicked(collection)
        verify { controller.handleCollectionShareTabsClicked(collection) }
    }

    @Test
    fun onDeleteCollectionTapped() {
        val collection: TabCollection = mockk(relaxed = true)
        interactor.onDeleteCollectionTapped(collection)
        verify { controller.handleDeleteCollectionTapped(collection) }
    }

    @Test
    fun onPrivateBrowsingLearnMoreClicked() {
        interactor.onPrivateBrowsingLearnMoreClicked()
        verify { controller.handlePrivateBrowsingLearnMoreClicked() }
    }

    @Test
    fun onRenameCollectionTapped() {
        val collection: TabCollection = mockk(relaxed = true)
        interactor.onRenameCollectionTapped(collection)
        verify { controller.handleRenameCollectionTapped(collection) }
    }

    @Test
    fun onStartBrowsingClicked() {
        interactor.onStartBrowsingClicked()
        verify { onboardingController.handleStartBrowsingClicked() }
    }

    @Test
    fun onReadPrivacyNoticeClicked() {
        interactor.onReadPrivacyNoticeClicked()
        verify { onboardingController.handleReadPrivacyNoticeClicked() }
    }

    @Test
    fun onToggleCollectionExpanded() {
        val collection: TabCollection = mockk(relaxed = true)
        interactor.onToggleCollectionExpanded(collection, true)
        verify { controller.handleToggleCollectionExpanded(collection, true) }
    }

    @Test
    fun onAddTabsToCollection() {
        interactor.onAddTabsToCollectionTapped()
        verify { controller.handleCreateCollection() }
    }

    @Test
    fun onPaste() {
        interactor.onPaste("text")
        verify { controller.handlePaste("text") }
    }

    @Test
    fun onPasteAndGo() {
        interactor.onPasteAndGo("text")
        verify { controller.handlePasteAndGo("text") }
    }

    @Test
    fun onNavigateSearch() {
        interactor.onNavigateSearch()
        verify { controller.handleNavigateSearch() }
    }

    @Test
    fun onRemoveCollectionsPlaceholder() {
        interactor.onRemoveCollectionsPlaceholder()
        verify { controller.handleRemoveCollectionsPlaceholder() }
    }

    @Test
    fun onRecentTabClicked() {
        val tabId = "tabId"
        interactor.onRecentTabClicked(tabId)
        verify { recentTabController.handleRecentTabClicked(tabId) }
    }

    @Test
    fun onRecentTabShowAllClicked() {
        interactor.onRecentTabShowAllClicked()
        verify { recentTabController.handleRecentTabShowAllClicked() }
    }

    @Test
    fun `WHEN recent synced tab is clicked THEN the tab is handled`() {
        val tab: RecentSyncedTab = mockk()
        interactor.onRecentSyncedTabClicked(tab)

        verify { recentSyncedTabController.handleRecentSyncedTabClick(tab) }
    }

    @Test
    fun `WHEN recent synced tabs show all is clicked THEN show all synced tabs is handled`() {
        interactor.onSyncedTabShowAllClicked()

        verify { recentSyncedTabController.handleSyncedTabShowAllClicked() }
    }

    @Test
    fun `WHEN a recently saved bookmark is clicked THEN the selected bookmark is handled`() {
        val bookmark = RecentBookmark()

        interactor.onRecentBookmarkClicked(bookmark)
        verify { recentBookmarksController.handleBookmarkClicked(bookmark) }
    }

    @Test
    fun `WHEN tapping on the customize home button THEN openCustomizeHomePage`() {
        interactor.openCustomizeHomePage()
        verify { controller.handleCustomizeHomeTapped() }
    }

    @Test
    fun `WHEN Show All recently saved bookmarks button is clicked THEN the click is handled`() {
        interactor.onShowAllBookmarksClicked()
        verify { recentBookmarksController.handleShowAllBookmarksClicked() }
    }

    @Test
    fun `WHEN private mode button is clicked THEN the click is handled`() {
        val newMode = BrowsingMode.Private
        val hasBeenOnboarded = true

        interactor.onPrivateModeButtonClicked(newMode, hasBeenOnboarded)
        verify { controller.handlePrivateModeButtonClicked(newMode, hasBeenOnboarded) }
    }

    @Test
    fun `WHEN onSettingsClicked is called THEN handleTopSiteSettingsClicked is called`() {
        interactor.onSettingsClicked()
        verify { controller.handleTopSiteSettingsClicked() }
    }

    @Test
    fun `WHEN onSponsorPrivacyClicked is called THEN handleSponsorPrivacyClicked is called`() {
        interactor.onSponsorPrivacyClicked()
        verify { controller.handleSponsorPrivacyClicked() }
    }

    @Test
    fun `GIVEN a PocketStoriesInteractor WHEN a story is shown THEN handle it in a PocketStoriesController`() {
        val shownStory: PocketStory = mockk()
        val storyGridLocation = 1 to 2

        interactor.onStoryShown(shownStory, storyGridLocation)

        verify { pocketStoriesController.handleStoryShown(shownStory, storyGridLocation) }
    }

    @Test
    fun `GIVEN a PocketStoriesInteractor WHEN stories are shown THEN handle it in a PocketStoriesController`() {
        val shownStories: List<PocketStory> = mockk()

        interactor.onStoriesShown(shownStories)

        verify { pocketStoriesController.handleStoriesShown(shownStories) }
    }

    @Test
    fun `GIVEN a PocketStoriesInteractor WHEN a category is clicked THEN handle it in a PocketStoriesController`() {
        val clickedCategory: PocketRecommendedStoriesCategory = mockk()

        interactor.onCategoryClicked(clickedCategory)

        verify { pocketStoriesController.handleCategoryClick(clickedCategory) }
    }

    @Test
    fun `GIVEN a PocketStoriesInteractor WHEN a story is clicked THEN handle it in a PocketStoriesController`() {
        val clickedStory: PocketStory = mockk()
        val storyGridLocation = 1 to 2

        interactor.onStoryClicked(clickedStory, storyGridLocation)

        verify { pocketStoriesController.handleStoryClicked(clickedStory, storyGridLocation) }
    }

    @Test
    fun `GIVEN a PocketStoriesInteractor WHEN discover more clicked THEN handle it in a PocketStoriesController`() {
        val link = "http://getpocket.com/explore"

        interactor.onDiscoverMoreClicked(link)

        verify { pocketStoriesController.handleDiscoverMoreClicked(link) }
    }

    @Test
    fun `GIVEN a PocketStoriesInteractor WHEN learn more clicked THEN handle it in a PocketStoriesController`() {
        val link = "https://www.mozilla.org/en-US/firefox/pocket/"

        interactor.onLearnMoreClicked(link)

        verify { pocketStoriesController.handleLearnMoreClicked(link) }
    }

    @Test
    fun reportSessionMetrics() {
        val appState: AppState = mockk(relaxed = true)
        every { appState.recentBookmarks } returns emptyList()
        interactor.reportSessionMetrics(appState)
        verify { controller.handleReportSessionMetrics(appState) }
    }
}
