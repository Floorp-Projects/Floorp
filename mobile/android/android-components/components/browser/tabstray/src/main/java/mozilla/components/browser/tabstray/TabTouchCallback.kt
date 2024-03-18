/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

package mozilla.components.browser.tabstray

import android.graphics.Canvas
import androidx.recyclerview.widget.ItemTouchHelper
import androidx.recyclerview.widget.RecyclerView
import mozilla.components.browser.state.state.TabSessionState

/**
 * An [ItemTouchHelper.Callback] for support gestures on tabs in the tray.
 *
 * @param onRemoveTab A callback invoked when a tab is removed.
 */
open class TabTouchCallback(
    private val onRemoveTab: (TabSessionState) -> Unit,
) : ItemTouchHelper.SimpleCallback(0, ItemTouchHelper.LEFT or ItemTouchHelper.RIGHT) {

    override fun onSwiped(viewHolder: RecyclerView.ViewHolder, direction: Int) {
        with(viewHolder as TabViewHolder) {
            tab?.let { onRemoveTab(it) }
        }
    }

    override fun onChildDraw(
        c: Canvas,
        recyclerView: RecyclerView,
        viewHolder: RecyclerView.ViewHolder,
        dX: Float,
        dY: Float,
        actionState: Int,
        isCurrentlyActive: Boolean,
    ) {
        if (actionState == ItemTouchHelper.ACTION_STATE_SWIPE) {
            // Alpha on an itemView being swiped should decrease to a min over a distance equal to
            // the width of the item being swiped.
            viewHolder.itemView.alpha = alphaForItemSwipe(dX, viewHolder.itemView.width)
        }

        super.onChildDraw(c, recyclerView, viewHolder, dX, dY, actionState, isCurrentlyActive)
    }

    /**
     * Sets the alpha value for a swipe gesture. This is useful for inherited classes to provide their own values.
     */
    open fun alphaForItemSwipe(dX: Float, distanceToAlphaMin: Int): Float {
        return 1f
    }

    override fun onMove(p0: RecyclerView, p1: RecyclerView.ViewHolder, p2: RecyclerView.ViewHolder): Boolean = false
}
