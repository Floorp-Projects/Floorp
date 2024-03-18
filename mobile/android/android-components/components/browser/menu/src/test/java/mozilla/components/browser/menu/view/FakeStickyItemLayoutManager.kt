/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

package mozilla.components.browser.menu.view

import android.content.Context
import android.view.View
import androidx.recyclerview.widget.RecyclerView

/**
 * A default implementation of a the abstract [StickyItemsLinearLayoutManager] to be used in tests.
 */
open class FakeStickyItemLayoutManager<T> constructor(
    context: Context,
    internal val stickyItemPlacement: StickyItemPlacement = StickyItemPlacement.TOP,
    reverseLayout: Boolean = false,
) : StickyItemsLinearLayoutManager<T>(
    context,
    stickyItemPlacement,
    reverseLayout,
) where T : RecyclerView.Adapter<*>, T : StickyItemsAdapter {
    override fun scrollToIndicatedPositionWithOffset(
        position: Int,
        offset: Int,
        actuallyScrollToPositionWithOffset: (Int, Int) -> Unit,
    ) { }

    override fun shouldStickyItemBeShownForCurrentPosition() = true

    override fun getY(itemView: View) = 0f
}
