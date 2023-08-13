/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

package org.mozilla.fenix.home.recentbookmarks

import kotlinx.coroutines.CoroutineDispatcher
import kotlinx.coroutines.CoroutineScope
import kotlinx.coroutines.Dispatchers
import kotlinx.coroutines.Job
import kotlinx.coroutines.launch
import mozilla.components.concept.storage.BookmarkNode
import mozilla.components.support.base.feature.LifecycleAwareFeature
import org.mozilla.fenix.components.AppStore
import org.mozilla.fenix.components.appstate.AppAction
import org.mozilla.fenix.components.bookmarks.BookmarksUseCase
import org.mozilla.fenix.home.HomeFragment

/**
 * View-bound feature that retrieves a list of recently added [BookmarkNode]s and dispatches
 * updates to the [AppStore].
 *
 * @property appStore the [AppStore] that holds the state of the [HomeFragment].
 * @property bookmarksUseCase the [BookmarksUseCase] for retrieving the list of recently saved
 * bookmarks from storage.
 * @property scope the [CoroutineScope] used to fetch the bookmarks list
 * @property ioDispatcher the [CoroutineDispatcher] for performing read/write operations.
 */
class RecentBookmarksFeature(
    private val appStore: AppStore,
    private val bookmarksUseCase: BookmarksUseCase,
    private val scope: CoroutineScope,
    private val ioDispatcher: CoroutineDispatcher = Dispatchers.IO,
) : LifecycleAwareFeature {
    private var job: Job? = null

    override fun start() {
        job = scope.launch(ioDispatcher) {
            val bookmarks = bookmarksUseCase.retrieveRecentBookmarks()
            appStore.dispatch(AppAction.RecentBookmarksChange(bookmarks))
        }
    }

    override fun stop() {
        job?.cancel()
    }
}

/**
 * A bookmark that was recently added.
 *
 * @property title The title of the bookmark.
 * @property url The url of the bookmark.
 * @property previewImageUrl A preview image of the page (a.k.a. the hero image), if available.
 */
data class RecentBookmark(
    val title: String? = null,
    val url: String? = null,
    val previewImageUrl: String? = null,
)
