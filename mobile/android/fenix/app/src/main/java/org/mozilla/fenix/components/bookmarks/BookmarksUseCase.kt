/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

package org.mozilla.fenix.components.bookmarks

import androidx.annotation.WorkerThread
import mozilla.appservices.places.BookmarkRoot
import mozilla.components.concept.storage.BookmarksStorage

/**
 * Use cases that allow for modifying bookmarks.
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

    val addBookmark by lazy { AddBookmarksUseCase(storage) }
}
