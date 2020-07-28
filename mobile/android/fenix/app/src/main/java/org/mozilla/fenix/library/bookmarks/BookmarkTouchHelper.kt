/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

package org.mozilla.fenix.library.bookmarks

import android.graphics.Canvas
import android.graphics.Rect
import android.graphics.drawable.Drawable
import androidx.appcompat.content.res.AppCompatResources
import androidx.recyclerview.widget.ItemTouchHelper
import androidx.recyclerview.widget.RecyclerView
import mozilla.components.concept.storage.BookmarkNodeType
import mozilla.components.support.ktx.android.content.getColorFromAttr
import mozilla.components.support.ktx.android.content.getDrawableWithTint
import mozilla.components.support.ktx.android.util.dpToPx
import org.mozilla.fenix.R
import org.mozilla.fenix.home.sessioncontrol.SwipeToDeleteCallback
import org.mozilla.fenix.library.bookmarks.viewholders.BookmarkNodeViewHolder
import org.mozilla.fenix.library.bookmarks.viewholders.BookmarkSeparatorViewHolder

class BookmarkTouchHelper(interactor: BookmarkViewInteractor) :
    ItemTouchHelper(BookmarkTouchCallback(interactor))

class BookmarkTouchCallback(
    private val interactor: BookmarkViewInteractor
) : ItemTouchHelper.SimpleCallback(0, ItemTouchHelper.LEFT or ItemTouchHelper.RIGHT) {

    override fun getSwipeDirs(
        recyclerView: RecyclerView,
        viewHolder: RecyclerView.ViewHolder
    ): Int {
        val item = (viewHolder as BookmarkNodeViewHolder).item
        return if (viewHolder is BookmarkSeparatorViewHolder || item?.inRoots() == true) {
            0
        } else {
            super.getSwipeDirs(recyclerView, viewHolder)
        }
    }

    /**
     * Delete the bookmark when swiped.
     */
    override fun onSwiped(viewHolder: RecyclerView.ViewHolder, direction: Int) {
        val item = (viewHolder as BookmarkNodeViewHolder).item
        item?.let {
            interactor.onDelete(setOf(it))
            // We need to notify the adapter of a change if we swipe a folder to prevent
            // visual bugs when cancelling deletion of a folder
            if (item.type == BookmarkNodeType.FOLDER) {
                viewHolder.bindingAdapter?.notifyItemChanged(viewHolder.bindingAdapterPosition)
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
        val icon = recyclerView.context.getDrawableWithTint(
            R.drawable.ic_delete,
            recyclerView.context.getColorFromAttr(R.attr.destructive)
        )!!
        val background = AppCompatResources.getDrawable(
            recyclerView.context,
            R.drawable.swipe_delete_background
        )!!
        val margin =
            SwipeToDeleteCallback.MARGIN.dpToPx(recyclerView.context.resources.displayMetrics)
        val cellHeight = viewHolder.itemView.bottom - viewHolder.itemView.top
        val iconTop = viewHolder.itemView.top + (cellHeight - icon.intrinsicHeight) / 2
        val iconBottom = iconTop + icon.intrinsicHeight

        when {
            dX > 0 -> { // Swiping to the right
                val backgroundBounds = Rect(
                    viewHolder.itemView.left, viewHolder.itemView.top,
                    (viewHolder.itemView.left + dX).toInt() + SwipeToDeleteCallback.BACKGROUND_CORNER_OFFSET,
                    viewHolder.itemView.bottom
                )
                val iconLeft = viewHolder.itemView.left + margin
                val iconRight = viewHolder.itemView.left + margin + icon.intrinsicWidth
                val iconBounds = Rect(iconLeft, iconTop, iconRight, iconBottom)

                setBounds(background, backgroundBounds, icon, iconBounds)
                draw(background, icon, c)
            }
            dX < 0 -> { // Swiping to the left
                val backgroundBounds = Rect(
                    (viewHolder.itemView.right + dX).toInt() - SwipeToDeleteCallback.BACKGROUND_CORNER_OFFSET,
                    viewHolder.itemView.top, viewHolder.itemView.right, viewHolder.itemView.bottom
                )
                val iconLeft = viewHolder.itemView.right - margin - icon.intrinsicWidth
                val iconRight = viewHolder.itemView.right - margin
                val iconBounds = Rect(iconLeft, iconTop, iconRight, iconBottom)

                setBounds(background, backgroundBounds, icon, iconBounds)
                draw(background, icon, c)
            }
            else -> { // View not swiped
                val bounds = Rect(0, 0, 0, 0)
                setBounds(background, bounds, icon, bounds)
            }
        }
    }

    override fun onMove(
        recyclerView: RecyclerView,
        viewHolder: RecyclerView.ViewHolder,
        target: RecyclerView.ViewHolder
    ): Boolean = false

    private fun setBounds(
        background: Drawable,
        backgroundBounds: Rect,
        icon: Drawable,
        iconBounds: Rect
    ) {
        background.bounds = backgroundBounds
        icon.bounds = iconBounds
    }

    private fun draw(background: Drawable, icon: Drawable, c: Canvas) {
        background.draw(c)
        icon.draw(c)
    }
}
