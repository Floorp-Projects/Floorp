/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

package org.mozilla.fenix.home.bookmarks.controller

import androidx.navigation.NavController
import mozilla.appservices.places.BookmarkRoot
import mozilla.components.browser.state.store.BrowserStore
import mozilla.components.concept.engine.EngineSession
import mozilla.components.concept.engine.EngineSession.LoadUrlFlags.Companion.ALLOW_JAVASCRIPT_URL
import mozilla.components.feature.tabs.TabsUseCases
import org.mozilla.fenix.BrowserDirection
import org.mozilla.fenix.GleanMetrics.HomeBookmarks
import org.mozilla.fenix.HomeActivity
import org.mozilla.fenix.R
import org.mozilla.fenix.components.AppStore
import org.mozilla.fenix.components.appstate.AppAction
import org.mozilla.fenix.home.HomeFragmentDirections
import org.mozilla.fenix.home.bookmarks.Bookmark
import org.mozilla.fenix.home.bookmarks.interactor.BookmarksInteractor

/**
 * An interface that handles the view manipulation of the bookmarks on the
 * Home screen.
 */
interface BookmarksController {

    /**
     * @see [BookmarksInteractor.onBookmarkClicked]
     */
    fun handleBookmarkClicked(bookmark: Bookmark)

    /**
     * @see [BookmarksInteractor.onShowAllBookmarksClicked]
     */
    fun handleShowAllBookmarksClicked()

    /**
     * @see [BookmarksInteractor.onBookmarkRemoved]
     */
    fun handleBookmarkRemoved(bookmark: Bookmark)
}

/**
 * The default implementation of [BookmarksController].
 */
class DefaultBookmarksController(
    private val activity: HomeActivity,
    private val navController: NavController,
    private val appStore: AppStore,
    private val browserStore: BrowserStore,
    private val selectTabUseCase: TabsUseCases.SelectTabUseCase,
) : BookmarksController {

    override fun handleBookmarkClicked(bookmark: Bookmark) {
        val existingTabForBookmark = browserStore.state.tabs.firstOrNull {
            it.content.url == bookmark.url
        }

        if (existingTabForBookmark == null) {
            activity.openToBrowserAndLoad(
                searchTermOrURL = bookmark.url!!,
                newTab = true,
                from = BrowserDirection.FromHome,
                flags = EngineSession.LoadUrlFlags.select(ALLOW_JAVASCRIPT_URL),
            )
        } else {
            selectTabUseCase.invoke(existingTabForBookmark.id)
            navController.navigate(R.id.browserFragment)
        }

        HomeBookmarks.bookmarkClicked.add()
    }

    override fun handleShowAllBookmarksClicked() {
        HomeBookmarks.showAllBookmarks.add()
        navController.navigate(
            HomeFragmentDirections.actionGlobalBookmarkFragment(BookmarkRoot.Mobile.id),
        )
    }

    override fun handleBookmarkRemoved(bookmark: Bookmark) {
        appStore.dispatch(AppAction.RemoveBookmark(bookmark))
    }
}
