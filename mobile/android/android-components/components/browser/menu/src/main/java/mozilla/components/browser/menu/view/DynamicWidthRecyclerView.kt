/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

package mozilla.components.browser.menu.view

import android.annotation.SuppressLint
import android.content.Context
import android.util.AttributeSet
import androidx.annotation.VisibleForTesting
import androidx.recyclerview.widget.RecyclerView
import mozilla.components.browser.menu.R

/**
 * [RecylerView] with automatically set width between widthMin / widthMax xml attributes.
 */
class DynamicWidthRecyclerView @JvmOverloads constructor(
    context: Context,
    attrs: AttributeSet? = null
) : RecyclerView(context, attrs) {
    @VisibleForTesting internal var minWidth: Int = -1
    @VisibleForTesting internal var maxWidth: Int = -1

    init {
        context.obtainStyledAttributes(attrs, R.styleable.DynamicWidthRecyclerView).apply {
            minWidth = getDimension(R.styleable.DynamicWidthRecyclerView_minWidth, minWidth.toFloat()).toInt()
            maxWidth = getDimension(R.styleable.DynamicWidthRecyclerView_maxWidth, maxWidth.toFloat()).toInt()
            recycle()
        }
    }

    @VisibleForTesting(otherwise = VisibleForTesting.PROTECTED)
    override fun onMeasure(widthSpec: Int, heightSpec: Int) {
        if (minWidth in 1 until maxWidth) {
            // Ignore any bounds set in xml. Allow for children to expand entirely.
            callParentOnMeasure(MeasureSpec.makeMeasureSpec(0, MeasureSpec.UNSPECIFIED), heightSpec)

            // Children now have "unspecified" width. Let's set some bounds.
            setReconciledDimensions(measuredWidth, measuredHeight)
        } else {
            // Default behavior. layout_width / layout_height properties will be used for measuring.
            callParentOnMeasure(widthSpec, heightSpec)
        }
    }

    @VisibleForTesting()
    internal fun setReconciledDimensions(
        desiredWidth: Int,
        desiredHeight: Int
    ) {
        val minimumWidth = desiredWidth.coerceAtLeast(minWidth)

        val reconciledWidth = minimumWidth
            .coerceAtMost(maxWidth)
            // Sanity check our xml set maxWidth.
            .coerceAtMost(getScreenWidth())

        callSetMeasuredDimension(reconciledWidth, desiredHeight)
    }

    @VisibleForTesting()
    internal fun getScreenWidth(): Int = resources.displayMetrics.widthPixels

    @SuppressLint("WrongCall")
    @VisibleForTesting()
    // Used for testing protected super.onMeasure(..) calls will be executed.
    internal fun callParentOnMeasure(widthSpec: Int, heightSpec: Int) {
        super.onMeasure(widthSpec, heightSpec)
    }

    @VisibleForTesting()
    // Used for testing final protected setMeasuredDimension(..) calls were executed
    internal fun callSetMeasuredDimension(width: Int, height: Int) {
        setMeasuredDimension(width, height)
    }
}
