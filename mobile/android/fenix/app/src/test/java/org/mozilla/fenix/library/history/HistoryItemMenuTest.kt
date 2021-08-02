/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

package org.mozilla.fenix.library.history

import android.content.Context
import androidx.appcompat.view.ContextThemeWrapper
import mozilla.components.concept.menu.candidate.TextStyle
import mozilla.components.support.ktx.android.content.getColorFromAttr
import mozilla.components.support.test.robolectric.testContext
import org.junit.Assert.assertEquals
import org.junit.Before
import org.junit.Test
import org.junit.runner.RunWith
import org.mozilla.fenix.R
import org.mozilla.fenix.helpers.FenixRobolectricTestRunner
import org.mozilla.fenix.library.history.HistoryItemMenu.Item

@RunWith(FenixRobolectricTestRunner::class)
class HistoryItemMenuTest {

    private lateinit var context: Context
    private lateinit var menu: HistoryItemMenu
    private var onItemTappedCaptured: Item? = null

    @Before
    fun setup() {
        context = ContextThemeWrapper(testContext, R.style.NormalTheme)
        onItemTappedCaptured = null
        menu = HistoryItemMenu(context) {
            onItemTappedCaptured = it
        }
    }

    @Test
    fun `delete item has special styling`() {
        val deleteItem = menu.menuItems().last()
        assertEquals("Delete", deleteItem.text)
        assertEquals(
            TextStyle(color = context.getColorFromAttr(R.attr.destructive)),
            deleteItem.textStyle
        )

        deleteItem.onClick()
        assertEquals(Item.Delete, onItemTappedCaptured)
    }

    @Test
    fun `builds menu items`() {
        val items = menu.menuItems()
        assertEquals(5, items.size)
        val (copy, share, openInNewTab, openInPrivateTab, delete) = items

        assertEquals("Copy", copy.text)
        assertEquals("Share", share.text)
        assertEquals("Open in new tab", openInNewTab.text)
        assertEquals("Open in private tab", openInPrivateTab.text)
        assertEquals("Delete", delete.text)

        copy.onClick()
        assertEquals(Item.Copy, onItemTappedCaptured)

        share.onClick()
        assertEquals(Item.Share, onItemTappedCaptured)

        openInNewTab.onClick()
        assertEquals(Item.OpenInNewTab, onItemTappedCaptured)

        openInPrivateTab.onClick()
        assertEquals(Item.OpenInPrivateTab, onItemTappedCaptured)

        delete.onClick()
        assertEquals(Item.Delete, onItemTappedCaptured)
    }
}
