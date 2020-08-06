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
    val containerHeight = containerView.measuredHeight

    val fitsUp = availableHeightToTop >= containerHeight
    val fitsDown = availableHeightToBottom >= containerHeight

    // Try to use the preferred orientation, if doesn't fit fallback to the best fit.
    when {
        preferredOrientation == Orientation.DOWN && fitsDown ->
            showPopupWithDownOrientation(anchor)
        preferredOrientation == Orientation.UP && fitsUp ->
            showPopupWithUpOrientation(anchor, availableHeightToBottom, containerHeight)
        else -> {
            showPopupWhereBestFits(
                anchor,
                fitsUp,
                fitsDown,
                availableHeightToTop,
                availableHeightToBottom,
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
    containerHeight: Int
) {
    // We don't have enough space to show the menu UP neither DOWN.
    // Let's just show the popup at the location of the anchor.
    if (!fitsUp && !fitsDown) {
        showAtAnchorLocation(anchor, availableHeightToTop < availableHeightToBottom)
    } else {
        if (fitsDown) {
            showPopupWithDownOrientation(anchor)
        } else {
            showPopupWithUpOrientation(anchor, availableHeightToBottom, containerHeight)
        }
    }
}

private fun PopupWindow.showPopupWithUpOrientation(anchor: View, availableHeightToBottom: Int, containerHeight: Int) {
    val xOffset = if (anchor.isRTL) -anchor.width else 0
    animationStyle = R.style.Mozac_Browser_Menu2_Animation_OverflowMenuBottom

    // Positioning the menu above and overlapping the anchor.
    val yOffset = if (availableHeightToBottom < 0) {
        // The anchor is partially below of the bottom of the screen, let's make the menu completely visible.
        availableHeightToBottom - containerHeight
    } else {
        -containerHeight
    }
    showAsDropDown(anchor, xOffset, yOffset)
}

private fun PopupWindow.showPopupWithDownOrientation(anchor: View) {
    val xOffset = if (anchor.isRTL) -anchor.width else 0
    animationStyle = R.style.Mozac_Browser_Menu2_Animation_OverflowMenuTop
    // Menu should overlay the anchor.
    showAsDropDown(anchor, xOffset, -anchor.height)
}

private fun PopupWindow.showAtAnchorLocation(anchor: View, isCloserToTop: Boolean) {
    val anchorPosition = IntArray(2)

    // Apply the best fit animation style based on positioning
    animationStyle = if (isCloserToTop) {
        R.style.Mozac_Browser_Menu2_Animation_OverflowMenuTop
    } else {
        R.style.Mozac_Browser_Menu2_Animation_OverflowMenuBottom
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
