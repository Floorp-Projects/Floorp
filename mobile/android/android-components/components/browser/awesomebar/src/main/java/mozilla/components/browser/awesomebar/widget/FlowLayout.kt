/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

package mozilla.components.browser.awesomebar.widget

import android.content.Context
import android.util.AttributeSet
import android.view.ViewGroup

/**
 * A [ViewGroup] that will put its child views in a row. If the horizontal space in the [ViewGroup] is too small to
 * put all views in one row, the [FlowLayout] will use multiple rows.
 *
 * Ported from Fennec:
 * https://searchfox.org/mozilla-central/source/mobile/android/base/java/org/mozilla/gecko/widget/FlowLayout.java
 *
 * Comparable to Swing's FlowLayout:
 * https://docs.oracle.com/javase/tutorial/uiswing/layout/flow.html
 */
internal class FlowLayout @JvmOverloads constructor(
    context: Context,
    attrs: AttributeSet? = null,
    defStyleAttr: Int = 0
) : ViewGroup(context, attrs, defStyleAttr) {
    internal var spacing: Int = 0

    override fun onMeasure(widthMeasureSpec: Int, heightMeasureSpec: Int) {
        val parentWidth = MeasureSpec.getSize(widthMeasureSpec)
        var rowWidth = 0
        var totalWidth = 0
        var totalHeight = 0
        var firstChild = true

        for (i in 0 until childCount) {
            val child = getChildAt(i)

            if (child.visibility == GONE) {
                continue
            }

            measureChild(child, widthMeasureSpec, heightMeasureSpec)

            val childWidth = child.measuredWidth
            val childHeight = child.measuredHeight

            if (firstChild || rowWidth + childWidth > parentWidth) {
                rowWidth = 0
                totalHeight += childHeight
                if (!firstChild) {
                    totalHeight += spacing
                }
                firstChild = false
            }

            rowWidth += childWidth

            if (rowWidth > totalWidth) {
                totalWidth = rowWidth
            }

            rowWidth += spacing
        }

        setMeasuredDimension(totalWidth, totalHeight)
    }

    override fun onLayout(changed: Boolean, l: Int, t: Int, r: Int, b: Int) {
        val totalWidth = r - l
        var x = 0
        var y = 0
        var prevChildHeight = 0

        for (i in 0 until childCount) {
            val child = getChildAt(i)
            if (child.visibility == GONE) {
                continue
            }

            val childWidth = child.measuredWidth
            val childHeight = child.measuredHeight
            if (x + childWidth > totalWidth) {
                x = 0
                y += prevChildHeight + spacing
            }
            prevChildHeight = childHeight
            child.layout(x, y, x + childWidth, y + childHeight)
            x += childWidth + spacing
        }
    }
}
