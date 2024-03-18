/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

package org.mozilla.fenix.home.recentbookmarks.interactor

import org.mozilla.fenix.home.recentbookmarks.RecentBookmark
import org.mozilla.fenix.home.sessioncontrol.SessionControlInteractor

/**
 * Interface for recently saved bookmark related actions in the [SessionControlInteractor].
 */
interface RecentBookmarksInteractor {

    /**
     * Opens the given bookmark in a new tab. Called when an user clicks on a recently saved
     * bookmark on the home screen.
     *
     * @param bookmark The bookmark that will be opened.
     */
    fun onRecentBookmarkClicked(bookmark: RecentBookmark)

    /**
     * Navigates to bookmark list. Called when an user clicks on the "Show all" button for
     * recently saved bookmarks on the home screen.
     */
    fun onShowAllBookmarksClicked()

    /**
     * Removes a bookmark from the recent bookmark list. Called when a user clicks the "Remove"
     * button for recently saved bookmarks on the home screen.
     *
     * @param bookmark The bookmark that has been removed.
     */
    fun onRecentBookmarkRemoved(bookmark: RecentBookmark)
}
