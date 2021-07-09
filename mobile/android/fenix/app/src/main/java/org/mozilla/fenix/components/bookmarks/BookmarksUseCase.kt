/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

package org.mozilla.fenix.components.bookmarks

import androidx.annotation.WorkerThread
import mozilla.appservices.places.BookmarkRoot
import mozilla.components.concept.storage.BookmarkNode
import mozilla.components.concept.storage.BookmarksStorage

/**
 * Use cases that allow for modifying and retrieving bookmarks.
 */
class BookmarksUseCase(storage: BookmarksStorage) {

    class AddBookmarksUseCase internal constructor(private val storage: BookmarksStorage) {

        /**
         * Adds a new bookmark with the provided [url] and [title].
         *
         * @return The result if the operation was executed or not. A bookmark may not be added if
         * one with the identical [url] already exists.
         */
        @WorkerThread
        suspend operator fun invoke(url: String, title: String, position: Int? = null): Boolean {
            val canAdd = storage.getBookmarksWithUrl(url).firstOrNull { it.url == it.url } == null

            if (canAdd) {
                storage.addItem(
                    BookmarkRoot.Mobile.id,
                    url = url,
                    title = title,
                    position = position
                )
            }

            return canAdd
        }
    }

    class RetrieveRecentBookmarksUseCase internal constructor(
        private val storage: BookmarksStorage
    ) {
        /**
         * Retrieves a list of recently added bookmarks, if any, up to maximum.
         */
        @WorkerThread
        suspend operator fun invoke(count: Int = DEFAULT_BOOKMARKS_TO_RETRIEVE): List<BookmarkNode> {
            return storage.getRecentBookmarks(count)
        }
    }

    val addBookmark by lazy { AddBookmarksUseCase(storage) }
    val retrieveRecentBookmarks by lazy { RetrieveRecentBookmarksUseCase(storage) }

    companion object {
        const val DEFAULT_BOOKMARKS_TO_RETRIEVE = 4
    }
}
