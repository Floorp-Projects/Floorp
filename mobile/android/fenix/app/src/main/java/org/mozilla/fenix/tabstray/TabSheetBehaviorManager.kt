/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

package org.mozilla.fenix.tabstray

import android.content.res.Configuration
import android.util.DisplayMetrics
import android.view.View
import androidx.annotation.VisibleForTesting
import androidx.core.view.ViewCompat
import androidx.core.view.WindowInsetsCompat
import com.google.android.material.bottomsheet.BottomSheetBehavior
import mozilla.components.support.ktx.android.util.dpToPx

@VisibleForTesting internal const val EXPANDED_OFFSET_IN_LANDSCAPE_DP = 0

@VisibleForTesting internal const val EXPANDED_OFFSET_IN_PORTRAIT_DP = 40

/**
 * The default max dim value of the [TabsTrayDialog].
 */
private const val DEFAULT_MAX_DIM = 0.6f

/**
 * The dim amount is 0.0 - 1.0 inclusive. We use this to convert the view element to the dim scale.
 */
private const val DIM_CONVERSION = 1000f

/**
 * Helper class for updating how the tray looks and behaves depending on app state / internal tray state.
 *
 * @property behavior [BottomSheetBehavior] that will actually control the tray.
 * @param orientation current Configuration.ORIENTATION_* of the device.
 * @property maxNumberOfTabs highest number of tabs in each tray page.
 * @property numberForExpandingTray limit depending on which the tray should be collapsed or expanded.
 * @property displayMetrics [DisplayMetrics] used for adapting resources to the current display.
 */
internal class TabSheetBehaviorManager(
    private val behavior: BottomSheetBehavior<out View>,
    orientation: Int,
    private val maxNumberOfTabs: Int,
    private val numberForExpandingTray: Int,
    private val displayMetrics: DisplayMetrics,
) {
    @VisibleForTesting
    internal var currentOrientation = orientation

    init {
        val isInLandscape = isLandscape(orientation)
        updateBehaviorExpandedOffset(isInLandscape)
        updateBehaviorState(isInLandscape)
    }

    /**
     * Update how the tray looks depending on whether it is shown in landscape or portrait.
     */
    internal fun updateDependingOnOrientation(newOrientation: Int) {
        if (currentOrientation != newOrientation) {
            currentOrientation = newOrientation

            val isInLandscape = isLandscape(newOrientation)
            updateBehaviorExpandedOffset(isInLandscape)
            updateBehaviorState(isInLandscape)
        }
    }

    @VisibleForTesting
    internal fun updateBehaviorState(isLandscape: Boolean) {
        behavior.state = if (isLandscape || maxNumberOfTabs >= numberForExpandingTray) {
            BottomSheetBehavior.STATE_EXPANDED
        } else {
            BottomSheetBehavior.STATE_COLLAPSED
        }
    }

    @VisibleForTesting
    internal fun updateBehaviorExpandedOffset(isLandscape: Boolean) {
        behavior.expandedOffset = if (isLandscape) {
            EXPANDED_OFFSET_IN_LANDSCAPE_DP.dpToPx(displayMetrics)
        } else {
            EXPANDED_OFFSET_IN_PORTRAIT_DP.dpToPx(displayMetrics)
        }
    }

    @VisibleForTesting
    internal fun isLandscape(orientation: Int) = Configuration.ORIENTATION_LANDSCAPE == orientation
}

internal class TraySheetBehaviorCallback(
    @get:VisibleForTesting internal val behavior: BottomSheetBehavior<out View>,
    @get:VisibleForTesting internal val trayInteractor: NavigationInteractor,
    private val tabsTrayDialog: TabsTrayDialog,
    private var newTabFab: View,
) : BottomSheetBehavior.BottomSheetCallback() {

    @VisibleForTesting
    var draggedLowestSheetTop: Int? = null

    override fun onStateChanged(bottomSheet: View, newState: Int) {
        when (newState) {
            BottomSheetBehavior.STATE_HIDDEN -> trayInteractor.onTabTrayDismissed()

            // We only support expanded and collapsed states.
            // Otherwise the tray may be left in an unusable state. See #14980.
            BottomSheetBehavior.STATE_HALF_EXPANDED ->
                behavior.state = BottomSheetBehavior.STATE_HIDDEN

            // Reset the dragged lowest top value
            BottomSheetBehavior.STATE_EXPANDED, BottomSheetBehavior.STATE_COLLAPSED -> {
                draggedLowestSheetTop = null
            }

            BottomSheetBehavior.STATE_DRAGGING, BottomSheetBehavior.STATE_SETTLING -> {
                // Do nothing. Both cases are handled in the onSlide function.
            }
        }
    }

    override fun onSlide(bottomSheet: View, slideOffset: Float) {
        setTabsTrayDialogDimAmount(bottomSheet.top)
        setFabY(bottomSheet.top)
    }

    private fun setTabsTrayDialogDimAmount(bottomSheetTop: Int) {
        // Get any displayed bottom system bar.
        val bottomSystemBarHeight =
            ViewCompat.getRootWindowInsets(newTabFab)
                ?.getInsets(WindowInsetsCompat.Type.systemBars())?.bottom ?: 0

        // Calculate and convert delta to dim amount.
        val appVisibleBottom = newTabFab.rootView.bottom - bottomSystemBarHeight
        val trayTopAppBottomDelta = appVisibleBottom - bottomSheetTop
        val convertedDimValue = trayTopAppBottomDelta / DIM_CONVERSION

        if (convertedDimValue < DEFAULT_MAX_DIM) {
            tabsTrayDialog.window?.setDimAmount(convertedDimValue)
        }
    }

    private fun setFabY(bottomSheetTop: Int) {
        if (behavior.state == BottomSheetBehavior.STATE_DRAGGING) {
            draggedLowestSheetTop = getDraggedLowestSheetTop(bottomSheetTop)

            val dynamicSheetButtonDelta = newTabFab.top - draggedLowestSheetTop!!
            newTabFab.y = getUpdatedFabY(bottomSheetTop, dynamicSheetButtonDelta)
        }

        if (behavior.state == BottomSheetBehavior.STATE_SETTLING) {
            val dynamicSheetButtonDelta = newTabFab.top - getDraggedLowestSheetTop(bottomSheetTop)
            newTabFab.y = getUpdatedFabY(bottomSheetTop, dynamicSheetButtonDelta)
        }
    }

    private fun getDraggedLowestSheetTop(currentBottomSheetTop: Int) =
        if (draggedLowestSheetTop == null || currentBottomSheetTop < draggedLowestSheetTop!!) {
            currentBottomSheetTop
        } else {
            draggedLowestSheetTop!!
        }

    private fun getUpdatedFabY(bottomSheetTop: Int, dynamicSheetButtonDelta: Int) =
        (bottomSheetTop + dynamicSheetButtonDelta).toFloat()
}
