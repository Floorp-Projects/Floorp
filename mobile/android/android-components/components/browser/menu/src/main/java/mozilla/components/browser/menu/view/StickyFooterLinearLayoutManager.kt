/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

package mozilla.components.browser.menu.view

import android.content.Context
import android.view.View
import androidx.recyclerview.widget.RecyclerView

/**
 * Vertical LinearLayoutManager that will ensure an item at a position specified through
 * [StickyItemsAdapter.isStickyItem] will not scroll past the list bottom.
 *
 * The list would otherwise scroll normally with the other elements being scrolled beneath the sticky item.
 *
 * @param context [Context] needed for various Android interactions.
 * @param reverseLayout When set to true, layouts from end to start.
 */
open class StickyFooterLinearLayoutManager<T> constructor(
    context: Context,
    reverseLayout: Boolean = false,
) : StickyItemsLinearLayoutManager<T>(
    context,
    StickyItemPlacement.BOTTOM,
    reverseLayout,
) where T : RecyclerView.Adapter<*>, T : StickyItemsAdapter {

    override fun scrollToIndicatedPositionWithOffset(
        position: Int,
        offset: Int,
        actuallyScrollToPositionWithOffset: (Int, Int) -> Unit,
    ) {
        // The following scenarios are handled:
        // - if position is bigger than [stickyItemPosition]
        //      -> the default behavior will have the list scrolled downwards enough to show that.
        // - if position is the one of the stickyItem
        //      -> the default behavior will scroll to exactly the header. Perfect match.
        // - if position is before that of the [stickyItem] and does not fit the screen
        //      -> only scenario we need to handle: the sticky footer must be shown and the default implementation
        //      would scroll to show as the last item in the list the item at [position]. But that is where the sticky
        //      item is anchored. Need to scroll to the next position so that that item will be obscured by the sticky
        //      item and not the now above item at [position].
        //
        // Providing any offsets with the stickyView shown and the above scenarios handles means they are handled also.

        if (position < stickyItemPosition && getChildAt(position) == null) {
            actuallyScrollToPositionWithOffset(position + 1, offset)
            return
        }

        actuallyScrollToPositionWithOffset(position, offset)
    }

    override fun shouldStickyItemBeShownForCurrentPosition(): Boolean {
        if (stickyItemPosition == RecyclerView.NO_POSITION) {
            return false
        }

        // The item at [stickyItemPosition] should be anchored to the top if:
        // - it or a lower indexed item is shown at the bottom of the list
        // - the last shown item is translated downwards off screen
        // (happens when [scrollToPositionWithOffset] was called with a big enough offset)
        val lastVisibleElement = stickyItemView?.let { childCount - 2 } ?: childCount - 1
        return getAdapterPositionForItemIndex(lastVisibleElement) <= stickyItemPosition
    }

    override fun getY(itemView: View): Float {
        return when (reverseLayout) {
            true -> 0f
            false -> height - itemView.height.toFloat()
        }
    }
}
