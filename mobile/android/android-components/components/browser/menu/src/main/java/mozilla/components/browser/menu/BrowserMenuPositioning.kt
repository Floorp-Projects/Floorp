/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

@file:Suppress("MatchingDeclarationName")

package mozilla.components.browser.menu

import android.graphics.Rect
import android.view.View
import android.view.ViewGroup
import androidx.annotation.Px
import androidx.annotation.VisibleForTesting

/**
 * All data needed for menu positioning.
 */
internal data class MenuPositioningData(
    /**
     * Where and how should the menu be placed in relation to the [BrowserMenuPlacement.anchor].
     */
    val inferredMenuPlacement: BrowserMenuPlacement? = null,

    /**
     * The orientation asked by users of this class when initializing it.
     */
    val askedOrientation: BrowserMenu.Orientation = BrowserMenu.Orientation.DOWN,

    /**
     * Whether the menu fits in the space between [display top, anchor] in a top - down layout.
     */
    val fitsUp: Boolean = false,

    /**
     * Whether the menu fits in the space between [anchor, display top] in a top - down layout.
     */
    val fitsDown: Boolean = false,

    /**
     * Distance between [display top, anchor top margin]. Used for better positioning the menu.
     */
    @Px val availableHeightToTop: Int = 0,

    /**
     * Distance between [display bottom, anchor bottom margin]. Used for better positioning the menu.
     */
    @Px val availableHeightToBottom: Int = 0,

    /**
     * [View#measuredHeight] of the menu. May be bigger than the available screen height.
     */
    @Px val containerViewHeight: Int = 0,
)

/**
 * Measure, calculate, obtain all data needed to know how the menu shown in a PopupWindow should be positioned.
 *
 * This method assumes [currentData] already contains the [MenuPositioningData.askedOrientation].
 *
 * @param containerView the menu layout that will be wrapped in the PopupWindow.
 * @param anchor view the PopupWindow will be aligned to.
 * @param currentData current known data for how the menu should be positioned.
 *
 * @return new [MenuPositioningData] containing the current constraints of the PopupWindow.
 */
internal fun inferMenuPositioningData(
    containerView: ViewGroup,
    anchor: View,
    currentData: MenuPositioningData,
): MenuPositioningData {
    val containerViewHeight = measureContainerHeight(containerView)
    val (availableHeightToTop, availableHeightToBottom) = getMaxAvailableHeightToTopAndBottom(anchor)
    val (fitsUp, fitsDown) = checkIfMenuFits(availableHeightToTop, availableHeightToBottom, containerViewHeight)

    return inferMenuPosition(
        anchor,
        currentData.copy(
            fitsUp = fitsUp,
            fitsDown = fitsDown,
            availableHeightToTop = availableHeightToTop,
            availableHeightToBottom = availableHeightToBottom,
            containerViewHeight = containerViewHeight,
        ),
    )
}

internal fun measureContainerHeight(containerView: ViewGroup): Int {
    val spec = View.MeasureSpec.makeMeasureSpec(0, View.MeasureSpec.UNSPECIFIED)
    containerView.measure(spec, spec)
    return containerView.measuredHeight
}

internal fun checkIfMenuFits(
    availableHeightToTop: Int,
    availableHeightToBottom: Int,
    containerHeight: Int,
): Pair<Boolean, Boolean> {
    val fitsUp = availableHeightToTop >= containerHeight
    val fitsDown = availableHeightToBottom >= containerHeight
    return fitsUp to fitsDown
}

/**
 * Infer where and how the PopupWindow should be shown based on the data available in [currentData].
 * Should be called only once per menu to be shown.
 *
 * @param anchor view the PopupWindow will be aligned to.
 * @param currentData current known data for how the menu should be positioned.
 *
 * @return new MenuPositioningData updated to contain the inferred [BrowserMenuPlacement]
 */
internal fun inferMenuPosition(anchor: View, currentData: MenuPositioningData): MenuPositioningData {
    return when {
        canUsePreferredOrientation(currentData) -> usePreferredOrientation(anchor, currentData)
        neitherOrientationFits(currentData) -> usePlacementWithMostSpaceAvailable(anchor, currentData)
        else -> useOrientationThatFits(anchor, currentData) // At least one orientation fits.
    }
}

@VisibleForTesting
internal fun canUsePreferredOrientation(currentData: MenuPositioningData): Boolean {
    return (currentData.askedOrientation == BrowserMenu.Orientation.DOWN && currentData.fitsDown) ||
        (currentData.askedOrientation == BrowserMenu.Orientation.UP && currentData.fitsUp)
}

@VisibleForTesting
internal fun usePreferredOrientation(anchor: View, currentData: MenuPositioningData): MenuPositioningData {
    val menuPlacement = when (currentData.askedOrientation) {
        BrowserMenu.Orientation.DOWN -> BrowserMenuPlacement.AnchoredToTop.Dropdown(anchor)
        BrowserMenu.Orientation.UP -> BrowserMenuPlacement.AnchoredToBottom.Dropdown(anchor)
    }
    return currentData.copy(inferredMenuPlacement = menuPlacement)
}

@VisibleForTesting
internal fun neitherOrientationFits(currentData: MenuPositioningData): Boolean {
    return !currentData.fitsUp && !currentData.fitsDown
}

@VisibleForTesting
internal fun usePlacementWithMostSpaceAvailable(anchor: View, currentData: MenuPositioningData): MenuPositioningData {
    val menuPlacement = if (currentData.availableHeightToTop < currentData.availableHeightToBottom) {
        BrowserMenuPlacement.AnchoredToTop.ManualAnchoring(anchor)
    } else {
        BrowserMenuPlacement.AnchoredToBottom.ManualAnchoring(anchor)
    }
    return currentData.copy(inferredMenuPlacement = menuPlacement)
}

@VisibleForTesting
internal fun useOrientationThatFits(anchor: View, currentData: MenuPositioningData): MenuPositioningData {
    val menuPlacement = if (currentData.fitsDown) {
        BrowserMenuPlacement.AnchoredToTop.Dropdown(anchor)
    } else {
        BrowserMenuPlacement.AnchoredToBottom.Dropdown(anchor)
    }
    return currentData.copy(inferredMenuPlacement = menuPlacement)
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
