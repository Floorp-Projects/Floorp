/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

package org.mozilla.fenix.ext

import android.content.Context
import mozilla.components.browser.storage.sync.PlacesBookmarksStorage
import mozilla.components.concept.storage.BookmarkNode

val Context.bookmarkStorage: PlacesBookmarksStorage
    get() = components.core.bookmarksStorage

/**
 * Removes [children] from [BookmarkNode.children] and returns the new modified [BookmarkNode].
 */
operator fun BookmarkNode.minus(children: Set<BookmarkNode>): BookmarkNode {
    val removedChildrenGuids = children.map { it.guid }
    return this.copy(children = this.children?.filterNot { removedChildrenGuids.contains(it.guid) })
}
