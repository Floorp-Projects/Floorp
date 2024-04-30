/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

package org.mozilla.fenix.home.bookmarks.interactor

import org.mozilla.fenix.home.bookmarks.Bookmark
import org.mozilla.fenix.home.sessioncontrol.SessionControlInteractor

/**
 * Interface for bookmark related actions in the [SessionControlInteractor].
 */
interface BookmarksInteractor {

    /**
     * Opens the given bookmark in a new tab. Called when an user clicks on a bookmark on the home
     * screen.
     *
     * @param bookmark The bookmark that will be opened.
     */
    fun onBookmarkClicked(bookmark: Bookmark)

    /**
     * Navigates to bookmark list. Called when an user clicks on the "Show all" button for
     * bookmarks on the home screen.
     */
    fun onShowAllBookmarksClicked()

    /**
     * Removes a bookmark from the list on the home screen. Called when a user clicks the "Remove"
     * button for a bookmark on the home screen.
     *
     * @param bookmark The bookmark that has been removed.
     */
    fun onBookmarkRemoved(bookmark: Bookmark)
}
