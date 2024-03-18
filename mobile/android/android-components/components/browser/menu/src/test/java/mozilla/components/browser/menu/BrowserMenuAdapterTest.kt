/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

package mozilla.components.browser.menu

import android.graphics.Color
import android.graphics.drawable.ColorDrawable
import android.view.View
import androidx.recyclerview.widget.RecyclerView
import androidx.test.ext.junit.runners.AndroidJUnit4
import mozilla.components.support.test.mock
import mozilla.components.support.test.robolectric.testContext
import org.junit.Assert.assertEquals
import org.junit.Assert.assertFalse
import org.junit.Assert.assertTrue
import org.junit.Assert.fail
import org.junit.Test
import org.junit.runner.RunWith
import org.mockito.Mockito.mock
import org.mockito.Mockito.never
import org.mockito.Mockito.reset
import org.mockito.Mockito.spy
import org.mockito.Mockito.verify
import org.mockito.Mockito.`when`

@RunWith(AndroidJUnit4::class)
class BrowserMenuAdapterTest {

    @Test
    fun `items that return false from the visible lambda will be filtered out`() {
        val items = listOf(
            createMenuItem(1, { true }),
            createMenuItem(2, { true }),
            createMenuItem(3, { false }),
            createMenuItem(4, { false }),
            createMenuItem(5, { true }),
        )

        val adapter = BrowserMenuAdapter(testContext, items)

        assertEquals(3, adapter.visibleItems.size)

        adapter.visibleItems.assertTrueForOne { it.getLayoutResource() == 1 }
        adapter.visibleItems.assertTrueForOne { it.getLayoutResource() == 2 }
        adapter.visibleItems.assertTrueForOne { it.getLayoutResource() == 5 }

        adapter.visibleItems.assertTrueForAll { it.visible() }

        assertEquals(3, adapter.itemCount)
    }

    @Test
    fun `layout resource ID is used as view type`() {
        val items = listOf(
            createMenuItem(23),
            createMenuItem(42),
        )

        val adapter = BrowserMenuAdapter(testContext, items)

        assertEquals(2, adapter.itemCount)

        assertEquals(23, adapter.getItemViewType(0))
        assertEquals(42, adapter.getItemViewType(1))
    }

    @Test
    fun `bind will be forwarded to item implementation`() {
        val item1 = spy(createMenuItem())
        val item2 = spy(createMenuItem())

        val menu = mock(BrowserMenu::class.java)

        val adapter = BrowserMenuAdapter(testContext, listOf(item1, item2))
        adapter.menu = menu

        val view = mock(View::class.java)
        val holder = BrowserMenuItemViewHolder(view)

        adapter.onBindViewHolder(holder, 0)

        verify(item1).bind(menu, view)
        verify(item2, never()).bind(menu, view)

        reset(item1)
        reset(item2)

        adapter.onBindViewHolder(holder, 1)

        verify(item1, never()).bind(menu, view)
        verify(item2).bind(menu, view)
    }

    @Test
    fun `invalidate will be forwarded to item implementation`() {
        val item1 = spy(createMenuItem())
        val item2 = spy(createMenuItem())

        val menu = mock(BrowserMenu::class.java)

        val adapter = BrowserMenuAdapter(testContext, listOf(item1, item2))
        adapter.menu = menu
        val recyclerView = mock(RecyclerView::class.java)

        val view = mock(View::class.java)
        val holder = BrowserMenuItemViewHolder(view)
        `when`(recyclerView.findViewHolderForAdapterPosition(0)).thenReturn(holder)
        `when`(recyclerView.findViewHolderForAdapterPosition(1)).thenReturn(null)

        adapter.invalidate(recyclerView)

        verify(item1).invalidate(view)
        verify(item2, never()).invalidate(view)
    }

    @Test
    fun `total interactive item count is given provided adapter`() {
        val items = listOf(
            createMenuItem(1, { true }, { 1 }),
            createMenuItem(2, { true }, { 0 }),
            createMenuItem(3, { false }, { 10 }),
            createMenuItem(4, { true }, { 5 }),
        )

        val adapter = BrowserMenuAdapter(testContext, items)

        assertEquals(6, adapter.interactiveCount)
    }

    @Test
    fun `GIVEN a stickyItem exists in the visible items WHEN isStickyItem is called THEN it returns true`() {
        val items = listOf(
            createMenuItem(1, { true }, { 1 }),
            createMenuItem(3, { true }, { 10 }, true),
            createMenuItem(4, { true }, { 5 }),
            createMenuItem(3, { false }, { 3 }, true),
        )

        val adapter = BrowserMenuAdapter(testContext, items)

        assertFalse(adapter.isStickyItem(0))
        assertTrue(adapter.isStickyItem(1))
        assertFalse(adapter.isStickyItem(2))
        assertFalse(adapter.isStickyItem(3))
        assertFalse(adapter.isStickyItem(4))
    }

    @Test
    fun `GIVEN a BrowserMenu exists WHEN setupStickyItem is called THEN the item background color is set for the View parameter`() {
        val adapter = BrowserMenuAdapter(testContext, emptyList())
        val menu: BrowserMenu = mock()
        menu.backgroundColor = Color.CYAN
        adapter.menu = menu
        val view = View(testContext)

        adapter.setupStickyItem(view)

        assertEquals(menu.backgroundColor, (view.background as ColorDrawable).color)
    }

    @Test
    fun `GIVEN BrowserMenuAdapter WHEN tearDownStickyItem is called THEN the item background is reset to transparent`() {
        val adapter = BrowserMenuAdapter(testContext, emptyList())
        val view = View(testContext)
        view.setBackgroundColor(Color.CYAN)

        adapter.tearDownStickyItem(view)

        assertEquals(Color.TRANSPARENT, (view.background as ColorDrawable).color)
    }

    private fun List<BrowserMenuItem>.assertTrueForOne(predicate: (BrowserMenuItem) -> Boolean) {
        for (item in this) {
            if (predicate(item)) {
                return
            }
        }
        fail("Predicate false for all items")
    }

    private fun List<BrowserMenuItem>.assertTrueForAll(predicate: (BrowserMenuItem) -> Boolean) {
        for (item in this) {
            if (!predicate(item)) {
                fail("Predicate not true for all items")
            }
        }
    }

    private fun createMenuItem(
        layout: Int = 0,
        visible: () -> Boolean = { true },
        interactiveCount: () -> Int = { 1 },
        isSticky: Boolean = false,
    ): BrowserMenuItem {
        return object : BrowserMenuItem {
            override val visible = visible

            override val interactiveCount = interactiveCount

            override fun getLayoutResource() = layout

            override fun bind(menu: BrowserMenu, view: View) {}

            override fun invalidate(view: View) {}

            override val isSticky = isSticky
        }
    }
}
