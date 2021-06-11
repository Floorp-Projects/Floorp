/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

package org.mozilla.fenix.tabstray

import android.content.res.Configuration
import android.view.View
import androidx.annotation.VisibleForTesting
import androidx.constraintlayout.widget.ConstraintLayout
import com.google.android.material.bottomsheet.BottomSheetBehavior
import com.google.android.material.bottomsheet.BottomSheetBehavior.STATE_HIDDEN

/**
 * Helper class for updating how the tray looks and behaves depending on app state / internal tray state.
 *
 * @param behavior [BottomSheetBehavior] that will actually control the tray.
 * @param orientation current Configuration.ORIENTATION_* of the device.
 * @param maxNumberOfTabs highest number of tabs in each tray page.
 * @param numberForExpandingTray limit depending on which the tray should be collapsed or expanded.
 * @param navigationInteractor [NavigationInteractor] used for tray updates / navigation.
 */
internal class TabSheetBehaviorManager(
    private val behavior: BottomSheetBehavior<ConstraintLayout>,
    orientation: Int,
    private val maxNumberOfTabs: Int,
    private val numberForExpandingTray: Int,
    navigationInteractor: NavigationInteractor
) {
    @VisibleForTesting
    internal var currentOrientation = orientation

    init {
        behavior.addBottomSheetCallback(
            TraySheetBehaviorCallback(behavior, navigationInteractor)
        )

        updateBehaviorState(isLandscape(orientation))
    }

    /**
     * Update how the tray looks depending on whether it is shown in landscape or portrait.
     */
    internal fun updateDependingOnOrientation(newOrientation: Int) {
        if (currentOrientation != newOrientation) {
            currentOrientation = newOrientation

            val isInLandscape = isLandscape(newOrientation)
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
    internal fun isLandscape(orientation: Int) = Configuration.ORIENTATION_LANDSCAPE == orientation
}

@VisibleForTesting
internal class TraySheetBehaviorCallback(
    @VisibleForTesting internal val behavior: BottomSheetBehavior<ConstraintLayout>,
    @VisibleForTesting internal val trayInteractor: NavigationInteractor
) : BottomSheetBehavior.BottomSheetCallback() {

    override fun onStateChanged(bottomSheet: View, newState: Int) {
        if (newState == STATE_HIDDEN) {
            trayInteractor.onTabTrayDismissed()
        } else if (newState == BottomSheetBehavior.STATE_HALF_EXPANDED) {
            // We only support expanded and collapsed states.
            // But why??
            behavior.state = STATE_HIDDEN
        }
    }

    override fun onSlide(bottomSheet: View, slideOffset: Float) = Unit
}
