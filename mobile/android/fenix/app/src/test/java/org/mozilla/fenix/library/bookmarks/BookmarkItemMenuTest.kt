/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

package org.mozilla.fenix.library.bookmarks

import android.content.Context
import androidx.appcompat.view.ContextThemeWrapper
import io.mockk.coEvery
import io.mockk.every
import io.mockk.mockk
import io.mockk.mockkStatic
import kotlinx.coroutines.runBlocking
import mozilla.components.concept.menu.candidate.TextMenuCandidate
import mozilla.components.concept.menu.candidate.TextStyle
import mozilla.components.concept.storage.BookmarkNodeType
import mozilla.components.support.ktx.android.content.getColorFromAttr
import mozilla.components.support.test.robolectric.testContext
import org.junit.Assert.assertEquals
import org.junit.Assert.assertNotNull
import org.junit.Before
import org.junit.Test
import org.junit.runner.RunWith
import org.mozilla.fenix.R
import org.mozilla.fenix.ext.bookmarkStorage
import org.mozilla.fenix.helpers.FenixRobolectricTestRunner
import org.mozilla.fenix.library.bookmarks.BookmarkItemMenu.Item

@RunWith(FenixRobolectricTestRunner::class)
class BookmarkItemMenuTest {

    private lateinit var context: Context
    private lateinit var menu: BookmarkItemMenu
    private var lastItemTapped: Item? = null

    @Before
    fun setup() {
        context = ContextThemeWrapper(testContext, R.style.NormalTheme)
        menu = BookmarkItemMenu(context) {
            lastItemTapped = it
        }
    }

    @Test
    fun `delete item has special styling`() = runBlocking {
        var deleteItem: TextMenuCandidate? = null
        mockkStatic("org.mozilla.fenix.ext.BookmarkNodeKt") {
            every { any<Context>().bookmarkStorage } returns mockk(relaxed = true)

            deleteItem = menu.menuItems(BookmarkNodeType.SEPARATOR, "").last()
        }
        assertNotNull(deleteItem)
        assertEquals("Delete", deleteItem!!.text)
        assertEquals(
            TextStyle(color = context.getColorFromAttr(R.attr.textCritical)),
            deleteItem!!.textStyle,
        )

        deleteItem!!.onClick()

        assertEquals(Item.Delete, lastItemTapped)
    }

    @Test
    fun `edit item appears for folders`() = runBlocking {
        // empty folder
        var emptyFolderItems: List<TextMenuCandidate>? = null
        mockkStatic("org.mozilla.fenix.ext.BookmarkNodeKt") {
            every { any<Context>().bookmarkStorage } returns mockk(relaxed = true)

            emptyFolderItems = menu.menuItems(BookmarkNodeType.FOLDER, "")
        }
        assertNotNull(emptyFolderItems)
        assertEquals(2, emptyFolderItems!!.size)

        // not empty
        var folderItems: List<TextMenuCandidate>? = null
        mockkStatic("org.mozilla.fenix.ext.BookmarkNodeKt") {
            coEvery { any<Context>().bookmarkStorage.getTree("")?.children } returns listOf(mockk(relaxed = true))

            folderItems = menu.menuItems(BookmarkNodeType.FOLDER, "")
        }
        assertNotNull(folderItems)
        assertEquals(4, folderItems!!.size)

        val (edit, openAll, openAllPrivate, delete) = folderItems!!

        assertEquals("Edit", edit.text)
        assertEquals("Open all in new tabs", openAll.text)
        assertEquals("Open all in private tabs", openAllPrivate.text)
        assertEquals("Delete", delete.text)

        edit.onClick()
        assertEquals(Item.Edit, lastItemTapped)

        openAll.onClick()
        assertEquals(Item.OpenAllInNewTabs, lastItemTapped)

        openAllPrivate.onClick()
        assertEquals(Item.OpenAllInPrivateTabs, lastItemTapped)

        delete.onClick()
        assertEquals(Item.Delete, lastItemTapped)
    }

    @Test
    fun `all item appears for sites`() = runBlocking {
        var siteItems: List<TextMenuCandidate>? = null
        mockkStatic("org.mozilla.fenix.ext.BookmarkNodeKt") {
            every { any<Context>().bookmarkStorage } returns mockk(relaxed = true)

            siteItems = menu.menuItems(BookmarkNodeType.ITEM, "")
        }
        assertNotNull(siteItems)
        assertEquals(6, siteItems!!.size)
        val (edit, copy, share, openInNewTab, openInPrivateTab, delete) = siteItems!!

        assertEquals("Edit", edit.text)
        assertEquals("Copy", copy.text)
        assertEquals("Share", share.text)
        assertEquals("Open in new tab", openInNewTab.text)
        assertEquals("Open in private tab", openInPrivateTab.text)
        assertEquals("Delete", delete.text)

        edit.onClick()
        assertEquals(Item.Edit, lastItemTapped)

        copy.onClick()
        assertEquals(Item.Copy, lastItemTapped)

        share.onClick()
        assertEquals(Item.Share, lastItemTapped)

        openInNewTab.onClick()
        assertEquals(Item.OpenInNewTab, lastItemTapped)

        openInPrivateTab.onClick()
        assertEquals(Item.OpenInPrivateTab, lastItemTapped)

        delete.onClick()
        assertEquals(Item.Delete, lastItemTapped)
    }

    private operator fun <T> List<T>.component6(): T {
        return get(5)
    }
}
