/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

package org.mozilla.fenix.library.bookmarks

import android.content.Context
import androidx.appcompat.view.ContextThemeWrapper
import io.mockk.mockk
import io.mockk.verify
import mozilla.components.concept.menu.candidate.TextStyle
import mozilla.components.concept.storage.BookmarkNodeType
import mozilla.components.support.ktx.android.content.getColorFromAttr
import mozilla.components.support.test.robolectric.testContext
import org.junit.Assert.assertEquals
import org.junit.Before
import org.junit.Test
import org.junit.runner.RunWith
import org.mozilla.fenix.R
import org.mozilla.fenix.helpers.FenixRobolectricTestRunner
import org.mozilla.fenix.library.bookmarks.BookmarkItemMenu.Item

@RunWith(FenixRobolectricTestRunner::class)
class BookmarkItemMenuTest {

    private lateinit var context: Context
    private lateinit var onItemTapped: (Item) -> Unit
    private lateinit var menu: BookmarkItemMenu

    @Before
    fun setup() {
        context = ContextThemeWrapper(testContext, R.style.NormalTheme)
        onItemTapped = mockk(relaxed = true)
        menu = BookmarkItemMenu(context, onItemTapped)
    }

    @Test
    fun `delete item has special styling`() {
        val deleteItem = menu.menuItems(BookmarkNodeType.SEPARATOR).last()
        assertEquals("Delete", deleteItem.text)
        assertEquals(
            TextStyle(color = context.getColorFromAttr(R.attr.destructive)),
            deleteItem.textStyle
        )

        deleteItem.onClick()
        verify { onItemTapped(Item.Delete) }
    }

    @Test
    fun `edit item appears for folders`() {
        val folderItems = menu.menuItems(BookmarkNodeType.FOLDER)
        assertEquals(2, folderItems.size)
        val (edit, delete) = folderItems

        assertEquals("Edit", edit.text)
        edit.onClick()
        verify { onItemTapped(Item.Edit) }

        assertEquals("Delete", delete.text)
    }

    @Test
    fun `all item appears for sites`() {
        val siteItems = menu.menuItems(BookmarkNodeType.ITEM)
        assertEquals(6, siteItems.size)
        val (edit, copy, share, openInNewTab, openInPrivateTab, delete) = siteItems

        assertEquals("Edit", edit.text)
        assertEquals("Copy", copy.text)
        assertEquals("Share", share.text)
        assertEquals("Open in new tab", openInNewTab.text)
        assertEquals("Open in private tab", openInPrivateTab.text)
        assertEquals("Delete", delete.text)

        edit.onClick()
        verify { onItemTapped(Item.Edit) }

        copy.onClick()
        verify { onItemTapped(Item.Copy) }

        share.onClick()
        verify { onItemTapped(Item.Share) }

        openInNewTab.onClick()
        verify { onItemTapped(Item.OpenInNewTab) }

        openInPrivateTab.onClick()
        verify { onItemTapped(Item.OpenInPrivateTab) }

        delete.onClick()
        verify { onItemTapped(Item.Delete) }
    }

    private operator fun <T> List<T>.component6(): T {
        return get(5)
    }
}
