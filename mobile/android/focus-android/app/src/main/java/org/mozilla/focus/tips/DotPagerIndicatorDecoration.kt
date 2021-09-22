/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */
package org.mozilla.focus.tips

import android.content.Context
import android.content.res.Resources
import android.graphics.Canvas
import android.graphics.Paint
import android.graphics.Rect
import android.view.View
import android.view.animation.AccelerateDecelerateInterpolator
import android.view.animation.Interpolator
import androidx.core.content.ContextCompat
import androidx.recyclerview.widget.LinearLayoutManager
import androidx.recyclerview.widget.RecyclerView
import org.mozilla.focus.R
import kotlin.math.max

private const val HEIGHT = 6
private const val STROKE_WIDTH = 3
private const val ITEM_LENGTH = 3
private const val ITEM_PADDING = 12

class DotPagerIndicatorDecoration : RecyclerView.ItemDecoration() {
    private val dp = Resources.getSystem().displayMetrics.density

    // Height of the space the indicator takes up at the bottom of the view.
    private val indicatorHeightInDp = HEIGHT * dp

    private val indicatorStrokeWidthInDp = STROKE_WIDTH * dp
    private val indicatorItemLengthInDp = ITEM_LENGTH * dp
    private val indicatorItemPaddingInDp = ITEM_PADDING * dp

    private val interpolator: Interpolator = AccelerateDecelerateInterpolator()
    private val paint = Paint()

    init {
        paint.strokeWidth = indicatorStrokeWidthInDp
        paint.style = Paint.Style.STROKE
        paint.isAntiAlias = true
    }

    override fun getItemOffsets(
        outRect: Rect,
        view: View,
        parent: RecyclerView,
        state: RecyclerView.State
    ) {
        super.getItemOffsets(outRect, view, parent, state)
        outRect.bottom = indicatorHeightInDp.toInt()
    }

    override fun onDrawOver(canvas: Canvas, parent: RecyclerView, state: RecyclerView.State) {
        super.onDrawOver(canvas, parent, state)

        val itemCount = parent.adapter?.itemCount ?: 0

        // center horizontally, calculate width and subtract half from center
        val totalLength: Float = indicatorItemLengthInDp * itemCount
        val paddingBetweenItems: Float =
            max(0, itemCount - 1).toFloat() * indicatorItemPaddingInDp
        val indicatorTotalWidth = totalLength + paddingBetweenItems
        val indicatorStartX = (parent.width - indicatorTotalWidth) / 2F

        // center vertically in the allotted space
        val indicatorPosY: Float = parent.height - indicatorHeightInDp

        drawInactiveIndicators(canvas, parent.context, indicatorStartX, indicatorPosY, itemCount)

        highlightActiveItem(canvas, parent, indicatorStartX, indicatorPosY)
    }

    private fun highlightActiveItem(
        canvas: Canvas,
        parent: RecyclerView,
        indicatorStartX: Float,
        indicatorPosY: Float
    ) {
        val layoutManager = parent.layoutManager as LinearLayoutManager
        val activePosition = layoutManager.findFirstVisibleItemPosition()
        if (activePosition == RecyclerView.NO_POSITION) {
            return
        }

        // find offset of active item (if the user is scrolling)
        val activeChild = layoutManager.findViewByPosition(activePosition)
        val left = activeChild?.left ?: 0
        val width = activeChild?.width ?: 0

        // on swipe the active item will be positioned from [-width, 0]
        // interpolate offset for smooth animation
        val progress: Float = interpolator.getInterpolation(left * -1 / width.toFloat())
        drawHighlights(
            canvas,
            parent.context,
            indicatorStartX,
            indicatorPosY,
            activePosition,
            progress
        )
    }

    private fun drawInactiveIndicators(
        canvas: Canvas,
        context: Context,
        indicatorStartX: Float,
        indicatorPosY: Float,
        itemCount: Int
    ) {
        paint.color = ContextCompat.getColor(context, R.color.inactive_dot_background)
        val itemWidth = indicatorItemLengthInDp + indicatorItemPaddingInDp
        var start = indicatorStartX

        for (i in 0 until itemCount) {
            canvas.drawCircle(start, indicatorPosY, indicatorItemLengthInDp / 2F, paint)
            start += itemWidth
        }
    }

    @Suppress("LongParameterList")
    private fun drawHighlights(
        canvas: Canvas,
        context: Context,
        indicatorStartX: Float,
        indicatorPosY: Float,
        highlightPosition: Int,
        progress: Float
    ) {
        paint.color = ContextCompat.getColor(context, R.color.active_dot_background)
        val itemWidth = indicatorItemLengthInDp + indicatorItemPaddingInDp

        if (progress == 0f) {
            // no swipe, draw a normal indicator
            val highlightStart = indicatorStartX + itemWidth * highlightPosition
            canvas.drawCircle(
                highlightStart,
                indicatorPosY,
                indicatorItemLengthInDp / 2F,
                paint
            )
        } else {
            val highlightStart = indicatorStartX + itemWidth * highlightPosition
            canvas.drawCircle(
                highlightStart,
                indicatorPosY,
                indicatorItemLengthInDp / 2F,
                paint
            )
        }
    }
}
