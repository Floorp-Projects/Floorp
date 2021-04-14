/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

package org.mozilla.fenix.tabstray

import androidx.constraintlayout.widget.ConstraintLayout
import com.google.android.material.bottomsheet.BottomSheetBehavior
import com.google.android.material.bottomsheet.BottomSheetBehavior.STATE_HALF_EXPANDED
import com.google.android.material.bottomsheet.BottomSheetBehavior.STATE_HIDDEN
import com.google.android.material.bottomsheet.BottomSheetBehavior.STATE_COLLAPSED
import com.google.android.material.bottomsheet.BottomSheetBehavior.STATE_DRAGGING
import com.google.android.material.bottomsheet.BottomSheetBehavior.STATE_SETTLING
import com.google.android.material.bottomsheet.BottomSheetBehavior.STATE_EXPANDED
import io.mockk.Called
import io.mockk.mockk
import io.mockk.verify
import org.junit.Test

class TraySheetBehaviorCallbackTest {

    @Test
    fun `WHEN state is hidden THEN invoke interactor`() {
        val interactor = mockk<NavigationInteractor>(relaxed = true)
        val callback = TraySheetBehaviorCallback(mockk(), interactor)

        callback.onStateChanged(mockk(), STATE_HIDDEN)

        verify { interactor.onTabTrayDismissed() }
    }

    @Test
    fun `WHEN state is half-expanded THEN close the tray`() {
        val behavior = mockk<BottomSheetBehavior<ConstraintLayout>>(relaxed = true)
        val callback = TraySheetBehaviorCallback(behavior, mockk())

        callback.onStateChanged(mockk(), STATE_HALF_EXPANDED)

        verify { behavior.state = STATE_HIDDEN }
    }

    @Test
    fun `WHEN other states are invoked THEN do nothing`() {
        val behavior = mockk<BottomSheetBehavior<ConstraintLayout>>(relaxed = true)
        val interactor = mockk<NavigationInteractor>(relaxed = true)
        val callback = TraySheetBehaviorCallback(behavior, interactor)

        callback.onStateChanged(mockk(), STATE_COLLAPSED)
        callback.onStateChanged(mockk(), STATE_DRAGGING)
        callback.onStateChanged(mockk(), STATE_SETTLING)
        callback.onStateChanged(mockk(), STATE_EXPANDED)

        verify { behavior wasNot Called }
        verify { interactor wasNot Called }
    }
}
