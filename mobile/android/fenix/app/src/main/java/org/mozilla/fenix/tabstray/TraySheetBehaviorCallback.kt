/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

package org.mozilla.fenix.tabstray

import android.view.View
import androidx.constraintlayout.widget.ConstraintLayout
import com.google.android.material.bottomsheet.BottomSheetBehavior
import com.google.android.material.bottomsheet.BottomSheetBehavior.STATE_HIDDEN

class TraySheetBehaviorCallback(
    private val behavior: BottomSheetBehavior<ConstraintLayout>,
    private val trayInteractor: NavigationInteractor
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
