/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

package mozilla.components.browser.menu2.ext

import android.graphics.Rect
import android.view.Gravity
import android.view.View
import android.widget.PopupWindow
import androidx.core.widget.PopupWindowCompat
import mozilla.components.browser.menu2.R
import mozilla.components.concept.menu.Orientation
import mozilla.components.support.ktx.android.view.isRTL

internal fun PopupWindow.displayPopup(
    containerView: View,
    anchor: View,
    preferredOrientation: Orientation
) {
    // Measure menu
    val spec = View.MeasureSpec.makeMeasureSpec(0, View.MeasureSpec.UNSPECIFIED)
    containerView.measure(spec, spec)

    val (availableHeightToTop, availableHeightToBottom) = getMaxAvailableHeightToTopAndBottom(anchor)
    val (availableWidthToLeft, availableWidthToRight) = getMaxAvailableWidthToLeftAndRight(anchor)
    val containerHeight = containerView.measuredHeight

    val fitsUp = availableHeightToTop >= containerHeight
    val fitsDown = availableHeightToBottom >= containerHeight
    val reversed = availableWidthToLeft < availableWidthToRight

    // Try to use the preferred orientation, if doesn't fit fallback to the best fit.
    when {
        preferredOrientation == Orientation.DOWN && fitsDown ->
            showPopupWithDownOrientation(anchor, reversed)
        preferredOrientation == Orientation.UP && fitsUp ->
            showPopupWithUpOrientation(anchor, availableHeightToBottom, containerHeight, reversed)
        else -> {
            showPopupWhereBestFits(
                anchor,
                fitsUp,
                fitsDown,
                availableHeightToTop,
                availableHeightToBottom,
                reversed,
                containerHeight
            )
        }
    }
}

@Suppress("LongParameterList")
private fun PopupWindow.showPopupWhereBestFits(
    anchor: View,
    fitsUp: Boolean,
    fitsDown: Boolean,
    availableHeightToTop: Int,
    availableHeightToBottom: Int,
    reversed: Boolean,
    containerHeight: Int
) {
    // We don't have enough space to show the menu UP neither DOWN.
    // Let's just show the popup at the location of the anchor.
    if (!fitsUp && !fitsDown) {
        showAtAnchorLocation(anchor, availableHeightToTop < availableHeightToBottom, reversed)
    } else {
        if (fitsDown) {
            showPopupWithDownOrientation(anchor, reversed)
        } else {
            showPopupWithUpOrientation(anchor, availableHeightToBottom, containerHeight, reversed)
        }
    }
}

private fun PopupWindow.showPopupWithUpOrientation(
    anchor: View,
    availableHeightToBottom: Int,
    containerHeight: Int,
    reversed: Boolean,
) {
    val xOffset = if (anchor.isRTL) -anchor.width else 0
    animationStyle = if (reversed) {
        R.style.Mozac_Browser_Menu2_Animation_OverflowMenuLeftBottom
    } else {
        R.style.Mozac_Browser_Menu2_Animation_OverflowMenuRightBottom
    }

    // Positioning the menu above and overlapping the anchor.
    val yOffset = if (availableHeightToBottom < 0) {
        // The anchor is partially below of the bottom of the screen, let's make the menu completely visible.
        availableHeightToBottom - containerHeight
    } else {
        -containerHeight
    }
    showAsDropDown(anchor, xOffset, yOffset)
}

private fun PopupWindow.showPopupWithDownOrientation(anchor: View, reversed: Boolean) {
    val xOffset = if (anchor.isRTL) -anchor.width else 0
    animationStyle = if (reversed) {
        R.style.Mozac_Browser_Menu2_Animation_OverflowMenuLeftTop
    } else {
        R.style.Mozac_Browser_Menu2_Animation_OverflowMenuRightTop
    }
    // Menu should overlay the anchor.
    showAsDropDown(anchor, xOffset, -anchor.height)
}

private fun PopupWindow.showAtAnchorLocation(anchor: View, isCloserToTop: Boolean, reversed: Boolean) {
    val anchorPosition = IntArray(2)

    // Apply the best fit animation style based on positioning
    animationStyle = if (isCloserToTop) {
        if (reversed) {
            R.style.Mozac_Browser_Menu2_Animation_OverflowMenuLeftTop
        } else {
            R.style.Mozac_Browser_Menu2_Animation_OverflowMenuRightTop
        }
    } else {
        if (reversed) {
            R.style.Mozac_Browser_Menu2_Animation_OverflowMenuLeftBottom
        } else {
            R.style.Mozac_Browser_Menu2_Animation_OverflowMenuRightBottom
        }
    }

    anchor.getLocationOnScreen(anchorPosition)
    val (x, y) = anchorPosition
    PopupWindowCompat.setOverlapAnchor(this, true)
    showAtLocation(anchor, Gravity.START or Gravity.TOP, x, y)
}

private fun getMaxAvailableHeightToTopAndBottom(anchor: View): Pair<Int, Int> {
    val anchorPosition = IntArray(2)
    val displayFrame = Rect()

    val appView = anchor.rootView
    appView.getWindowVisibleDisplayFrame(displayFrame)

    anchor.getLocationOnScreen(anchorPosition)

    val bottomEdge = displayFrame.bottom

    val distanceToBottom = bottomEdge - (anchorPosition[1] + anchor.height)
    val distanceToTop = anchorPosition[1] - displayFrame.top

    return distanceToTop to distanceToBottom
}

private fun getMaxAvailableWidthToLeftAndRight(anchor: View): Pair<Int, Int> {
    val anchorPosition = IntArray(2)
    val displayFrame = Rect()

    val appView = anchor.rootView
    appView.getWindowVisibleDisplayFrame(displayFrame)

    anchor.getLocationOnScreen(anchorPosition)

    val distanceToLeft = anchorPosition[0] - displayFrame.left
    val distanceToRight = displayFrame.right - (anchorPosition[0] + anchor.width)

    return distanceToLeft to distanceToRight
}
