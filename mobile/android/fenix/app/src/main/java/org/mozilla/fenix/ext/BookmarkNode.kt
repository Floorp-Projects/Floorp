/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

package org.mozilla.fenix.ext

import android.content.Context
import mozilla.appservices.places.BookmarkRoot
import mozilla.components.browser.storage.sync.PlacesBookmarksStorage
import mozilla.components.concept.storage.BookmarkNode
import org.mozilla.fenix.R

var rootTitles: Map<String, String> = emptyMap()

@SuppressWarnings("ComplexMethod")
suspend fun BookmarkNode?.withOptionalDesktopFolders(
    context: Context?,
    showMobileRoot: Boolean = false
): BookmarkNode? {
    // No-op if node is missing.
    if (this == null) {
        return null
    }

    val loggedIn = context?.components?.backgroundServices?.accountManager?.authenticatedAccount() != null

    // If we're in the mobile root and logged in, add-in a synthetic "Desktop Bookmarks" folder.
    return if (guid == BookmarkRoot.Mobile.id && loggedIn) {
        // We're going to make a copy of the mobile node, and add-in a synthetic child folder to the top of the
        // children's list that contains all of the desktop roots.
        val childrenWithVirtualFolder: MutableList<BookmarkNode> = mutableListOf()
        virtualDesktopFolder(context)?.let { childrenWithVirtualFolder.add(it) }

        this.children?.let { children ->
            childrenWithVirtualFolder.addAll(children)
        }

        copy(children = childrenWithVirtualFolder)
    } else if (guid == BookmarkRoot.Root.id) {
        copy(
            title = rootTitles[this.title],
            children = if (showMobileRoot) restructureMobileRoots(context, children)
            else restructureDesktopRoots(children)
        )
    } else if (guid in listOf(BookmarkRoot.Menu.id, BookmarkRoot.Toolbar.id, BookmarkRoot.Unfiled.id)) {
        // If we're looking at one of the desktop roots, change their titles to friendly names.
        copy(title = rootTitles[this.title])
    } else {
        // Otherwise, just return the node as-is.
        this
    }
}

private suspend fun virtualDesktopFolder(context: Context?): BookmarkNode? {
    val rootNode = context?.bookmarkStorage()?.getTree(BookmarkRoot.Root.id, false) ?: return null
    return rootNode.copy(title = rootTitles[rootNode.title])
}

/**
 * Removes 'mobile' root (to avoid a cyclical bookmarks tree in the UI) and renames other roots to friendly titles.
 */
private fun restructureDesktopRoots(roots: List<BookmarkNode>?): List<BookmarkNode>? {
    if (roots == null) {
        return null
    }

    return roots.filter { rootTitles.containsKey(it.title) }.map {
        it.copy(title = rootTitles[it.title])
    }
}

/**
 * Restructures roots to place desktop roots underneath the mobile root and renames them to friendly titles.
 * This provides a recognizable bookmark tree when offering destinations to move a bookmark.
 */
private fun restructureMobileRoots(context: Context?, roots: List<BookmarkNode>?): List<BookmarkNode>? {
    if (roots == null) {
        return null
    }

    val loggedIn = context?.components?.backgroundServices?.accountManager?.authenticatedAccount() != null

    val others = roots.filter { it.guid != BookmarkRoot.Mobile.id }
        .map { it.copy(title = rootTitles[it.title]) }

    val mobileRoot = roots.find { it.guid == BookmarkRoot.Mobile.id } ?: return roots
    val mobileChildren = (if (loggedIn) others else listOf()) + (mobileRoot.children ?: listOf())

    // Note that the desktop bookmarks folder does not appear because it is not selectable as a parent
    return listOf(
        mobileRoot.copy(
            children = mobileChildren,
            title = context?.getString(R.string.library_bookmarks)
        )
    )
}

fun Context?.bookmarkStorage(): PlacesBookmarksStorage? {
    return this?.components?.core?.bookmarksStorage
}

fun setRootTitles(context: Context, showMobileRoot: Boolean = false) {
    rootTitles = if (showMobileRoot) {
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
}

fun BookmarkNode?.withRootTitle(): BookmarkNode? {
    if (this == null) {
        return null
    }
    return if (rootTitles.containsKey(title)) this.copy(title = rootTitles[title]) else this
}

operator fun BookmarkNode?.minus(child: String): BookmarkNode {
    return this!!.copy(children = this.children?.filter { it.guid != child })
}

operator fun BookmarkNode?.minus(children: Set<BookmarkNode>): BookmarkNode {
    return this!!.copy(children = this.children?.filter { it !in children })
}
