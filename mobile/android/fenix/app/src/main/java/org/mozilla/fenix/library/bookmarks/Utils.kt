/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

package org.mozilla.fenix.library.bookmarks

import android.content.Context
import mozilla.components.concept.storage.BookmarkNode
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
