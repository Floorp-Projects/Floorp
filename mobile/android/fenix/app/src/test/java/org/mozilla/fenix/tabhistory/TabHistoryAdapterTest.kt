/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

package org.mozilla.fenix.tabhistory

import android.text.Spanned
import android.widget.FrameLayout
import androidx.appcompat.view.ContextThemeWrapper
import androidx.core.text.HtmlCompat
import io.mockk.mockk
import kotlinx.android.synthetic.main.history_list_item.*
import mozilla.components.support.test.robolectric.testContext
import org.junit.Assert.assertEquals
import org.junit.Assert.assertFalse
import org.junit.Assert.assertTrue
import org.junit.Before
import org.junit.Test
import org.junit.runner.RunWith
import org.mozilla.fenix.R
import org.mozilla.fenix.helpers.FenixRobolectricTestRunner

@RunWith(FenixRobolectricTestRunner::class)
class TabHistoryAdapterTest {

    private lateinit var parent: FrameLayout
    private lateinit var interactor: TabHistoryInteractor
    private lateinit var adapter: TabHistoryAdapter

    private val selectedItem = TabHistoryItem(
        title = "Mozilla",
        url = "https://mozilla.org",
        index = 0,
        isSelected = true
    )
    private val unselectedItem = TabHistoryItem(
        title = "Firefox",
        url = "https://firefox.com",
        index = 1,
        isSelected = false
    )

    @Before
    fun setup() {
        parent = FrameLayout(ContextThemeWrapper(testContext, R.style.NormalTheme))
        interactor = mockk()
        adapter = TabHistoryAdapter(interactor)
    }

    @Test
    fun `creates and binds view holder`() {
        adapter.submitList(listOf(selectedItem, unselectedItem))

        val holder = adapter.createViewHolder(parent, 0)

        adapter.bindViewHolder(holder, 0)
        val htmlSelected = HtmlCompat.toHtml(holder.history_layout.titleView.text as Spanned, 0)
        assertTrue(htmlSelected, "<b>Mozilla</b>" in htmlSelected)

        adapter.bindViewHolder(holder, 1)
        assertFalse(holder.history_layout.titleView.text is Spanned)
        assertEquals("Firefox", holder.history_layout.titleView.text)
    }

    @Test
    fun `items are the same if they have matching URLs`() {
        assertTrue(TabHistoryAdapter.DiffCallback.areItemsTheSame(
            selectedItem,
            selectedItem
        ))
        assertTrue(TabHistoryAdapter.DiffCallback.areItemsTheSame(
            unselectedItem,
            unselectedItem.copy(title = "Waterbug", index = 2, isSelected = true)
        ))
        assertFalse(TabHistoryAdapter.DiffCallback.areItemsTheSame(
            unselectedItem,
            unselectedItem.copy(url = "https://firefox.com/subpage")
        ))
    }

    @Test
    fun `equal items have the same contents`() {
        assertTrue(TabHistoryAdapter.DiffCallback.areContentsTheSame(
            selectedItem,
            selectedItem
        ))
        assertFalse(TabHistoryAdapter.DiffCallback.areContentsTheSame(
            selectedItem,
            selectedItem.copy(title = "Waterbug", index = 2, isSelected = false)
        ))
        assertFalse(TabHistoryAdapter.DiffCallback.areContentsTheSame(
            unselectedItem,
            selectedItem
        ))
    }
}
