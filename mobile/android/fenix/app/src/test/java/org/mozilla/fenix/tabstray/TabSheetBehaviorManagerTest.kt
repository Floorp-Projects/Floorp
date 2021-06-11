/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

package org.mozilla.fenix.tabstray

import android.content.res.Configuration
import androidx.constraintlayout.widget.ConstraintLayout
import androidx.paging.Config
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
import io.mockk.spyk
import io.mockk.verify
import kotlinx.coroutines.currentCoroutineContext
import org.junit.Assert.assertEquals
import org.junit.Assert.assertFalse
import org.junit.Assert.assertSame
import org.junit.Assert.assertTrue
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

        TabSheetBehaviorManager(behavior, Configuration.ORIENTATION_UNDEFINED, 2, 2, navigationInteractor)

        verify { behavior.addBottomSheetCallback(capture(callbackCaptor)) }
        assertSame(behavior, callbackCaptor.captured.behavior)
        assertSame(navigationInteractor, callbackCaptor.captured.trayInteractor)
    }

    @Test
    fun `WHEN TabSheetBehaviorManager is initialized THEN it caches the orientation parameter value`() {
        val manager0 = TabSheetBehaviorManager(mockk(relaxed = true), Configuration.ORIENTATION_UNDEFINED, 5, 4, mockk())
        assertEquals(Configuration.ORIENTATION_UNDEFINED, manager0.currentOrientation)

        val manager1 = TabSheetBehaviorManager(mockk(relaxed = true), Configuration.ORIENTATION_PORTRAIT, 5, 4, mockk(relaxed = true))
        assertEquals(Configuration.ORIENTATION_PORTRAIT, manager1.currentOrientation)

        val manager2 = TabSheetBehaviorManager(mockk(relaxed = true), Configuration.ORIENTATION_LANDSCAPE, 5, 4, mockk())
        assertEquals(Configuration.ORIENTATION_LANDSCAPE, manager2.currentOrientation)
    }

    @Test
    fun `GIVEN more tabs opened than the expanding limit and portrait orientation WHEN TabSheetBehaviorManager is initialized THEN the behavior is set as expanded`() {
        val behavior = BottomSheetBehavior<ConstraintLayout>()

        TabSheetBehaviorManager(behavior, Configuration.ORIENTATION_PORTRAIT, 5, 4, mockk())

        assertEquals(STATE_EXPANDED, behavior.state)
    }

    @Test
    fun `GIVEN the number of tabs opened is exactly the expanding limit and portrait orientation WHEN TabSheetBehaviorManager is initialized THEN the behavior is set as expanded`() {
        val behavior = BottomSheetBehavior<ConstraintLayout>()

        TabSheetBehaviorManager(behavior, Configuration.ORIENTATION_PORTRAIT, 5, 5, mockk())

        assertEquals(STATE_EXPANDED, behavior.state)
    }

    @Test
    fun `GIVEN fewer tabs opened than the expanding limit and portrait orientation WHEN TabSheetBehaviorManager is initialized THEN the behavior is set as collapsed`() {
        val behavior = BottomSheetBehavior<ConstraintLayout>()

        TabSheetBehaviorManager(behavior, Configuration.ORIENTATION_PORTRAIT, 4, 5, mockk())

        assertEquals(STATE_COLLAPSED, behavior.state)
    }

    @Test
    fun `GIVEN more tabs opened than the expanding limit and undefined orientation WHEN TabSheetBehaviorManager is initialized THEN the behavior is set as expanded`() {
        val behavior = BottomSheetBehavior<ConstraintLayout>()

        TabSheetBehaviorManager(behavior, Configuration.ORIENTATION_UNDEFINED, 5, 4, mockk())

        assertEquals(STATE_EXPANDED, behavior.state)
    }

    @Test
    fun `GIVEN the number of tabs opened is exactly the expanding limit and undefined orientation WHEN TabSheetBehaviorManager is initialized THEN the behavior is set as expanded`() {
        val behavior = BottomSheetBehavior<ConstraintLayout>()

        TabSheetBehaviorManager(behavior, Configuration.ORIENTATION_UNDEFINED, 5, 5, mockk())

        assertEquals(STATE_EXPANDED, behavior.state)
    }

    @Test
    fun `GIVEN fewer tabs opened than the expanding limit and undefined orientation WHEN TabSheetBehaviorManager is initialized THEN the behavior is set as collapsed`() {
        val behavior = BottomSheetBehavior<ConstraintLayout>()

        TabSheetBehaviorManager(behavior, Configuration.ORIENTATION_UNDEFINED, 4, 5, mockk())

        assertEquals(STATE_COLLAPSED, behavior.state)
    }

    @Test
    fun `GIVEN more tabs opened than the expanding limit and landscape orientation WHEN TabSheetBehaviorManager is initialized THEN the behavior is set as expanded`() {
        val behavior = BottomSheetBehavior<ConstraintLayout>()

        TabSheetBehaviorManager(behavior, Configuration.ORIENTATION_LANDSCAPE, 5, 4, mockk())

        assertEquals(STATE_EXPANDED, behavior.state)
    }

    @Test
    fun `GIVEN the number of tabs opened is exactly the expanding limit and landscape orientation WHEN TabSheetBehaviorManager is initialized THEN the behavior is set as expanded`() {
        val behavior = BottomSheetBehavior<ConstraintLayout>()

        TabSheetBehaviorManager(behavior, Configuration.ORIENTATION_LANDSCAPE, 5, 5, mockk())

        assertEquals(STATE_EXPANDED, behavior.state)
    }

    @Test
    fun `GIVEN fewer tabs opened than the expanding limit and landscape orientation WHEN TabSheetBehaviorManager is initialized THEN the behavior is set as expanded`() {
        val behavior = BottomSheetBehavior<ConstraintLayout>()

        TabSheetBehaviorManager(behavior, Configuration.ORIENTATION_LANDSCAPE, 4, 5, mockk())

        assertEquals(STATE_EXPANDED, behavior.state)
    }

    @Test
    fun `GIVEN more tabs opened than the expanding limit and not landscape orientation WHEN updateBehaviorState is called THEN the behavior is set as expanded`() {
        val behavior = BottomSheetBehavior<ConstraintLayout>()
        val manager = TabSheetBehaviorManager(behavior, Configuration.ORIENTATION_UNDEFINED, 5, 4, mockk())

        manager.updateBehaviorState(false)

        assertEquals(STATE_EXPANDED, behavior.state)
    }

    @Test
    fun `GIVEN the number of tabs opened is exactly the expanding limit and portrait orientation WHEN updateBehaviorState is called THEN the behavior is set as expanded`() {
        val behavior = BottomSheetBehavior<ConstraintLayout>()
        val manager = TabSheetBehaviorManager(behavior, Configuration.ORIENTATION_UNDEFINED, 5, 5, mockk())

        manager.updateBehaviorState(false)

        assertEquals(STATE_EXPANDED, behavior.state)
    }

    @Test
    fun `GIVEN fewer tabs opened than the expanding limit and portrait orientation WHEN updateBehaviorState is called THEN the behavior is set as collapsed`() {
        val behavior = BottomSheetBehavior<ConstraintLayout>()
        val manager = TabSheetBehaviorManager(behavior, Configuration.ORIENTATION_UNDEFINED, 4, 5, mockk())

        manager.updateBehaviorState(false)

        assertEquals(STATE_COLLAPSED, behavior.state)
    }

    @Test
    fun `GIVEN more tabs opened than the expanding limit and landscape orientation WHEN updateBehaviorState is called THEN the behavior is set as expanded`() {
        val behavior = BottomSheetBehavior<ConstraintLayout>()
        val manager = TabSheetBehaviorManager(behavior, Configuration.ORIENTATION_UNDEFINED, 5, 4, mockk())

        manager.updateBehaviorState(true)

        assertEquals(STATE_EXPANDED, behavior.state)
    }

    @Test
    fun `GIVEN the number of tabs opened is exactly the expanding limit and landscape orientation WHEN updateBehaviorState is called THEN the behavior is set as expanded`() {
        val behavior = BottomSheetBehavior<ConstraintLayout>()
        val manager = TabSheetBehaviorManager(behavior, Configuration.ORIENTATION_UNDEFINED, 5, 5, mockk())

        manager.updateBehaviorState(true)

        assertEquals(STATE_EXPANDED, behavior.state)
    }

    @Test
    fun `GIVEN fewer tabs opened than the expanding limit and landscape orientation WHEN updateBehaviorState is called THEN the behavior is set as expanded`() {
        val behavior = BottomSheetBehavior<ConstraintLayout>()
        val manager = TabSheetBehaviorManager(behavior, Configuration.ORIENTATION_UNDEFINED, 4, 5, mockk())

        manager.updateBehaviorState(true)

        assertEquals(STATE_EXPANDED, behavior.state)
    }

    @Test
    fun `WHEN updateDependingOnOrientation is called with the same orientation as the current one THEN nothing happens`() {
        val manager = spyk(TabSheetBehaviorManager(mockk(relaxed = true), Configuration.ORIENTATION_PORTRAIT, 4, 5, mockk()))

        manager.updateDependingOnOrientation(Configuration.ORIENTATION_PORTRAIT)

        verify(exactly = 0) { manager.currentOrientation = any() }
        verify(exactly = 0) { manager.updateBehaviorState(any()) }
    }

    @Test
    fun `WHEN updateDependingOnOrientation is called with a new orientation THEN this is cached and updateBehaviorState is called`() {
        val manager = spyk(TabSheetBehaviorManager(mockk(relaxed = true), Configuration.ORIENTATION_PORTRAIT, 4, 5, mockk()))

        manager.updateDependingOnOrientation(Configuration.ORIENTATION_UNDEFINED)
        assertEquals(Configuration.ORIENTATION_UNDEFINED, manager.currentOrientation)
        verify { manager.updateBehaviorState(any()) }

        manager.updateDependingOnOrientation(Configuration.ORIENTATION_LANDSCAPE)
        assertEquals(Configuration.ORIENTATION_LANDSCAPE, manager.currentOrientation)
        verify(exactly = 2) { manager.updateBehaviorState(any()) }
    }

    @Test
    fun `WHEN isLandscape is called with Configuration#ORIENTATION_LANDSCAPE THEN it returns true`() {
        val manager = spyk(TabSheetBehaviorManager(mockk(relaxed = true), Configuration.ORIENTATION_PORTRAIT, 4, 5, mockk()))

        assertTrue(manager.isLandscape(Configuration.ORIENTATION_LANDSCAPE))
    }

    @Test
    fun `WHEN isLandscape is called with Configuration#ORIENTATION_PORTRAIT THEN it returns false`() {
        val manager = spyk(TabSheetBehaviorManager(mockk(relaxed = true), Configuration.ORIENTATION_PORTRAIT, 4, 5, mockk()))

        assertFalse(manager.isLandscape(Configuration.ORIENTATION_PORTRAIT))
    }

    @Test
    fun `WHEN isLandscape is called with Configuration#ORIENTATION_UNDEFINED THEN it returns false`() {
        val manager = spyk(TabSheetBehaviorManager(mockk(relaxed = true), Configuration.ORIENTATION_PORTRAIT, 4, 5, mockk()))

        assertFalse(manager.isLandscape(Configuration.ORIENTATION_UNDEFINED))
    }
}
