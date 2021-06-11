/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

package org.mozilla.fenix.tabstray

import androidx.constraintlayout.widget.ConstraintLayout
import com.google.android.material.bottomsheet.BottomSheetBehavior
import com.google.android.material.bottomsheet.BottomSheetBehavior.STATE_COLLAPSED
import com.google.android.material.bottomsheet.BottomSheetBehavior.STATE_DRAGGING
import com.google.android.material.bottomsheet.BottomSheetBehavior.STATE_EXPANDED
import com.google.android.material.bottomsheet.BottomSheetBehavior.STATE_HALF_EXPANDED
import com.google.android.material.bottomsheet.BottomSheetBehavior.STATE_HIDDEN
import com.google.android.material.bottomsheet.BottomSheetBehavior.STATE_SETTLING
import io.mockk.Called
import io.mockk.mockk
import io.mockk.slot
import io.mockk.verify
import org.junit.Assert.assertEquals
import org.junit.Assert.assertSame
import org.junit.Test

class TabSheetBehaviorManagerTest {

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

    @Test
    fun `GIVEN a behavior WHEN TabSheetBehaviorManager is initialized THEN it sets a TraySheetBehaviorCallback on that behavior`() {
        val behavior: BottomSheetBehavior<ConstraintLayout> = mockk(relaxed = true)
        val navigationInteractor: NavigationInteractor = mockk()
        val callbackCaptor = slot<TraySheetBehaviorCallback>()

        TabSheetBehaviorManager(behavior, true, 2, 2, navigationInteractor)

        verify { behavior.addBottomSheetCallback(capture(callbackCaptor)) }
        assertSame(behavior, callbackCaptor.captured.behavior)
        assertSame(navigationInteractor, callbackCaptor.captured.trayInteractor)
    }

    @Test
    fun `GIVEN more tabs opened than the expanding limit and portrait orientation WHEN TabSheetBehaviorManager is initialized THEN the behavior is set as expanded`() {
        val behavior = BottomSheetBehavior<ConstraintLayout>()

        TabSheetBehaviorManager(behavior, false, 5, 4, mockk())

        assertEquals(STATE_EXPANDED, behavior.state)
    }

    @Test
    fun `GIVEN the number of tabs opened is exactly the expanding limit and portrait orientation WHEN TabSheetBehaviorManager is initialized THEN the behavior is set as expanded`() {
        val behavior = BottomSheetBehavior<ConstraintLayout>()

        TabSheetBehaviorManager(behavior, false, 5, 5, mockk())

        assertEquals(STATE_EXPANDED, behavior.state)
    }

    @Test
    fun `GIVEN fewer tabs opened than the expanding limit and portrait orientation WHEN TabSheetBehaviorManager is initialized THEN the behavior is set as collapsed`() {
        val behavior = BottomSheetBehavior<ConstraintLayout>()

        TabSheetBehaviorManager(behavior, false, 4, 5, mockk())

        assertEquals(STATE_COLLAPSED, behavior.state)
    }

    @Test
    fun `GIVEN more tabs opened than the expanding limit and landscape orientation WHEN TabSheetBehaviorManager is initialized THEN the behavior is set as expanded`() {
        val behavior = BottomSheetBehavior<ConstraintLayout>()

        TabSheetBehaviorManager(behavior, true, 5, 4, mockk())

        assertEquals(STATE_EXPANDED, behavior.state)
    }

    @Test
    fun `GIVEN the number of tabs opened is exactly the expanding limit and landscape orientation WHEN TabSheetBehaviorManager is initialized THEN the behavior is set as expanded`() {
        val behavior = BottomSheetBehavior<ConstraintLayout>()

        TabSheetBehaviorManager(behavior, true, 5, 5, mockk())

        assertEquals(STATE_EXPANDED, behavior.state)
    }

    @Test
    fun `GIVEN fewer tabs opened than the expanding limit and landscape orientation WHEN TabSheetBehaviorManager is initialized THEN the behavior is set as expanded`() {
        val behavior = BottomSheetBehavior<ConstraintLayout>()

        TabSheetBehaviorManager(behavior, true, 4, 5, mockk())

        assertEquals(STATE_EXPANDED, behavior.state)
    }
}
