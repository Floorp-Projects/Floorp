/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

package mozilla.components.browser.menu2.ext

import android.graphics.Rect
import android.view.Gravity
import android.view.View
import android.widget.PopupWindow
import androidx.core.view.ViewCompat
import androidx.core.view.WindowInsetsCompat
import androidx.core.widget.PopupWindowCompat
import androidx.recyclerview.widget.RecyclerView
import mozilla.components.browser.menu2.R
import mozilla.components.concept.menu.Orientation
import mozilla.components.support.base.log.Log
import mozilla.components.support.ktx.android.view.isRTL
import kotlin.math.roundToInt

const val HALF_MENU_ITEM = 0.5

/**
 * If parameters passed are a valid configuration attempt to show a [PopupWindow].
 *
 * @return True if the parameters passed are a valid configuration, otherwise false.
 */
internal fun PopupWindow.displayPopup(
    containerView: View,
    anchor: View,
    orientation: Orientation? = null,
): Boolean {
    // Measure menu.
    val spec = View.MeasureSpec.makeMeasureSpec(0, View.MeasureSpec.UNSPECIFIED)
    containerView.measure(spec, spec)

    val containerRecyclerView =
        containerView.findViewById<RecyclerView>(R.id.mozac_browser_menu_recyclerView)
    val recyclerViewAdapter = containerRecyclerView.adapter

    if (recyclerViewAdapter == null) {
        Log.log(
            priority = Log.Priority.ERROR,
            tag = "mozac-popupwindow",
            message = "The browser menu recyclerview has no adapter set.",
        )
        return false
    }

    val containerViewHeight = containerView.measuredHeight
    val recyclerViewHeight = containerRecyclerView.measuredHeight
    val recyclerViewItemCount = recyclerViewAdapter.itemCount

    val hasViewInitializedCorrectly =
        containerViewHeight > 0 && recyclerViewHeight > 0 && recyclerViewItemCount > 0
    if (!hasViewInitializedCorrectly) {
        Log.log(
            priority = Log.Priority.ERROR,
            tag = "mozac-popupwindow",
            message = "containerViewHeight: $containerViewHeight must be more than 0," +
                "recyclerViewHeight: $recyclerViewHeight must be more than 0 &" +
                "recyclerViewItemCount: $recyclerViewItemCount must be more than 0.",
        )
        return false
    }

    // Popup window does not need a input method. This avoids keyboard flicker when menu is opened.
    inputMethodMode = PopupWindow.INPUT_METHOD_NOT_NEEDED

    showPopup(orientation, anchor, recyclerViewHeight, recyclerViewItemCount, containerViewHeight)
    return true
}

/**
 * Show the [PopupWindow] with the required configuration.
 */
private fun PopupWindow.showPopup(
    orientation: Orientation?,
    anchor: View,
    recyclerViewHeight: Int,
    recyclerViewItemCount: Int,
    containerViewHeight: Int,
) {
    val calculatedContainerHeight =
        calculateContainerHeight(recyclerViewHeight, recyclerViewItemCount, containerViewHeight, anchor)

    when (orientation) {
        Orientation.DOWN -> showPopupWithDownOrientation(anchor, calculatedContainerHeight)

        Orientation.UP -> {
            val availableHeightToTop = getMaxAvailableHeightToTopAndBottom(anchor).first

            // This could include cases where the keyboard is being displayed.
            if (calculatedContainerHeight > availableHeightToTop) {
                showPopupWhereBestFits(anchor, calculatedContainerHeight)
            } else {
                showPopupWithUpOrientation(anchor, calculatedContainerHeight)
            }
        }

        else -> showPopupWhereBestFits(anchor, calculatedContainerHeight)
    }
}

/**
 * Determine whether the container view can display all menu items (without scrolling) within
 * the available height.
 *
 * @return The original container height if the container view can display all menu items
 * (without scrolling), else calculate the maximum available container height for a scrollable
 * view with a half menu item.
 */
private fun calculateContainerHeight(
    recyclerViewHeight: Int,
    recyclerViewItemCount: Int,
    containerViewHeight: Int,
    anchor: View,
): Int {
    // Get the total screen display height.
    val totalHeight = anchor.rootView.measuredHeight

    // Note: We cannot use getWindowVisibleDisplayFrame() as the height is dynamic based on whether
    // the keyboard is open.
    // Get any displayed system bars e.g. top status bar, bottom navigation bar or soft buttons bar.
    val systemBars =
        ViewCompat.getRootWindowInsets(anchor)?.getInsets(WindowInsetsCompat.Type.systemBars())
    // Store the vertical status bars.
    val topSystemBarHeight = systemBars?.top ?: 0
    val bottomSystemBarHeight = systemBars?.bottom ?: 0

    // Deduct any status bar heights from the total height.
    val availableHeight = totalHeight - (bottomSystemBarHeight + topSystemBarHeight)

    val menuItemHeight = recyclerViewHeight / recyclerViewItemCount

    // We must take the menu container padding into account as this will be applied to the final height.
    val containerPadding = containerViewHeight - recyclerViewHeight

    val maxAvailableHeightForRecyclerView = availableHeight - containerPadding

    // The number of menu items that can fit exactly (no cropping) within the max app height.
    // Round the number of items to the closet Int value to ensure the max space available is utilized.
    // E.g if 6.9 items fit, round to 7 so the calculation below will show 6.5 items instead of 5.5 .
    val numberOfItemsFitExactly =
        (maxAvailableHeightForRecyclerView.toFloat() / menuItemHeight.toFloat()).roundToInt()

    val itemsAlreadyFitContainerHeight = recyclerViewItemCount <= numberOfItemsFitExactly

    return if (itemsAlreadyFitContainerHeight) {
        containerViewHeight
    } else {
        getCroppedMenuContainerHeight(numberOfItemsFitExactly, menuItemHeight, containerPadding)
    }
}

private fun getCroppedMenuContainerHeight(
    numberOfItemsFitExactly: Int,
    menuItemHeight: Int,
    containerPadding: Int,
): Int {
    // The number of menu items that fit exactly, minus a half menu item (indicates more menu items exist).
    val numberOfItemsFitWithOverflow = numberOfItemsFitExactly - HALF_MENU_ITEM
    val updatedRecyclerViewHeight = (numberOfItemsFitWithOverflow * menuItemHeight).toInt()

    return updatedRecyclerViewHeight + containerPadding
}

private fun PopupWindow.showPopupWhereBestFits(
    anchor: View,
    containerHeight: Int,
) {
    val (availableHeightToTop, availableHeightToBottom) = getMaxAvailableHeightToTopAndBottom(anchor)
    val fitsUp = availableHeightToTop >= containerHeight
    val fitsDown = availableHeightToBottom >= containerHeight

    when {
        // Not enough space to show the menu UP neither DOWN.
        // Let's just show the popup at the location of the anchor.
        !fitsUp && !fitsDown -> showAtAnchorLocation(anchor, containerHeight)
        // Enough space to show menu down
        fitsDown -> showPopupWithDownOrientation(anchor, containerHeight)
        // Otherwise, show menu up
        else -> showPopupWithUpOrientation(anchor, containerHeight)
    }
}

private fun PopupWindow.showPopupWithUpOrientation(
    anchor: View,
    containerHeight: Int,
) {
    // Apply the best fit animation style based on positioning
    animationStyle = if (anchor.isRTL) {
        R.style.Mozac_Browser_Menu2_Animation_OverflowMenuRightBottom
    } else {
        R.style.Mozac_Browser_Menu2_Animation_OverflowMenuLeftBottom
    }
    height = containerHeight

    val yOffset = -containerHeight
    showAsDropDown(anchor, 0, yOffset)
}

private fun PopupWindow.showPopupWithDownOrientation(
    anchor: View,
    containerHeight: Int,
) {
    // Apply the best fit animation style based on positioning
    animationStyle = if (anchor.isRTL) {
        R.style.Mozac_Browser_Menu2_Animation_OverflowMenuRightTop
    } else {
        R.style.Mozac_Browser_Menu2_Animation_OverflowMenuLeftTop
    }
    height = containerHeight

    PopupWindowCompat.setOverlapAnchor(this, true)
    showAsDropDown(anchor)
}

private fun PopupWindow.showAtAnchorLocation(
    anchor: View,
    containerHeight: Int,
) {
    val anchorPosition = IntArray(2)

    // Apply the best fit animation style based on positioning
    animationStyle = if (anchor.isRTL) {
        R.style.Mozac_Browser_Menu2_Animation_OverflowMenuRight
    } else {
        R.style.Mozac_Browser_Menu2_Animation_OverflowMenuLeft
    }
    height = containerHeight

    anchor.getLocationInWindow(anchorPosition)
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
