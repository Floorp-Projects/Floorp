/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

package org.mozilla.fenix.home.topsites

import android.content.Context
import android.util.AttributeSet
import android.util.TypedValue
import android.view.View
import android.view.ViewGroup.LayoutParams
import android.widget.LinearLayout
import org.mozilla.fenix.R

/**
 * A pager indicator widget to display the number of pages and the current selected page.
 */
class PagerIndicator : LinearLayout {

    constructor(context: Context) : super(context)

    constructor(context: Context, attrs: AttributeSet?) : super(context, attrs)

    constructor(context: Context, attrs: AttributeSet?, defStyleAttr: Int) : super(context, attrs, defStyleAttr)

    private var selectedIndex = 0

    /**
     * Set the number of pager dots to display.
     */
    fun setSize(size: Int) {
        if (childCount == size) {
            return
        }
        if (selectedIndex >= size) {
            selectedIndex = size - 1
        }

        removeAllViews()
        for (i in 0 until size) {
            val isLast = i == size - 1
            addView(
                View(context).apply {
                    setBackgroundResource(R.drawable.pager_dot)
                    isSelected = i == selectedIndex
                },
                LayoutParams(dpToPx(DOT_SIZE_IN_DP), dpToPx(DOT_SIZE_IN_DP)).apply {
                    if (!isLast) {
                        this.setMarginEnd(dpToPx(DOT_MARGIN))
                    }
                },
            )
        }
    }

    /**
     * Set the current selected pager dot.
     */
    fun setSelection(index: Int) {
        if (selectedIndex == index) {
            return
        }

        getChildAt(selectedIndex)?.run {
            isSelected = false
        }
        getChildAt(index)?.run {
            isSelected = true
        }
        selectedIndex = index
    }

    companion object {
        private const val DOT_SIZE_IN_DP = 6f
        private const val DOT_MARGIN = 4f
    }
}

fun Context.dpToPx(value: Float): Int =
    TypedValue.applyDimension(TypedValue.COMPLEX_UNIT_DIP, value, resources.displayMetrics).toInt()

fun View.dpToPx(value: Float): Int = context.dpToPx(value)
