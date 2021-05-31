/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

package org.mozilla.fenix.sync

import android.graphics.Canvas
import android.graphics.Rect
import android.graphics.drawable.Drawable
import android.view.View
import androidx.recyclerview.widget.RecyclerView
import io.mockk.Called
import io.mockk.every
import io.mockk.mockk
import io.mockk.verify
import org.junit.Before
import org.junit.Test
import org.mozilla.fenix.sync.SyncedTabsTitleDecoration.Style
import org.mozilla.fenix.sync.SyncedTabsViewHolder.DeviceViewHolder
import org.mozilla.fenix.sync.SyncedTabsViewHolder.ErrorViewHolder

class SyncedTabsTitleDecorationTest {

    private val recyclerView: RecyclerView = mockk(relaxed = true)
    private val canvas: Canvas = mockk(relaxed = true)
    private val viewHolder: RecyclerView.ViewHolder = mockk(relaxed = true)
    private val state: RecyclerView.State = mockk(relaxed = true)
    private val view: View = mockk(relaxed = true)

    // Mocking these classes so we don't have to use the (slow) Android test runner.
    private val rect: Rect = mockk(relaxed = true)
    private val colorDrawable: Drawable = mockk(relaxed = true)

    private val style = Style(5, colorDrawable)

    @Before
    fun setup() {
        every { recyclerView.getChildViewHolder(any()) }.returns(viewHolder)
        every { recyclerView.childCount }.returns(1)
        every { recyclerView.getChildAt(any()) }.returns(view)
        every { view.left }.returns(5)
        every { view.top }.returns(5)
        every { view.right }.returns(5)
        every { view.bottom }.returns(5)
    }

    @Test
    fun `WHEN device title and not first item THEN add offset to the layout rect`() {
        val decoration = SyncedTabsTitleDecoration(mockk(), style)

        every { viewHolder.itemViewType }.answers { DeviceViewHolder.LAYOUT_ID }
        every { viewHolder.bindingAdapterPosition }.answers { 1 }

        decoration.getItemOffsets(rect, mockk(), recyclerView, state)

        verify { rect.set(0, 5, 0, 0) }
    }

    @Test
    fun `WHEN not device title and first position THEN do not add offsets`() {
        val decoration = SyncedTabsTitleDecoration(mockk(), style)

        every { viewHolder.itemViewType }.answers { ErrorViewHolder.LAYOUT_ID }
        every { viewHolder.bindingAdapterPosition }
            .answers { 1 }
            .andThen { 0 }

        decoration.getItemOffsets(rect, mockk(), recyclerView, state)

        decoration.getItemOffsets(rect, mockk(), recyclerView, state)

        verify(exactly = 2) { rect.setEmpty() }
    }

    @Test
    fun `WHEN device title and not first THEN draw`() {
        val decoration = SyncedTabsTitleDecoration(mockk(), style)

        every { viewHolder.itemViewType }.answers { DeviceViewHolder.LAYOUT_ID }
        every { viewHolder.bindingAdapterPosition }.answers { 1 }

        decoration.onDraw(canvas, recyclerView, state)

        verify { colorDrawable.setBounds(5, 0, 5, 5) }
        verify { colorDrawable.draw(canvas) }
    }

    @Test
    fun `WHEN not device title and not first THEN do not draw`() {
        val decoration = SyncedTabsTitleDecoration(mockk(), style)

        every { viewHolder.itemViewType }.answers { ErrorViewHolder.LAYOUT_ID }
        every { viewHolder.bindingAdapterPosition }
            .answers { 1 }
            .andThen { 0 }

        decoration.onDraw(canvas, recyclerView, state)

        verify { colorDrawable wasNot Called }
    }
}
