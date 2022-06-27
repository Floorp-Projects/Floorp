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

internal fun PopupWindow.displayPopup(
    containerView: View,
    anchor: View,
    preferredOrientation: Orientation? = null,
    forceOrientation: Boolean = false,
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

    // Popup window does not need a input method. This avoids keyboard flicker when menu is opened.
    inputMethodMode = PopupWindow.INPUT_METHOD_NOT_NEEDED

    // Try to use the preferred orientation, if doesn't fit fallback to the best fit.
    when {
        preferredOrientation == Orientation.DOWN && (fitsDown || forceOrientation) ->
            showPopupWithDownOrientation(anchor, reversed)
        preferredOrientation == Orientation.UP && (fitsUp || forceOrientation) ->
            showPopupWithUpOrientation(anchor, containerHeight, reversed)
        else -> {
            showPopupWhereBestFits(
                anchor,
                fitsUp,
                fitsDown,
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
    reversed: Boolean,
    containerHeight: Int
) {
    when {
        // Not enough space to show the menu UP neither DOWN.
        // Let's just show the popup at the location of the anchor.
        !fitsUp && !fitsDown -> showAtAnchorLocation(anchor, reversed)
        // Enough space to show menu down
        fitsDown -> showPopupWithDownOrientation(anchor, reversed)
        // Otherwise, show menu up
        else -> showPopupWithUpOrientation(anchor, containerHeight, reversed)
    }
}

private fun PopupWindow.showPopupWithUpOrientation(
    anchor: View,
    containerHeight: Int,
    reversed: Boolean,
) {
    animationStyle = if (reversed) {
        R.style.Mozac_Browser_Menu2_Animation_OverflowMenuLeftBottom
    } else {
        R.style.Mozac_Browser_Menu2_Animation_OverflowMenuRightBottom
    }

    val yOffset = -containerHeight
    showAsDropDown(anchor, 0, yOffset)
}

private fun PopupWindow.showPopupWithDownOrientation(anchor: View, reversed: Boolean) {
    // Apply the best fit animation style based on positioning
    animationStyle = if (reversed) {
        R.style.Mozac_Browser_Menu2_Animation_OverflowMenuLeftTop
    } else {
        R.style.Mozac_Browser_Menu2_Animation_OverflowMenuRightTop
    }

    PopupWindowCompat.setOverlapAnchor(this, true)
    showAsDropDown(anchor)
}

private fun PopupWindow.showAtAnchorLocation(
    anchor: View,
    reversed: Boolean,
) {
    val anchorPosition = IntArray(2)

    // Apply the best fit animation style based on positioning
    animationStyle = if (reversed) {
        R.style.Mozac_Browser_Menu2_Animation_OverflowMenuLeft
    } else {
        R.style.Mozac_Browser_Menu2_Animation_OverflowMenuRight
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
