/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

package org.mozilla.focus.firstrun

import android.content.Context
import android.util.AttributeSet
import android.view.View
import androidx.cardview.widget.CardView
import org.mozilla.focus.R

class FirstrunCardView @JvmOverloads constructor(
    context: Context,
    attrs: AttributeSet? = null,
    defStyleAttr: Int = R.attr.cardViewStyle
) : androidx.cardview.widget.CardView(context, attrs, defStyleAttr) {

    private val maxWidth = resources.getDimensionPixelSize(R.dimen.firstrun_card_width)
    private val maxHeight = resources.getDimensionPixelSize(R.dimen.firstrun_card_height)

    override fun onMeasure(widthMeasureSpec: Int, heightMeasureSpec: Int) {
        // The view is set to match_parent in the layout file. So width and height should be the
        // value needed to fill the whole parent view.
        val availableWidth = View.MeasureSpec.getSize(widthMeasureSpec)
        val availableHeight = View.MeasureSpec.getSize(heightMeasureSpec)

        // Now let's use those sizes to measure - but let's not exceed our defined max sizes (We do
        // not want to have gigantic cards on large devices like tablets.)
        val measuredWidth = Math.min(availableWidth, maxWidth)
        val measuredHeight = Math.min(availableHeight, maxHeight)

        // Let's use the measured values to hand them to the super class to measure the child views etc.
        val newWidthMeasureSpec = View.MeasureSpec.makeMeasureSpec(measuredWidth, View.MeasureSpec.EXACTLY)
        val newHeightMeasureSpec = View.MeasureSpec.makeMeasureSpec(measuredHeight, View.MeasureSpec.EXACTLY)

        super.onMeasure(newWidthMeasureSpec, newHeightMeasureSpec)
    }
}
