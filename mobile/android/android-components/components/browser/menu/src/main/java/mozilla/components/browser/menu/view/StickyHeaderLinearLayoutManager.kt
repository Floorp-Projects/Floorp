/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

package mozilla.components.browser.menu.view

import android.content.Context
import android.view.View
import androidx.recyclerview.widget.RecyclerView

/**
 * Vertical LinearLayoutManager that will ensure an item at a position specified through
 * [StickyItemsAdapter.isStickyItem] will not scroll past the list's top.
 *
 * The list would otherwise scroll normally with the other elements being scrolled beneath the sticky item.
 *
 * @param context [Context] needed for various Android interactions.
 * @param reverseLayout When set to true, layouts from end to start.
 */
open class StickyHeaderLinearLayoutManager<T> constructor(
    context: Context,
    reverseLayout: Boolean = false,
) : StickyItemsLinearLayoutManager<T>(
    context,
    StickyItemPlacement.TOP,
    reverseLayout,
) where T : RecyclerView.Adapter<*>, T : StickyItemsAdapter {

    override fun scrollToIndicatedPositionWithOffset(
        position: Int,
        offset: Int,
        actuallyScrollToPositionWithOffset: (Int, Int) -> Unit,
    ) {
        // The following scenarios are handled:
        // - if position is smaller than [stickyItemPosition]
        //      -> the default behavior will have the list scrolled upwards enough to show that.
        // - if position is the one of the stickyItem
        //      -> the default behavior will scroll to exactly the header. Perfect match.
        // - if position is bigger than [stickyItemPosition]
        //      -> only scenario we need to handle: default implementation would scroll to show at the top of the list
        //      the item at that position. But that is where the sticky item is anchored. Need to ask for the item at
        //      the before position being shown at the top of the list and let that be obscured by the sticky item.
        //
        // Providing any offsets with the stickyView shown and the above scenarios handles means they are handled also.

        if (position + 1 > stickyItemPosition) {
            actuallyScrollToPositionWithOffset(position - 1, offset)
            return
        }

        actuallyScrollToPositionWithOffset(position, offset)
    }

    override fun shouldStickyItemBeShownForCurrentPosition(): Boolean {
        if (stickyItemPosition == RecyclerView.NO_POSITION) {
            return false
        }

        // The item at [stickyItemPosition] should be anchored to the top if:
        // - it or a below item is shown at the top of the list
        // - the first shown item is translated upwards off screen
        // (happens when [scrollToPositionWithOffset] was called with a big enough offset)
        return getAdapterPositionForItemIndex(0) >= stickyItemPosition ||
            getChildAt(0)?.bottom ?: 1 <= 0 // return false if there is no item at index 0
    }

    override fun getY(itemView: View): Float {
        return when (reverseLayout) {
            true -> height - itemView.height.toFloat()
            false -> 0f
        }
    }
}
