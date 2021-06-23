/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

package org.mozilla.fenix.home.recenttabs.view

import android.view.View
import android.view.ViewGroup
import androidx.appcompat.content.res.AppCompatResources
import mozilla.components.support.ktx.android.content.getColorFromAttr
import mozilla.components.support.ktx.android.util.dpToPx
import org.mozilla.fenix.R

private const val TOP_MARGIN_DP = 1

/**
 * All possible positions of a recent tab in relation to others when shown in the "Jump back in" section.
 */
enum class RecentTabsItemPosition {
    /**
     * This is the only tab to be shown in this section.
     */
    SINGLE,

    /**
     * This item is to be shown at the top of the section with others below it.
     */
    TOP,

    /**
     * This item is to be shown between others in this section.
     */
    MIDDLE,

    /**
     * This item is to be shown at the bottom of the section with others above it.
     */
    BOTTOM
}

/**
 * Helpers for setting various layout properties for the view from a [RecentTabViewHolder].
 *
 * Depending on the provided [RecentTabsItemPosition]:
 * - sets a different background so that the entire section possibly containing
 * more such items would have rounded corners but sibling items not.
 * - sets small margins for the items so that there's a clear separation between siblings
 */
sealed class RecentTabViewDecorator {
    /**
     * Apply the decoration to [itemView].
     */
    abstract operator fun invoke(itemView: View): View

    companion object {
        /**
         * Get the appropriate decorator to set view background / margins depending on the position
         * of that view in the recent tabs section.
         */
        fun forPosition(position: RecentTabsItemPosition) = when (position) {
            RecentTabsItemPosition.SINGLE -> SingleTabDecoration
            RecentTabsItemPosition.TOP -> TopTabDecoration
            RecentTabsItemPosition.MIDDLE -> MiddleTabDecoration
            RecentTabsItemPosition.BOTTOM -> BottomTabDecoration
        }
    }

    /**
     * Decorator for a view shown in the recent tabs section that will update it to express
     * that that item is the single one shown in this section.
     */
    object SingleTabDecoration : RecentTabViewDecorator() {
        override fun invoke(itemView: View): View {
            val context = itemView.context

            itemView.background =
                AppCompatResources.getDrawable(context, R.drawable.home_list_row_background)

            return itemView
        }
    }

    /**
     * Decorator for a view shown in the recent tabs section that will update it to express
     * that this is an item shown at the top of the section and there are others below it.
     */
    object TopTabDecoration : RecentTabViewDecorator() {
        override fun invoke(itemView: View): View {
            val context = itemView.context

            itemView.background =
                AppCompatResources.getDrawable(context, R.drawable.rounded_top_corners)

            return itemView
        }
    }

    /**
     * Decorator for a view shown in the recent tabs section that will update it to express
     * that this is an item shown has other recents tabs to be shown on top or below it.
     */
    object MiddleTabDecoration : RecentTabViewDecorator() {
        override fun invoke(itemView: View): View {
            val context = itemView.context

            itemView.setBackgroundColor(context.getColorFromAttr(R.attr.above))

            (itemView.layoutParams as? ViewGroup.MarginLayoutParams)?.topMargin =
                TOP_MARGIN_DP.dpToPx(context.resources.displayMetrics)

            return itemView
        }
    }

    /**
     * Decorator for a view shown in the recent tabs section that will update it to express
     * that this is an item shown at the bottom of the section and there are others above it.
     */
    object BottomTabDecoration : RecentTabViewDecorator() {
        override fun invoke(itemView: View): View {
            val context = itemView.context

            itemView.background =
                AppCompatResources.getDrawable(context, R.drawable.rounded_bottom_corners)

            (itemView.layoutParams as? ViewGroup.MarginLayoutParams)?.topMargin =
                TOP_MARGIN_DP.dpToPx(context.resources.displayMetrics)

            return itemView
        }
    }
}
