/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

package org.mozilla.fenix.library.bookmarks

import mozilla.components.concept.storage.BookmarkNode
import mozilla.components.concept.storage.BookmarkNodeType
import mozilla.components.support.test.robolectric.testContext
import org.junit.Assert.assertEquals
import org.junit.Test
import org.junit.runner.RunWith
import org.mozilla.fenix.helpers.FenixRobolectricTestRunner

@RunWith(FenixRobolectricTestRunner::class)
class UtilsKtTest {
    @Test
    fun `friendly root titles`() {
        val url = testBookmarkItem("folder", "http://mozilla.org", "Mozilla")
        assertEquals("Mozilla", friendlyRootTitle(testContext, url))

        val folder = testFolder("456", "folder", null, "Folder")
        assertEquals("Folder", friendlyRootTitle(testContext, folder))

        val root = folder.copy(guid = "root________", title = "root")
        assertEquals("Bookmarks", friendlyRootTitle(testContext, root, withMobileRoot = true))
        assertEquals("Desktop Bookmarks", friendlyRootTitle(testContext, root, withMobileRoot = false))

        val mobileRoot = folder.copy(guid = "mobile______", title = "mobile")
        assertEquals("Bookmarks", friendlyRootTitle(testContext, mobileRoot, withMobileRoot = true))
        assertEquals("mobile", friendlyRootTitle(testContext, mobileRoot, withMobileRoot = false))

        val menuRoot = folder.copy(guid = "menu________", title = "menu")
        assertEquals("Bookmarks Menu", friendlyRootTitle(testContext, menuRoot, withMobileRoot = true))
        assertEquals("Bookmarks Menu", friendlyRootTitle(testContext, menuRoot, withMobileRoot = false))

        val toolbarRoot = folder.copy(guid = "toolbar_____", title = "toolbar")
        assertEquals("Bookmarks Toolbar", friendlyRootTitle(testContext, toolbarRoot, withMobileRoot = true))
        assertEquals("Bookmarks Toolbar", friendlyRootTitle(testContext, toolbarRoot, withMobileRoot = false))

        val unfiledRoot = folder.copy(guid = "unfiled_____", title = "unfiled")
        assertEquals("Other Bookmarks", friendlyRootTitle(testContext, unfiledRoot, withMobileRoot = true))
        assertEquals("Other Bookmarks", friendlyRootTitle(testContext, unfiledRoot, withMobileRoot = false))

        val almostRoot = folder.copy(guid = "notRoot________", title = "root")
        assertEquals("root", friendlyRootTitle(testContext, almostRoot, withMobileRoot = true))
        assertEquals("root", friendlyRootTitle(testContext, almostRoot, withMobileRoot = false))
    }

    @Test
    fun `flatNodeList various cases`() {
        val url = testBookmarkItem("folder", "http://mozilla.org")
        val url2 = testBookmarkItem("folder2", "http://mozilla.org")
        assertEquals(emptyList<BookmarkNodeWithDepth>(), url.flatNodeList(null))

        val root = testFolder("root", null, null)
        assertEquals(listOf(BookmarkNodeWithDepth(0, root, null)), root.flatNodeList(null))
        assertEquals(emptyList<BookmarkNodeWithDepth>(), root.flatNodeList("root"))

        val folder = testFolder("folder", root.guid, listOf(url))
        val folder3 = testFolder("folder3", "folder2", null)
        val folder2 = testFolder("folder2", root.guid, listOf(folder3, url2))

        val rootWithChildren = root.copy(children = listOf(folder, folder2))
        assertEquals(
            listOf(
                BookmarkNodeWithDepth(0, rootWithChildren, null),
                BookmarkNodeWithDepth(1, folder, "root"),
                BookmarkNodeWithDepth(1, folder2, "root"),
                BookmarkNodeWithDepth(2, folder3, "folder2"),
            ),
            rootWithChildren.flatNodeList(null),
        )

        assertEquals(
            listOf(
                BookmarkNodeWithDepth(0, rootWithChildren, null),
                BookmarkNodeWithDepth(1, folder, "root"),
            ),
            rootWithChildren.flatNodeList(excludeSubtreeRoot = "folder2"),
        )
    }
}

internal fun testBookmarkItem(parentGuid: String, url: String, title: String = "Item for $url") = BookmarkNode(
    BookmarkNodeType.ITEM,
    "guid#${Math.random() * 1000}",
    parentGuid,
    0u,
    title,
    url,
    0,
    null,
)

internal fun testFolder(guid: String, parentGuid: String?, children: List<BookmarkNode>?, title: String = "Folder: $guid") = BookmarkNode(
    BookmarkNodeType.FOLDER,
    guid,
    parentGuid,
    0u,
    title,
    null,
    0,
    children,
)

internal fun testSeparator(parentGuid: String) = BookmarkNode(
    BookmarkNodeType.SEPARATOR,
    "guid#${Math.random() * 1000}",
    parentGuid,
    null,
    null,
    null,
    0,
    null,
)
