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
import org.mozilla.fenix.components.bookmarks.BookmarksUseCase
import org.mozilla.fenix.home.HomeFragmentAction
import org.mozilla.fenix.home.HomeFragmentStore

/**
 *  View-bound feature that retrieves a list of recently added [BookmarkNode]s and dispatches
 *  updates to the [HomeFragmentStore].
 *
 *  @param homeStore the [HomeFragmentStore]
 *  @param bookmarksUseCase the [BookmarksUseCase] for retrieving the list of recently saved
*   bookmarks from storage.
 *  @param scope the [CoroutineScope] used to fetch the bookmarks list
 *  @param ioDispatcher the [CoroutineDispatcher] for performing read/write operations.
 */
class RecentBookmarksFeature(
    private val homeStore: HomeFragmentStore,
    private val bookmarksUseCase: BookmarksUseCase,
    private val scope: CoroutineScope,
    private val ioDispatcher: CoroutineDispatcher = Dispatchers.IO
) : LifecycleAwareFeature {
    internal var job: Job? = null

    override fun start() {
        job = scope.launch(ioDispatcher) {
            val bookmarks = bookmarksUseCase.retrieveRecentBookmarks()

            homeStore.dispatch(HomeFragmentAction.RecentBookmarksChange(bookmarks))
        }
    }

    override fun stop() {
        job?.cancel()
    }
}
