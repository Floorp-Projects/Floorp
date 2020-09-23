/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

package org.mozilla.fenix.library.bookmarks

import android.content.Context
import mozilla.components.concept.storage.BookmarkNode
import mozilla.components.concept.storage.BookmarkNodeType
import org.mozilla.fenix.R

fun rootTitles(context: Context, withMobileRoot: Boolean): Map<String, String> = if (withMobileRoot) {
    mapOf(
        "root" to context.getString(R.string.library_bookmarks),
        "mobile" to context.getString(R.string.library_bookmarks),
        "menu" to context.getString(R.string.library_desktop_bookmarks_menu),
        "toolbar" to context.getString(R.string.library_desktop_bookmarks_toolbar),
        "unfiled" to context.getString(R.string.library_desktop_bookmarks_unfiled)
    )
} else {
    mapOf(
        "root" to context.getString(R.string.library_desktop_bookmarks_root),
        "menu" to context.getString(R.string.library_desktop_bookmarks_menu),
        "toolbar" to context.getString(R.string.library_desktop_bookmarks_toolbar),
        "unfiled" to context.getString(R.string.library_desktop_bookmarks_unfiled)
    )
}

fun friendlyRootTitle(
    context: Context,
    node: BookmarkNode,
    withMobileRoot: Boolean = true,
    rootTitles: Map<String, String> = rootTitles(context, withMobileRoot)
) = when {
    !node.inRoots() -> node.title
    rootTitles.containsKey(node.title) -> rootTitles[node.title]
    else -> node.title
}

data class BookmarkNodeWithDepth(val depth: Int, val node: BookmarkNode, val parent: String?)

fun BookmarkNode.flatNodeList(excludeSubtreeRoot: String?, depth: Int = 0): List<BookmarkNodeWithDepth> {
    if (this.type != BookmarkNodeType.FOLDER || this.guid == excludeSubtreeRoot) {
        return emptyList()
    }
    val newList = listOf(BookmarkNodeWithDepth(depth, this, this.parentGuid))
    return newList + children
        ?.filter { it.type == BookmarkNodeType.FOLDER }
        ?.flatMap { it.flatNodeList(excludeSubtreeRoot = excludeSubtreeRoot, depth = depth + 1) }
        .orEmpty()
}
