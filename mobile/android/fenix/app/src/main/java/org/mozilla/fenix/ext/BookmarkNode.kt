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

@SuppressWarnings("ReturnCount")
suspend fun BookmarkNode?.withOptionalDesktopFolders(
    context: Context?
): BookmarkNode? {
    // No-op if node is missing.
    if (this == null) {
        return null
    }

    // If we're in the mobile root and logged in, add-in a synthetic "Desktop Bookmarks" folder.
    if (this.guid == BookmarkRoot.Mobile.id &&
        context?.components?.backgroundServices?.accountManager?.authenticatedAccount() != null
    ) {
        // We're going to make a copy of the mobile node, and add-in a synthetic child folder to the top of the
        // children's list that contains all of the desktop roots.
        val childrenWithVirtualFolder: MutableList<BookmarkNode> = mutableListOf()
        virtualDesktopFolder(context)?.let { childrenWithVirtualFolder.add(it) }

        this.children?.let { children ->
            childrenWithVirtualFolder.addAll(children)
        }

        return BookmarkNode(
            type = this.type,
            guid = this.guid,
            parentGuid = this.parentGuid,
            position = this.position,
            title = this.title,
            url = this.url,
            children = childrenWithVirtualFolder
        )

        // If we're looking at the root, that means we're in the "Desktop Bookmarks" folder.
        // Rename its child roots and remove the mobile root.
    } else if (this.guid == BookmarkRoot.Root.id) {
        return BookmarkNode(
            type = this.type,
            guid = this.guid,
            parentGuid = this.parentGuid,
            position = this.position,
            title = rootTitles[this.title],
            url = this.url,
            children = processDesktopRoots(this.children)
        )
        // If we're looking at one of the desktop roots, change their titles to friendly names.
    } else if (this.guid in listOf(BookmarkRoot.Menu.id, BookmarkRoot.Toolbar.id, BookmarkRoot.Unfiled.id)) {
        return BookmarkNode(
            type = this.type,
            guid = this.guid,
            parentGuid = this.parentGuid,
            position = this.position,
            title = rootTitles[this.title],
            url = this.url,
            children = this.children
        )
    }

    // Otherwise, just return the node as-is.
    return this
}

private suspend fun virtualDesktopFolder(context: Context?): BookmarkNode? {
    val rootNode = context?.bookmarkStorage()?.getTree(BookmarkRoot.Root.id, false) ?: return null
    return BookmarkNode(
        type = rootNode.type,
        guid = rootNode.guid,
        parentGuid = rootNode.parentGuid,
        position = rootNode.position,
        title = rootTitles[rootNode.title],
        url = rootNode.url,
        children = rootNode.children
    )
}

/**
 * Removes 'mobile' root (to avoid a cyclical bookmarks tree in the UI) and renames other roots to friendly titles.
 */
private fun processDesktopRoots(roots: List<BookmarkNode>?): List<BookmarkNode>? {
    if (roots == null) {
        return null
    }

    return roots.filter { rootTitles.containsKey(it.title) }.map {
        BookmarkNode(
            type = it.type,
            guid = it.guid,
            parentGuid = it.parentGuid,
            position = it.position,
            title = rootTitles[it.title],
            url = it.url,
            children = it.children
        )
    }
}

fun Context?.bookmarkStorage(): PlacesBookmarksStorage? {
    return this?.components?.core?.bookmarksStorage
}

fun setRootTitles(context: Context) {
    rootTitles = mapOf(
        "root" to context.getString(R.string.library_desktop_bookmarks_root),
        "menu" to context.getString(R.string.library_desktop_bookmarks_menu),
        "toolbar" to context.getString(R.string.library_desktop_bookmarks_toolbar),
        "unfiled" to context.getString(R.string.library_desktop_bookmarks_unfiled)
    )
}

operator fun BookmarkNode?.minus(child: String): BookmarkNode {
    return this!!.copy(children = this.children?.filter { it.guid != child })
}

operator fun BookmarkNode?.minus(children: Set<BookmarkNode>): BookmarkNode {
    return this!!.copy(children = this.children?.filter { it !in children })
}
