/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

package mozilla.components.browser.menu.view

import android.annotation.SuppressLint
import android.content.Context
import android.util.AttributeSet
import androidx.annotation.Px
import androidx.annotation.VisibleForTesting
import androidx.recyclerview.widget.RecyclerView
import mozilla.components.browser.menu.R

/**
 * [RecyclerView] with automatically set width between widthMin / widthMax xml attributes.
 */
class DynamicWidthRecyclerView @JvmOverloads constructor(
    context: Context,
    attrs: AttributeSet? = null,
) : RecyclerView(context, attrs) {
    @VisibleForTesting
    @Px
    internal var maxWidthOfAllChildren: Int = 0
        set(value) {
            if (field == 0) field = value
        }

    @Px var minWidth: Int = -1

    @Px var maxWidth: Int = -1

    @VisibleForTesting(otherwise = VisibleForTesting.PROTECTED)
    override fun onMeasure(widthSpec: Int, heightSpec: Int) {
        if (minWidth in 1 until maxWidth) {
            // Ignore any bounds set in xml. Allow for children to expand entirely.
            callParentOnMeasure(MeasureSpec.makeMeasureSpec(0, MeasureSpec.UNSPECIFIED), heightSpec)

            // First measure will report the width/height for the entire list
            // The first layout pass will actually remove child views that do not fit the screen
            // so future onMeasure calls will report skewed values.
            maxWidthOfAllChildren = measuredWidth

            // Children now have "unspecified" width. Let's set some bounds.
            setReconciledDimensions(maxWidthOfAllChildren, measuredHeight)
        } else {
            // Default behavior. layout_width / layout_height properties will be used for measuring.
            callParentOnMeasure(widthSpec, heightSpec)
        }
    }

    @VisibleForTesting
    internal fun setReconciledDimensions(
        desiredWidth: Int,
        desiredHeight: Int,
    ) {
        val minimumTapArea = resources.getDimensionPixelSize(R.dimen.mozac_browser_menu_material_min_tap_area)
        val minimumItemWidth = resources.getDimensionPixelSize(R.dimen.mozac_browser_menu_material_min_item_width)

        val reconciledWidth = desiredWidth
            .coerceAtLeast(minWidth)
            // Follow material guidelines where the minimum width is 112dp.
            .coerceAtLeast(minimumItemWidth)
            .coerceAtMost(maxWidth)
            // Leave at least 48dp as a tappable “exit area” available whenever the menu is open.
            .coerceAtMost(getScreenWidth() - minimumTapArea)

        callSetMeasuredDimension(reconciledWidth, desiredHeight)
    }

    @VisibleForTesting
    internal fun getScreenWidth(): Int = resources.displayMetrics.widthPixels

    @SuppressLint("WrongCall")
    @VisibleForTesting
    // Used for testing protected super.onMeasure(..) calls will be executed.
    internal fun callParentOnMeasure(widthSpec: Int, heightSpec: Int) {
        super.onMeasure(widthSpec, heightSpec)
    }

    @VisibleForTesting
    // Used for testing final protected setMeasuredDimension(..) calls were executed
    internal fun callSetMeasuredDimension(width: Int, height: Int) {
        setMeasuredDimension(width, height)
    }
}
