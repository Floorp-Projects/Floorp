/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

package org.mozilla.fenix.sync

import android.content.Context
import android.graphics.Canvas
import android.graphics.Rect
import android.graphics.drawable.Drawable
import android.view.View
import androidx.recyclerview.widget.RecyclerView
import androidx.recyclerview.widget.RecyclerView.ItemDecoration
import mozilla.components.support.ktx.android.util.dpToPx
import org.mozilla.fenix.R
import org.mozilla.fenix.sync.SyncedTabsViewHolder.DeviceViewHolder

/**
 * Adds an [ItemDecoration] to the device name of each Synced Tab group.
 */
class SyncedTabsTitleDecoration(
    context: Context,
    private val style: Style = Style(
        height = 1.dpToPx(context.resources.displayMetrics),
        color = run {
            val a = context.obtainStyledAttributes(intArrayOf(R.attr.toolbarDivider))
            val color = a.getDrawable(0)!!
            a.recycle()
            color
        }
    )
) : ItemDecoration() {

    /**
     * A class for holding various customizations.
     */
    data class Style(val height: Int, val color: Drawable)

    override fun getItemOffsets(
        outRect: Rect,
        view: View,
        parent: RecyclerView,
        state: RecyclerView.State
    ) {
        val viewHolder = parent.getChildViewHolder(view)
        val position = viewHolder.bindingAdapterPosition
        val viewType = viewHolder.itemViewType

        // Only add offsets on the device title that is not the first.
        if (viewType == DeviceViewHolder.LAYOUT_ID && position != 0) {
            outRect.set(0, style.height, 0, 0)
            return
        }

        outRect.setEmpty()
    }

    override fun onDraw(c: Canvas, parent: RecyclerView, state: RecyclerView.State) {
        for (i in 0 until parent.childCount) {
            val view = parent.getChildAt(i)
            val viewHolder = parent.getChildViewHolder(view)
            val position = viewHolder.bindingAdapterPosition
            val viewType = viewHolder.itemViewType

            // Only draw on the device title that is not the first.
            if (viewType == DeviceViewHolder.LAYOUT_ID && position != 0) {
                style.color.setBounds(
                    view.left,
                    view.top - style.height,
                    view.right,
                    view.top
                )
                style.color.draw(c)
            }
        }
    }
}
