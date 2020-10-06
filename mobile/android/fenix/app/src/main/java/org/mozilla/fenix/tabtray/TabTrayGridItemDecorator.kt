/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

package org.mozilla.fenix.tabtray

import android.content.Context
import android.graphics.Canvas
import android.graphics.Paint
import android.graphics.RectF
import android.view.View
import androidx.core.content.ContextCompat
import androidx.recyclerview.widget.RecyclerView
import org.mozilla.fenix.R
import org.mozilla.fenix.ext.components

/**
 * An item decorator for the tab tray grid tabs. Draws a border for the selected tab.
 */
class TabTrayGridItemDecorator(context: Context) :
    RecyclerView.ItemDecoration() {

    override fun onDrawOver(canvas: Canvas, recyclerView: RecyclerView, state: RecyclerView.State) {
        for (i in 0 until recyclerView.childCount) {
            val child = recyclerView.getChildAt(i)
            val viewHolder = recyclerView.getChildViewHolder(child)
            if (viewHolder !is TabTrayViewHolder) {
                continue
            }

            if (viewHolder.tab?.id == recyclerView.context.components.core.store.state.selectedTabId) {
                drawSelectedTabBorder(child, canvas)
            }
        }

        super.onDrawOver(canvas, recyclerView, state)
    }

    private fun drawSelectedTabBorder(child: View, canvas: Canvas) {
        val borderRadius =
            child.resources.getDimensionPixelSize(R.dimen.tab_tray_grid_item_border_radius)
                .toFloat()
        canvas.drawRoundRect(child.getBounds(), borderRadius, borderRadius, borderStroke)
    }

    private val borderStroke: Paint = Paint().apply {
        color = ContextCompat.getColor(context, R.color.photonViolet50)
        isAntiAlias = true
        strokeWidth =
            context.resources.getDimensionPixelSize(R.dimen.tab_tray_grid_item_selected_border_width)
                .toFloat()
        style = Paint.Style.STROKE
    }

    private fun View.getBounds(): RectF {
        val leftPosition = left + translationX - paddingLeft
        val rightPosition = right + translationX + paddingRight
        val topPosition = top + translationY - paddingTop
        val bottomPosition = bottom + translationY + paddingBottom
        return RectF(leftPosition, topPosition, rightPosition, bottomPosition)
    }
}
