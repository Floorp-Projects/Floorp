/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

package org.mozilla.fenix.tabstray

import android.view.View
import androidx.annotation.VisibleForTesting
import androidx.constraintlayout.widget.ConstraintLayout
import com.google.android.material.bottomsheet.BottomSheetBehavior
import com.google.android.material.bottomsheet.BottomSheetBehavior.STATE_HIDDEN

/**
 * Helper class for updating how the tray looks and behaves depending on app state / internal tray state.
 *
 * @param behavior [BottomSheetBehavior] that will actually control the tray.
 * @param isLandscape whether the device is currently is portrait or landscape.
 * @param maxNumberOfTabs highest number of tabs in each tray page.
 * @param numberForExpandingTray limit depending on which the tray should be collapsed or expanded.
 * @param navigationInteractor [NavigationInteractor] used for tray updates / navigation.
 */
internal class TabSheetBehaviorManager(
    behavior: BottomSheetBehavior<ConstraintLayout>,
    isLandscape: Boolean,
    maxNumberOfTabs: Int,
    numberForExpandingTray: Int,
    navigationInteractor: NavigationInteractor
) {
    init {
        behavior.addBottomSheetCallback(
            TraySheetBehaviorCallback(behavior, navigationInteractor)
        )

        behavior.state = if (isLandscape || maxNumberOfTabs >= numberForExpandingTray) {
            BottomSheetBehavior.STATE_EXPANDED
        } else {
            BottomSheetBehavior.STATE_COLLAPSED
        }
    }
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
