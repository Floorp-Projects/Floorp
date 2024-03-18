/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

package mozilla.components.browser.menu2.ext

import android.graphics.Rect
import android.view.View
import androidx.annotation.Px
import androidx.core.view.ViewCompat
import androidx.core.view.WindowInsetsCompat
import androidx.recyclerview.widget.RecyclerView
import mozilla.components.browser.menu2.R
import mozilla.components.concept.menu.MenuStyle
import mozilla.components.concept.menu.Orientation
import mozilla.components.support.ktx.android.view.isRTL
import kotlin.math.roundToInt

const val HALF_MENU_ITEM = 0.5

@Suppress("ComplexMethod")
internal fun inferMenuPositioningData(
    containerView: View,
    anchor: View,
    style: MenuStyle? = null,
    orientation: Orientation?,
): MenuPositioningData? {
    // Measure the menu allowing it to expand entirely.
    val spec = View.MeasureSpec.makeMeasureSpec(0, View.MeasureSpec.UNSPECIFIED)
    containerView.measure(spec, spec)

    val recyclerView = containerView.findViewById<RecyclerView>(R.id.mozac_browser_menu_recyclerView)
    val recyclerViewAdapter = recyclerView.adapter ?: run {
        // We might want to track how often and in what circumstances the menu gets called without
        // valid parameters once we have a system for that.
        // https://bugzilla.mozilla.org/show_bug.cgi?id=1814816
        return null
    }

    val hasViewInitializedCorrectly = containerView.measuredHeight > 0 && containerView.measuredWidth > 0 &&
        recyclerView.measuredHeight > 0 && recyclerViewAdapter.itemCount > 0
    if (!hasViewInitializedCorrectly) {
        // Same as above: https://bugzilla.mozilla.org/show_bug.cgi?id=1814816
        return null
    }

    val menuHorizontalPadding = containerView.measuredWidth - recyclerView.measuredWidth
    val menuVerticalPadding = containerView.measuredHeight - recyclerView.measuredHeight
    var horizontalOffset = style?.horizontalOffset ?: 0
    var verticalOffset = style?.verticalOffset ?: 0

    // Elevation creates some padding between the menu and its container, so that the start corner
    // of the menu doesn't match the corner of the anchor view. If the user wants the menu to hide
    // the anchor completely, we have to adjust the position of the menu to compensate for the inner
    // padding of the menu container.
    if (style?.completelyOverlap == true) {
        horizontalOffset -= menuHorizontalPadding / 2
        verticalOffset -= menuVerticalPadding / 2
    }

    // The menu height might be adjusted: if there is not enough space to show all the items,
    // it will crop the last visible item in half, to give the user a hint that it is scrollable.
    val containerViewHeight = calculateContainerHeight(
        recyclerView.measuredHeight,
        recyclerViewAdapter.itemCount,
        containerView.measuredHeight,
        style?.verticalOffset ?: 0,
        anchor,
    )

    val (availableHeightToTop, availableHeightToBottom) = getMaxAvailableHeightToTopAndBottom(anchor)
    val (availableWidthToLeft, availableWidthToRight) = getMaxAvailableWidthToLeftAndRight(anchor)

    val fitsUp = availableHeightToTop + anchor.height >= containerViewHeight
    val fitsDown = availableHeightToBottom + anchor.height >= containerViewHeight
    val fitsRight = availableWidthToRight + anchor.width >= containerView.measuredWidth
    val fitsLeft = availableWidthToLeft + anchor.width >= containerView.measuredWidth

    val notEnoughHorizontalSpace = !fitsRight && !fitsLeft
    val fitsBothHorizontalDirections = fitsRight && fitsLeft
    val drawingLeft = if (notEnoughHorizontalSpace || fitsBothHorizontalDirections) {
        anchor.isRTL
    } else {
        !fitsRight
    }

    val anchorPosition = IntArray(2)
    anchor.getLocationInWindow(anchorPosition)
    var (anchorX, anchorY) = anchorPosition

    // Position the menu above the anchor if the orientation is UP and there is enough space.
    if (orientation == Orientation.UP && fitsUp) {
        anchorY -= containerViewHeight - anchor.height
        verticalOffset = -verticalOffset
    }

    if (drawingLeft) {
        anchorX -= containerView.measuredWidth - anchor.width
        horizontalOffset = -horizontalOffset
    }

    return MenuPositioningData(
        anchor = anchor,
        x = anchorX + horizontalOffset,
        y = anchorY + verticalOffset,
        containerHeight = containerViewHeight,
        animation = getAnimation(fitsUp, fitsDown, drawingLeft, orientation),
    )
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
    menuStylePadding: Int,
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

    val maxAvailableHeightForRecyclerView = availableHeight - containerPadding - menuStylePadding

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

private fun getAnimation(
    fitsUp: Boolean,
    fitsDown: Boolean,
    drawingLeft: Boolean,
    orientation: Orientation?,
): Int {
    val isUpOrientation = orientation == Orientation.UP
    return when {
        isUpOrientation && fitsUp -> if (drawingLeft) {
            R.style.Mozac_Browser_Menu2_Animation_OverflowMenuRightBottom
        } else {
            R.style.Mozac_Browser_Menu2_Animation_OverflowMenuLeftBottom
        }
        fitsDown -> if (drawingLeft) {
            R.style.Mozac_Browser_Menu2_Animation_OverflowMenuRightTop
        } else {
            R.style.Mozac_Browser_Menu2_Animation_OverflowMenuLeftTop
        }
        else -> if (drawingLeft) {
            R.style.Mozac_Browser_Menu2_Animation_OverflowMenuRight
        } else {
            R.style.Mozac_Browser_Menu2_Animation_OverflowMenuLeft
        }
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

/**
 * Data needed for menu positioning.
 */
data class MenuPositioningData(
    /**
     * Android View that the PopupWindow should be anchored to.
     */
    val anchor: View,

    /**
     * [WindowManager#LayoutParams#x] of params the menu will be added with.
     */
    @Px val x: Int = 0,

    /**
     * [WindowManager#LayoutParams#y] of params the menu will be added with.
     */
    @Px val y: Int = 0,

    /**
     * [View#measuredHeight] of the menu.
     */
    @Px val containerHeight: Int = 0,

    /**
     * [PopupWindow#animationStyle] of the menu.
     */
    val animation: Int,
)
