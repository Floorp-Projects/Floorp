/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

package org.mozilla.fenix.home.sessioncontrol

import android.content.res.Resources
import android.graphics.Canvas
import android.graphics.drawable.Drawable
import androidx.core.content.ContextCompat
import androidx.recyclerview.widget.ItemTouchHelper
import androidx.recyclerview.widget.RecyclerView
import org.mozilla.fenix.R
import org.mozilla.fenix.ext.getColorFromAttr
import org.mozilla.fenix.home.sessioncontrol.viewholders.TabInCollectionViewHolder
import org.mozilla.fenix.home.sessioncontrol.viewholders.TabViewHolder

class SwipeToDeleteCallback(
    val interactor: SessionControlInteractor
) : ItemTouchHelper.SimpleCallback(0, ItemTouchHelper.LEFT or ItemTouchHelper.RIGHT) {
    override fun onMove(
        recyclerView: RecyclerView,
        viewHolder: RecyclerView.ViewHolder,
        target: RecyclerView.ViewHolder
    ): Boolean {
        // We don't support drag and drop so this method will never be called
        return false
    }

    override fun onSwiped(viewHolder: RecyclerView.ViewHolder, direction: Int) {
        when (viewHolder) {
            is TabViewHolder -> interactor.onCloseTab(viewHolder.tab?.sessionId!!)
            is TabInCollectionViewHolder -> {
                interactor.onCollectionRemoveTab(viewHolder.collection, viewHolder.tab)
            }
        }
    }

    override fun onChildDraw(
        c: Canvas,
        recyclerView: RecyclerView,
        viewHolder: RecyclerView.ViewHolder,
        dX: Float,
        dY: Float,
        actionState: Int,
        isCurrentlyActive: Boolean
    ) {
        super.onChildDraw(c, recyclerView, viewHolder, dX, dY, actionState, isCurrentlyActive)
        val icon = ContextCompat.getDrawable(recyclerView.context, R.drawable.ic_delete)
        icon?.setTint(recyclerView.context.getColorFromAttr(R.attr.destructive))

        val backgroundDrawable = when {
            viewHolder is TabInCollectionViewHolder && viewHolder.isLastTab -> {
                R.drawable.tab_in_collection_last_swipe_background
            }
            viewHolder is TabInCollectionViewHolder -> {
                R.drawable.tab_in_collection_swipe_background
            }
            else -> R.drawable.session_background
        }

        val background = ContextCompat.getDrawable(recyclerView.context, backgroundDrawable)
        background?.let {
            icon?.let {
                val itemView = viewHolder.itemView
                val iconLeft: Int
                val iconRight: Int
                val margin = convertDpToPixel(MARGIN.toFloat())
                val iconWidth = icon.intrinsicWidth
                val iconHeight = icon.intrinsicHeight
                val cellHeight = itemView.bottom - itemView.top
                val iconTop = itemView.top + (cellHeight - iconHeight) / 2
                val iconBottom = iconTop + iconHeight

                when {
                    dX > 0 -> { // Swiping to the right
                        iconLeft = itemView.left + margin
                        iconRight = itemView.left + margin + iconWidth
                        background.setBounds(
                            itemView.left, itemView.top,
                            (itemView.left + dX).toInt() + BACKGROUND_CORNER_OFFSET,
                            itemView.bottom
                        )
                        icon.setBounds(iconLeft, iconTop, iconRight, iconBottom)
                        draw(background, icon, c)
                    }
                    dX < 0 -> { // Swiping to the left
                        iconLeft = itemView.right - margin - iconWidth
                        iconRight = itemView.right - margin
                        background.setBounds(
                            (itemView.right + dX).toInt() - BACKGROUND_CORNER_OFFSET,
                            itemView.top, itemView.right, itemView.bottom
                        )
                        icon.setBounds(iconLeft, iconTop, iconRight, iconBottom)
                        draw(background, icon, c)
                    }
                    else -> { // View not swiped
                        background.setBounds(0, 0, 0, 0)
                        icon.setBounds(0, 0, 0, 0)
                    }
                }
            }
        }
    }

    override fun getSwipeDirs(
        recyclerView: RecyclerView,
        viewHolder: RecyclerView.ViewHolder
    ): Int {
        return if (recyclerView.hasWindowFocus() &&
            (viewHolder is TabViewHolder || viewHolder is TabInCollectionViewHolder)
        ) {
            super.getSwipeDirs(recyclerView, viewHolder)
        } else 0
    }

    companion object {
        const val BACKGROUND_CORNER_OFFSET = 40
        const val MARGIN = 32
        const val DENSITY_CONVERSION = 160f

        private fun draw(
            background: Drawable,
            icon: Drawable,
            c: Canvas
        ) {
            background.draw(c)
            icon.draw(c)
        }

        private fun convertDpToPixel(dp: Float): Int {
            val metrics = Resources.getSystem().displayMetrics
            val px = dp * (metrics.densityDpi / DENSITY_CONVERSION)
            return Math.round(px)
        }
    }
}
