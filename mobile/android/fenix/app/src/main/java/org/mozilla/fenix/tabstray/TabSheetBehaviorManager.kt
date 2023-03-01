/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

package org.mozilla.fenix.tabstray

import android.content.res.Configuration
import android.util.DisplayMetrics
import android.view.View
import androidx.annotation.VisibleForTesting
import com.google.android.material.bottomsheet.BottomSheetBehavior
import com.google.android.material.bottomsheet.BottomSheetBehavior.STATE_HIDDEN
import mozilla.components.support.ktx.android.util.dpToPx

@VisibleForTesting internal const val EXPANDED_OFFSET_IN_LANDSCAPE_DP = 0

@VisibleForTesting internal const val EXPANDED_OFFSET_IN_PORTRAIT_DP = 40

/**
 * Helper class for updating how the tray looks and behaves depending on app state / internal tray state.
 *
 * @param behavior [BottomSheetBehavior] that will actually control the tray.
 * @param orientation current Configuration.ORIENTATION_* of the device.
 * @param maxNumberOfTabs highest number of tabs in each tray page.
 * @param numberForExpandingTray limit depending on which the tray should be collapsed or expanded.
 * @param navigationInteractor [NavigationInteractor] used for tray updates / navigation.
 * @param displayMetrics [DisplayMetrics] used for adapting resources to the current display.
 */
internal class TabSheetBehaviorManager(
    private val behavior: BottomSheetBehavior<out View>,
    orientation: Int,
    private val maxNumberOfTabs: Int,
    private val numberForExpandingTray: Int,
    navigationInteractor: NavigationInteractor,
    private val displayMetrics: DisplayMetrics,
) {
    @VisibleForTesting
    internal var currentOrientation = orientation

    init {
        behavior.skipCollapsed = true
        behavior.addBottomSheetCallback(
            TraySheetBehaviorCallback(behavior, navigationInteractor),
        )

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

@VisibleForTesting
internal class TraySheetBehaviorCallback(
    @get:VisibleForTesting internal val behavior: BottomSheetBehavior<out View>,
    @get:VisibleForTesting internal val trayInteractor: NavigationInteractor,
) : BottomSheetBehavior.BottomSheetCallback() {

    override fun onStateChanged(bottomSheet: View, newState: Int) {
        if (newState == STATE_HIDDEN) {
            trayInteractor.onTabTrayDismissed()
        } else if (newState == BottomSheetBehavior.STATE_HALF_EXPANDED) {
            // We only support expanded and collapsed states.
            // Otherwise the tray may be left in an unusable state. See #14980.
            behavior.state = STATE_HIDDEN
        }
    }

    override fun onSlide(bottomSheet: View, slideOffset: Float) = Unit
}
