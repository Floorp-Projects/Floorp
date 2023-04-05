/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

package org.mozilla.fenix.tabstray

import android.content.res.Configuration
import android.util.DisplayMetrics
import android.view.View
import androidx.constraintlayout.widget.ConstraintLayout
import com.google.android.material.bottomsheet.BottomSheetBehavior
import com.google.android.material.bottomsheet.BottomSheetBehavior.STATE_COLLAPSED
import com.google.android.material.bottomsheet.BottomSheetBehavior.STATE_DRAGGING
import com.google.android.material.bottomsheet.BottomSheetBehavior.STATE_EXPANDED
import com.google.android.material.bottomsheet.BottomSheetBehavior.STATE_HALF_EXPANDED
import com.google.android.material.bottomsheet.BottomSheetBehavior.STATE_HIDDEN
import com.google.android.material.bottomsheet.BottomSheetBehavior.STATE_SETTLING
import com.google.android.material.floatingactionbutton.ExtendedFloatingActionButton
import io.mockk.Called
import io.mockk.every
import io.mockk.mockk
import io.mockk.mockkStatic
import io.mockk.spyk
import io.mockk.unmockkStatic
import io.mockk.verify
import mozilla.components.support.ktx.android.util.dpToPx
import org.junit.Assert.assertEquals
import org.junit.Assert.assertFalse
import org.junit.Assert.assertNull
import org.junit.Assert.assertTrue
import org.junit.Test

class TabSheetBehaviorManagerTest {

    @Test
    fun `WHEN state is hidden THEN invoke interactor`() {
        val interactor = mockk<NavigationInteractor>(relaxed = true)
        val callback = TraySheetBehaviorCallback(mockk(), interactor, mockk(), mockk())

        callback.onStateChanged(mockk(), STATE_HIDDEN)

        verify { interactor.onTabTrayDismissed() }
    }

    @Test
    fun `WHEN state is half-expanded THEN close the tray`() {
        val behavior = mockk<BottomSheetBehavior<ConstraintLayout>>(relaxed = true)
        val callback = TraySheetBehaviorCallback(behavior, mockk(), mockk(), mockk())

        callback.onStateChanged(mockk(), STATE_HALF_EXPANDED)

        verify { behavior.state = STATE_HIDDEN }
    }

    @Test
    fun `WHEN other states are invoked THEN do nothing`() {
        val behavior = mockk<BottomSheetBehavior<ConstraintLayout>>(relaxed = true)
        val interactor = mockk<NavigationInteractor>(relaxed = true)
        val callback = TraySheetBehaviorCallback(behavior, interactor, mockk(), mockk())

        callback.onStateChanged(mockk(), STATE_COLLAPSED)
        callback.onStateChanged(mockk(), STATE_DRAGGING)
        callback.onStateChanged(mockk(), STATE_SETTLING)
        callback.onStateChanged(mockk(), STATE_EXPANDED)

        verify { behavior wasNot Called }
        verify { interactor wasNot Called }
    }

    @Test
    fun `GIVEN converted dim value is more than max dim WHEN onSlide is called THEN it does not set the dialog dim amount`() {
        val behavior = mockk<BottomSheetBehavior<ConstraintLayout>>(relaxed = true)

        val tabsTrayDialog = mockk<TabsTrayDialog>(relaxed = true)

        val rootView = mockk<View>(relaxed = true)
        every { rootView.bottom } returns 1000
        val fab = mockk<ExtendedFloatingActionButton>(relaxed = true)
        every { fab.rootView } returns rootView

        val callback = TraySheetBehaviorCallback(behavior, mockk(), tabsTrayDialog, fab)

        val bottomSheet = mockk<View>(relaxed = true)
        every { bottomSheet.top } returns 300 // (1000 - 300) / 1000 = 0.7

        callback.onSlide(bottomSheet, 1f)

        verify(exactly = 0) { tabsTrayDialog.window?.setDimAmount(any()) }
    }

    @Test
    fun `GIVEN converted dim value is at max dim WHEN onSlide is called THEN it does not set the dialog dim amount`() {
        val behavior = mockk<BottomSheetBehavior<ConstraintLayout>>(relaxed = true)

        val tabsTrayDialog = mockk<TabsTrayDialog>(relaxed = true)

        val rootView = mockk<View>(relaxed = true)
        every { rootView.bottom } returns 1000
        val fab = mockk<ExtendedFloatingActionButton>(relaxed = true)
        every { fab.rootView } returns rootView

        val callback = TraySheetBehaviorCallback(behavior, mockk(), tabsTrayDialog, fab)

        val bottomSheet = mockk<View>(relaxed = true)
        every { bottomSheet.top } returns 400 // (1000 - 400) / 1000 = 0.6

        callback.onSlide(bottomSheet, 1f)

        verify(exactly = 0) { tabsTrayDialog.window?.setDimAmount(any()) }
    }

    @Test
    fun `GIVEN converted dim value is less than max dim WHEN onSlide is called THEN it sets the dialog dim amount`() {
        val behavior = mockk<BottomSheetBehavior<ConstraintLayout>>(relaxed = true)

        val tabsTrayDialog = mockk<TabsTrayDialog>(relaxed = true)

        val rootView = mockk<View>(relaxed = true)
        every { rootView.bottom } returns 1000
        val fab = mockk<ExtendedFloatingActionButton>(relaxed = true)
        every { fab.rootView } returns rootView

        val callback = TraySheetBehaviorCallback(behavior, mockk(), tabsTrayDialog, fab)

        val bottomSheet = mockk<View>(relaxed = true)
        every { bottomSheet.top } returns 500 // (1000 - 500) / 1000 = 0.5

        callback.onSlide(bottomSheet, 1f)

        verify(exactly = 1) { tabsTrayDialog.window?.setDimAmount(any()) }
    }

    @Test
    fun `GIVEN behaviour state is 'dragging' & draggedLowestSheetTop is null WHEN onSlide is called THEN draggedLowestSheetTop is set to currentBottomSheetTop`() {
        val behavior = mockk<BottomSheetBehavior<ConstraintLayout>>(relaxed = true)
        every { behavior.state } returns STATE_DRAGGING

        val tabsTrayDialog = mockk<TabsTrayDialog>(relaxed = true)

        val rootView = mockk<View>(relaxed = true)
        every { rootView.bottom } returns 1000
        val fab = mockk<ExtendedFloatingActionButton>(relaxed = true)
        every { fab.rootView } returns rootView
        every { fab.top } returns 900

        val callback = TraySheetBehaviorCallback(behavior, mockk(), tabsTrayDialog, fab)

        val bottomSheetTop = 10

        // draggedLowestSheetTop is null
        val bottomSheet = mockk<View>(relaxed = true)
        every { bottomSheet.top } returns bottomSheetTop
        callback.onSlide(bottomSheet, 1f)
        verify { fab.y = 900f }

        assertEquals(bottomSheetTop, callback.draggedLowestSheetTop)
    }

    @Test
    fun `GIVEN behaviour state is 'dragging' & currentBottomSheetTop is same as draggedLowestSheetTop WHEN onSlide is called THEN draggedLowestSheetTop is same value`() {
        val behavior = mockk<BottomSheetBehavior<ConstraintLayout>>(relaxed = true)
        every { behavior.state } returns STATE_DRAGGING

        val tabsTrayDialog = mockk<TabsTrayDialog>(relaxed = true)

        val rootView = mockk<View>(relaxed = true)
        every { rootView.bottom } returns 1000
        val fab = mockk<ExtendedFloatingActionButton>(relaxed = true)
        every { fab.rootView } returns rootView
        every { fab.top } returns 900

        val callback = TraySheetBehaviorCallback(behavior, mockk(), tabsTrayDialog, fab)

        val bottomSheetTop = 10

        // draggedLowestSheetTop is null
        val bottomSheet1 = mockk<View>(relaxed = true)
        every { bottomSheet1.top } returns bottomSheetTop
        callback.onSlide(bottomSheet1, 1f)
        verify { fab.y = 900f }

        // currentBottomSheetTop is same as draggedLowestSheetTop
        val bottomSheet2 = mockk<View>(relaxed = true)
        every { bottomSheet2.top } returns bottomSheetTop
        callback.onSlide(bottomSheet2, 1f)
        verify { fab.y = 900f }

        assertEquals(bottomSheetTop, callback.draggedLowestSheetTop)
    }

    @Test
    fun `GIVEN behaviour state is 'dragging' & currentBottomSheetTop is more than draggedLowestSheetTop WHEN onSlide is called THEN draggedLowestSheetTop is same value`() {
        val behavior = mockk<BottomSheetBehavior<ConstraintLayout>>(relaxed = true)
        every { behavior.state } returns STATE_DRAGGING

        val tabsTrayDialog = mockk<TabsTrayDialog>(relaxed = true)

        val rootView = mockk<View>(relaxed = true)
        every { rootView.bottom } returns 1000
        val fab = mockk<ExtendedFloatingActionButton>(relaxed = true)
        every { fab.rootView } returns rootView
        every { fab.top } returns 900

        val callback = TraySheetBehaviorCallback(behavior, mockk(), tabsTrayDialog, fab)

        val originalBottomSheetTop = 10

        // draggedLowestSheetTop is null
        val bottomSheet1 = mockk<View>(relaxed = true)
        every { bottomSheet1.top } returns originalBottomSheetTop
        callback.onSlide(bottomSheet1, 1f)
        verify { fab.y = 900f }

        // currentBottomSheetTop is same as draggedLowestSheetTop
        val newBottomSheetTop = originalBottomSheetTop + 1
        val bottomSheet2 = mockk<View>(relaxed = true)
        every { bottomSheet2.top } returns newBottomSheetTop
        callback.onSlide(bottomSheet2, 1f)
        verify { fab.y = 901f }

        assertEquals(originalBottomSheetTop, callback.draggedLowestSheetTop)
    }

    @Test
    fun `GIVEN behaviour state is 'dragging' & currentBottomSheetTop less than draggedLowestSheetTop WHEN onSlide is called THEN draggedLowestSheetTop is set to currentBottomSheetTop`() {
        val behavior = mockk<BottomSheetBehavior<ConstraintLayout>>(relaxed = true)
        every { behavior.state } returns STATE_DRAGGING

        val tabsTrayDialog = mockk<TabsTrayDialog>(relaxed = true)

        val rootView = mockk<View>(relaxed = true)
        every { rootView.bottom } returns 1000
        val fab = mockk<ExtendedFloatingActionButton>(relaxed = true)
        every { fab.rootView } returns rootView
        every { fab.top } returns 900

        val callback = TraySheetBehaviorCallback(behavior, mockk(), tabsTrayDialog, fab)

        // draggedLowestSheetTop is null
        val bottomSheet1 = mockk<View>(relaxed = true)
        every { bottomSheet1.top } returns 10
        callback.onSlide(bottomSheet1, 1f)
        verify { fab.y = 900f }

        // currentBottomSheetTop is less than draggedLowestSheetTop
        val newBottomSheetTop = 9
        val bottomSheet2 = mockk<View>(relaxed = true)
        every { bottomSheet2.top } returns newBottomSheetTop
        callback.onSlide(bottomSheet2, 1f)
        verify { fab.y = 900f }

        assertEquals(newBottomSheetTop, callback.draggedLowestSheetTop)
    }

    @Test
    fun `GIVEN behaviour state is 'settling' & draggedLowestSheetTop is null WHEN onSlide is called THEN draggedLowestSheetTop is set to fab y is set`() {
        val behavior = mockk<BottomSheetBehavior<ConstraintLayout>>(relaxed = true)
        every { behavior.state } returns STATE_SETTLING

        val tabsTrayDialog = mockk<TabsTrayDialog>(relaxed = true)

        val rootView = mockk<View>(relaxed = true)
        every { rootView.bottom } returns 1000
        val fab = mockk<ExtendedFloatingActionButton>(relaxed = true)
        every { fab.rootView } returns rootView
        every { fab.top } returns 900

        val callback = TraySheetBehaviorCallback(behavior, mockk(), tabsTrayDialog, fab)

        // draggedLowestSheetTop is null
        val bottomSheet = mockk<View>(relaxed = true)
        every { bottomSheet.top } returns 10
        callback.onSlide(bottomSheet, 1f)
        verify { fab.y = 900f }

        assertNull(callback.draggedLowestSheetTop)
    }

    @Test
    fun `GIVEN behaviour state is 'settling' & currentBottomSheetTop is same as draggedLowestSheetTop WHEN onSlide is called THEN fab y is set`() {
        val behavior = mockk<BottomSheetBehavior<ConstraintLayout>>(relaxed = true)
        every { behavior.state } returns STATE_SETTLING

        val tabsTrayDialog = mockk<TabsTrayDialog>(relaxed = true)

        val rootView = mockk<View>(relaxed = true)
        every { rootView.bottom } returns 1000
        val fab = mockk<ExtendedFloatingActionButton>(relaxed = true)
        every { fab.rootView } returns rootView
        every { fab.top } returns 900

        val callback = TraySheetBehaviorCallback(behavior, mockk(), tabsTrayDialog, fab)

        val bottomSheetTop = 10

        // draggedLowestSheetTop is null
        val bottomSheet1 = mockk<View>(relaxed = true)
        every { bottomSheet1.top } returns bottomSheetTop
        callback.onSlide(bottomSheet1, 1f)
        verify { fab.y = 900f }

        // currentBottomSheetTop is same as draggedLowestSheetTop
        val bottomSheet2 = mockk<View>(relaxed = true)
        every { bottomSheet2.top } returns bottomSheetTop
        callback.onSlide(bottomSheet2, 1f)
        verify { fab.y = 900f }

        assertNull(callback.draggedLowestSheetTop)
    }

    @Test
    fun `GIVEN behaviour state is 'settling' & currentBottomSheetTop is more than draggedLowestSheetTop WHEN onSlide is called THEN fab y is set`() {
        val behavior = mockk<BottomSheetBehavior<ConstraintLayout>>(relaxed = true)
        every { behavior.state } returns STATE_SETTLING

        val tabsTrayDialog = mockk<TabsTrayDialog>(relaxed = true)

        val rootView = mockk<View>(relaxed = true)
        every { rootView.bottom } returns 1000
        val fab = mockk<ExtendedFloatingActionButton>(relaxed = true)
        every { fab.rootView } returns rootView
        every { fab.top } returns 900

        val callback = TraySheetBehaviorCallback(behavior, mockk(), tabsTrayDialog, fab)

        // draggedLowestSheetTop is null
        val bottomSheet1 = mockk<View>(relaxed = true)
        every { bottomSheet1.top } returns 10
        callback.onSlide(bottomSheet1, 1f)
        verify { fab.y = 900f }

        // currentBottomSheetTop is same as draggedLowestSheetTop
        val bottomSheet2 = mockk<View>(relaxed = true)
        every { bottomSheet2.top } returns 11
        callback.onSlide(bottomSheet2, 1f)
        verify { fab.y = 900f }

        assertNull(callback.draggedLowestSheetTop)
    }

    @Test
    fun `GIVEN behaviour state is 'settling' & currentBottomSheetTop less than draggedLowestSheetTop WHEN onSlide is called THEN fab y is set`() {
        val behavior = mockk<BottomSheetBehavior<ConstraintLayout>>(relaxed = true)
        every { behavior.state } returns STATE_SETTLING

        val tabsTrayDialog = mockk<TabsTrayDialog>(relaxed = true)

        val rootView = mockk<View>(relaxed = true)
        every { rootView.bottom } returns 1000
        val fab = mockk<ExtendedFloatingActionButton>(relaxed = true)
        every { fab.rootView } returns rootView
        every { fab.top } returns 900

        val callback = TraySheetBehaviorCallback(behavior, mockk(), tabsTrayDialog, fab)

        // draggedLowestSheetTop is null
        val bottomSheet1 = mockk<View>(relaxed = true)
        every { bottomSheet1.top } returns 10
        callback.onSlide(bottomSheet1, 1f)
        verify { fab.y = 900f }

        // currentBottomSheetTop is less than draggedLowestSheetTop
        val bottomSheet2 = mockk<View>(relaxed = true)
        every { bottomSheet2.top } returns 9
        callback.onSlide(bottomSheet2, 1f)
        verify { fab.y = 900f }

        assertNull(callback.draggedLowestSheetTop)
    }

    @Test
    fun `GIVEN behaviour state is 'state expanded' WHEN onStateChanged is called THEN draggedLowestSheetTop is set to null`() {
        val bottomSheet = mockk<View>(relaxed = true)
        val callback = TraySheetBehaviorCallback(mockk(), mockk(), mockk(), mockk())

        callback.draggedLowestSheetTop = 1
        assertEquals(1, callback.draggedLowestSheetTop)

        callback.onStateChanged(bottomSheet, STATE_EXPANDED)

        assertNull(callback.draggedLowestSheetTop)
    }

    @Test
    fun `GIVEN behaviour state is 'state collapsed' WHEN onStateChanged is called THEN draggedLowestSheetTop is set to null`() {
        val bottomSheet = mockk<View>(relaxed = true)
        val callback = TraySheetBehaviorCallback(mockk(), mockk(), mockk(), mockk())

        callback.draggedLowestSheetTop = 1
        assertEquals(1, callback.draggedLowestSheetTop)

        callback.onStateChanged(bottomSheet, STATE_COLLAPSED)

        assertNull(callback.draggedLowestSheetTop)
    }

    @Test
    fun `WHEN TabSheetBehaviorManager is initialized THEN it caches the orientation parameter value`() {
        val manager0 = TabSheetBehaviorManager(
            mockk(relaxed = true),
            Configuration.ORIENTATION_UNDEFINED,
            5,
            4,
            mockk(),
        )
        assertEquals(Configuration.ORIENTATION_UNDEFINED, manager0.currentOrientation)

        val manager1 = TabSheetBehaviorManager(
            mockk(relaxed = true),
            Configuration.ORIENTATION_PORTRAIT,
            5,
            4,
            mockk(),
        )
        assertEquals(Configuration.ORIENTATION_PORTRAIT, manager1.currentOrientation)

        val manager2 = TabSheetBehaviorManager(
            mockk(relaxed = true),
            Configuration.ORIENTATION_LANDSCAPE,
            5,
            4,
            mockk(),
        )
        assertEquals(Configuration.ORIENTATION_LANDSCAPE, manager2.currentOrientation)
    }

    @Test
    fun `GIVEN more tabs opened than the expanding limit and portrait orientation WHEN TabSheetBehaviorManager is initialized THEN the behavior is set as expanded`() {
        val behavior = BottomSheetBehavior<ConstraintLayout>()

        TabSheetBehaviorManager(
            behavior,
            Configuration.ORIENTATION_PORTRAIT,
            5,
            4,
            mockk(),
        )

        assertEquals(STATE_EXPANDED, behavior.state)
    }

    @Test
    fun `GIVEN the number of tabs opened is exactly the expanding limit and portrait orientation WHEN TabSheetBehaviorManager is initialized THEN the behavior is set as expanded`() {
        val behavior = BottomSheetBehavior<ConstraintLayout>()

        TabSheetBehaviorManager(
            behavior,
            Configuration.ORIENTATION_PORTRAIT,
            5,
            5,
            mockk(),
        )

        assertEquals(STATE_EXPANDED, behavior.state)
    }

    @Test
    fun `GIVEN fewer tabs opened than the expanding limit and portrait orientation WHEN TabSheetBehaviorManager is initialized THEN the behavior is set as collapsed`() {
        val behavior = BottomSheetBehavior<ConstraintLayout>()

        TabSheetBehaviorManager(
            behavior,
            Configuration.ORIENTATION_PORTRAIT,
            4,
            5,
            mockk(),
        )

        assertEquals(STATE_COLLAPSED, behavior.state)
    }

    @Test
    fun `GIVEN more tabs opened than the expanding limit and undefined orientation WHEN TabSheetBehaviorManager is initialized THEN the behavior is set as expanded`() {
        val behavior = BottomSheetBehavior<ConstraintLayout>()

        TabSheetBehaviorManager(
            behavior,
            Configuration.ORIENTATION_UNDEFINED,
            5,
            4,
            mockk(),
        )

        assertEquals(STATE_EXPANDED, behavior.state)
    }

    @Test
    fun `GIVEN the number of tabs opened is exactly the expanding limit and undefined orientation WHEN TabSheetBehaviorManager is initialized THEN the behavior is set as expanded`() {
        val behavior = BottomSheetBehavior<ConstraintLayout>()

        TabSheetBehaviorManager(
            behavior,
            Configuration.ORIENTATION_UNDEFINED,
            5,
            5,
            mockk(),
        )

        assertEquals(STATE_EXPANDED, behavior.state)
    }

    @Test
    fun `GIVEN fewer tabs opened than the expanding limit and undefined orientation WHEN TabSheetBehaviorManager is initialized THEN the behavior is set as collapsed`() {
        val behavior = BottomSheetBehavior<ConstraintLayout>()

        TabSheetBehaviorManager(
            behavior,
            Configuration.ORIENTATION_UNDEFINED,
            4,
            5,
            mockk(),
        )

        assertEquals(STATE_COLLAPSED, behavior.state)
    }

    @Test
    fun `GIVEN more tabs opened than the expanding limit and landscape orientation WHEN TabSheetBehaviorManager is initialized THEN the behavior is set as expanded`() {
        val behavior = BottomSheetBehavior<ConstraintLayout>()

        TabSheetBehaviorManager(
            behavior,
            Configuration.ORIENTATION_LANDSCAPE,
            5,
            4,
            mockk(),
        )

        assertEquals(STATE_EXPANDED, behavior.state)
    }

    @Test
    fun `GIVEN the number of tabs opened is exactly the expanding limit and landscape orientation WHEN TabSheetBehaviorManager is initialized THEN the behavior is set as expanded`() {
        val behavior = BottomSheetBehavior<ConstraintLayout>()

        TabSheetBehaviorManager(
            behavior,
            Configuration.ORIENTATION_LANDSCAPE,
            5,
            5,
            mockk(),
        )

        assertEquals(STATE_EXPANDED, behavior.state)
    }

    @Test
    fun `GIVEN fewer tabs opened than the expanding limit and landscape orientation WHEN TabSheetBehaviorManager is initialized THEN the behavior is set as expanded`() {
        val behavior = BottomSheetBehavior<ConstraintLayout>()

        TabSheetBehaviorManager(
            behavior,
            Configuration.ORIENTATION_LANDSCAPE,
            4,
            5,
            mockk(),
        )

        assertEquals(STATE_EXPANDED, behavior.state)
    }

    @Test
    fun `GIVEN more tabs opened than the expanding limit and not landscape orientation WHEN updateBehaviorState is called THEN the behavior is set as expanded`() {
        val behavior = BottomSheetBehavior<ConstraintLayout>()
        val manager = TabSheetBehaviorManager(
            behavior,
            Configuration.ORIENTATION_UNDEFINED,
            5,
            4,
            mockk(),
        )

        manager.updateBehaviorState(false)

        assertEquals(STATE_EXPANDED, behavior.state)
    }

    @Test
    fun `GIVEN the number of tabs opened is exactly the expanding limit and portrait orientation WHEN updateBehaviorState is called THEN the behavior is set as expanded`() {
        val behavior = BottomSheetBehavior<ConstraintLayout>()
        val manager = TabSheetBehaviorManager(
            behavior,
            Configuration.ORIENTATION_UNDEFINED,
            5,
            5,
            mockk(),
        )

        manager.updateBehaviorState(false)

        assertEquals(STATE_EXPANDED, behavior.state)
    }

    @Test
    fun `GIVEN fewer tabs opened than the expanding limit and portrait orientation WHEN updateBehaviorState is called THEN the behavior is set as collapsed`() {
        val behavior = BottomSheetBehavior<ConstraintLayout>()
        val manager = TabSheetBehaviorManager(
            behavior,
            Configuration.ORIENTATION_UNDEFINED,
            4,
            5,
            mockk(),
        )

        manager.updateBehaviorState(false)

        assertEquals(STATE_COLLAPSED, behavior.state)
    }

    @Test
    fun `GIVEN more tabs opened than the expanding limit and landscape orientation WHEN updateBehaviorState is called THEN the behavior is set as expanded`() {
        val behavior = BottomSheetBehavior<ConstraintLayout>()
        val manager = TabSheetBehaviorManager(
            behavior,
            Configuration.ORIENTATION_UNDEFINED,
            5,
            4,
            mockk(),
        )

        manager.updateBehaviorState(true)

        assertEquals(STATE_EXPANDED, behavior.state)
    }

    @Test
    fun `GIVEN the number of tabs opened is exactly the expanding limit and landscape orientation WHEN updateBehaviorState is called THEN the behavior is set as expanded`() {
        val behavior = BottomSheetBehavior<ConstraintLayout>()
        val manager = TabSheetBehaviorManager(
            behavior,
            Configuration.ORIENTATION_UNDEFINED,
            5,
            5,
            mockk(),
        )

        manager.updateBehaviorState(true)

        assertEquals(STATE_EXPANDED, behavior.state)
    }

    @Test
    fun `GIVEN fewer tabs opened than the expanding limit and landscape orientation WHEN updateBehaviorState is called THEN the behavior is set as expanded`() {
        val behavior = BottomSheetBehavior<ConstraintLayout>()
        val manager = TabSheetBehaviorManager(
            behavior,
            Configuration.ORIENTATION_UNDEFINED,
            4,
            5,
            mockk(),
        )

        manager.updateBehaviorState(true)

        assertEquals(STATE_EXPANDED, behavior.state)
    }

    @Test
    fun `WHEN updateDependingOnOrientation is called with the same orientation as the current one THEN nothing happens`() {
        val manager = spyk(
            TabSheetBehaviorManager(
                mockk(relaxed = true),
                Configuration.ORIENTATION_PORTRAIT,
                4,
                5,
                mockk(),
            ),
        )

        manager.updateDependingOnOrientation(Configuration.ORIENTATION_PORTRAIT)

        verify(exactly = 0) { manager.currentOrientation = any() }
        verify(exactly = 0) { manager.updateBehaviorExpandedOffset(any()) }
        verify(exactly = 0) { manager.updateBehaviorState(any()) }
    }

    @Test
    fun `WHEN updateDependingOnOrientation is called with a new orientation THEN this is cached and updateBehaviorState is called`() {
        val manager = spyk(
            TabSheetBehaviorManager(
                mockk(relaxed = true),
                Configuration.ORIENTATION_PORTRAIT,
                4,
                5,
                mockk(),
            ),
        )

        manager.updateDependingOnOrientation(Configuration.ORIENTATION_UNDEFINED)
        assertEquals(Configuration.ORIENTATION_UNDEFINED, manager.currentOrientation)
        verify { manager.updateBehaviorExpandedOffset(any()) }
        verify { manager.updateBehaviorState(any()) }

        manager.updateDependingOnOrientation(Configuration.ORIENTATION_LANDSCAPE)
        assertEquals(Configuration.ORIENTATION_LANDSCAPE, manager.currentOrientation)
        verify(exactly = 2) { manager.updateBehaviorExpandedOffset(any()) }
        verify(exactly = 2) { manager.updateBehaviorState(any()) }
    }

    @Test
    fun `WHEN isLandscape is called with Configuration#ORIENTATION_LANDSCAPE THEN it returns true`() {
        val manager = spyk(
            TabSheetBehaviorManager(
                mockk(relaxed = true),
                Configuration.ORIENTATION_PORTRAIT,
                4,
                5,
                mockk(),
            ),
        )

        assertTrue(manager.isLandscape(Configuration.ORIENTATION_LANDSCAPE))
    }

    @Test
    fun `WHEN isLandscape is called with Configuration#ORIENTATION_PORTRAIT THEN it returns false`() {
        val manager = spyk(
            TabSheetBehaviorManager(
                mockk(relaxed = true),
                Configuration.ORIENTATION_PORTRAIT,
                4,
                5,
                mockk(),
            ),
        )

        assertFalse(manager.isLandscape(Configuration.ORIENTATION_PORTRAIT))
    }

    @Test
    fun `WHEN isLandscape is called with Configuration#ORIENTATION_UNDEFINED THEN it returns false`() {
        val manager = spyk(
            TabSheetBehaviorManager(
                mockk(relaxed = true),
                Configuration.ORIENTATION_PORTRAIT,
                4,
                5,
                mockk(),
            ),
        )

        assertFalse(manager.isLandscape(Configuration.ORIENTATION_UNDEFINED))
    }

    @Test
    fun `GIVEN a behavior and landscape orientation WHEN TabSheetBehaviorManager is initialized THEN it sets the behavior expandedOffset to 0`() {
        val behavior = BottomSheetBehavior<ConstraintLayout>()
        // expandedOffset is only used if isFitToContents == false
        behavior.isFitToContents = false
        val displayMetrics: DisplayMetrics = mockk()

        try {
            mockkStatic("mozilla.components.support.ktx.android.util.DisplayMetricsKt")
            every { EXPANDED_OFFSET_IN_LANDSCAPE_DP.dpToPx(displayMetrics) } returns EXPANDED_OFFSET_IN_LANDSCAPE_DP

            TabSheetBehaviorManager(
                behavior,
                Configuration.ORIENTATION_LANDSCAPE,
                5,
                4,
                displayMetrics,
            )
        } finally {
            unmockkStatic("mozilla.components.support.ktx.android.util.DisplayMetricsKt")
        }

        assertEquals(0, behavior.expandedOffset)
    }

    @Test
    fun `GIVEN a behavior and portrait orientation WHEN TabSheetBehaviorManager is initialized THEN it sets the behavior expandedOffset to 40`() {
        val behavior = BottomSheetBehavior<ConstraintLayout>()
        // expandedOffset is only used if isFitToContents == false
        behavior.isFitToContents = false
        val displayMetrics: DisplayMetrics = mockk()

        try {
            mockkStatic("mozilla.components.support.ktx.android.util.DisplayMetricsKt")
            every { EXPANDED_OFFSET_IN_PORTRAIT_DP.dpToPx(displayMetrics) } returns EXPANDED_OFFSET_IN_PORTRAIT_DP

            TabSheetBehaviorManager(
                behavior,
                Configuration.ORIENTATION_PORTRAIT,
                5,
                4,
                displayMetrics,
            )
        } finally {
            unmockkStatic("mozilla.components.support.ktx.android.util.DisplayMetricsKt")
        }

        assertEquals(40, behavior.expandedOffset)
    }

    @Test
    fun `GIVEN a behavior and undefined orientation WHEN TabSheetBehaviorManager is initialized THEN it sets the behavior expandedOffset to 40`() {
        val behavior = BottomSheetBehavior<ConstraintLayout>()
        // expandedOffset is only used if isFitToContents == false
        behavior.isFitToContents = false
        val displayMetrics: DisplayMetrics = mockk()

        try {
            mockkStatic("mozilla.components.support.ktx.android.util.DisplayMetricsKt")
            every { EXPANDED_OFFSET_IN_PORTRAIT_DP.dpToPx(displayMetrics) } returns EXPANDED_OFFSET_IN_PORTRAIT_DP

            TabSheetBehaviorManager(
                behavior,
                Configuration.ORIENTATION_UNDEFINED,
                5,
                4,
                displayMetrics,
            )
        } finally {
            unmockkStatic("mozilla.components.support.ktx.android.util.DisplayMetricsKt")
        }

        assertEquals(40, behavior.expandedOffset)
    }

    @Test
    fun `WHEN updateBehaviorExpandedOffset is called with a portrait parameter THEN it sets expandedOffset to be 40 dp`() {
        val behavior = BottomSheetBehavior<ConstraintLayout>()
        // expandedOffset is only used if isFitToContents == false
        behavior.isFitToContents = false
        val displayMetrics: DisplayMetrics = mockk()

        try {
            mockkStatic("mozilla.components.support.ktx.android.util.DisplayMetricsKt")
            every { EXPANDED_OFFSET_IN_PORTRAIT_DP.dpToPx(displayMetrics) } returns EXPANDED_OFFSET_IN_PORTRAIT_DP
            val manager = TabSheetBehaviorManager(
                behavior,
                Configuration.ORIENTATION_LANDSCAPE,
                5,
                4,
                displayMetrics,
            )

            manager.updateDependingOnOrientation(Configuration.ORIENTATION_PORTRAIT)
        } finally {
            unmockkStatic("mozilla.components.support.ktx.android.util.DisplayMetricsKt")
        }

        assertEquals(40, behavior.expandedOffset)
    }

    @Test
    fun `WHEN updateBehaviorExpandedOffset is called with a undefined parameter THEN it sets expandedOffset to be 40 dp`() {
        val behavior = BottomSheetBehavior<ConstraintLayout>()
        // expandedOffset is only used if isFitToContents == false
        behavior.isFitToContents = false
        val displayMetrics: DisplayMetrics = mockk()

        try {
            mockkStatic("mozilla.components.support.ktx.android.util.DisplayMetricsKt")
            every { EXPANDED_OFFSET_IN_PORTRAIT_DP.dpToPx(displayMetrics) } returns EXPANDED_OFFSET_IN_PORTRAIT_DP
            val manager = TabSheetBehaviorManager(
                behavior,
                Configuration.ORIENTATION_LANDSCAPE,
                5,
                4,
                displayMetrics,
            )

            manager.updateDependingOnOrientation(Configuration.ORIENTATION_UNDEFINED)
        } finally {
            unmockkStatic("mozilla.components.support.ktx.android.util.DisplayMetricsKt")
        }

        assertEquals(40, behavior.expandedOffset)
    }

    @Test
    fun `WHEN updateBehaviorExpandedOffset is called with a landscape parameter THEN it sets expandedOffset to be 0 dp`() {
        val behavior = BottomSheetBehavior<ConstraintLayout>()
        // expandedOffset is only used if isFitToContents == false
        behavior.isFitToContents = false
        val displayMetrics: DisplayMetrics = mockk()

        try {
            mockkStatic("mozilla.components.support.ktx.android.util.DisplayMetricsKt")
            every { EXPANDED_OFFSET_IN_LANDSCAPE_DP.dpToPx(displayMetrics) } returns EXPANDED_OFFSET_IN_LANDSCAPE_DP
            val manager = TabSheetBehaviorManager(
                behavior,
                Configuration.ORIENTATION_UNDEFINED,
                5,
                4,
                displayMetrics,
            )

            manager.updateDependingOnOrientation(Configuration.ORIENTATION_LANDSCAPE)
        } finally {
            unmockkStatic("mozilla.components.support.ktx.android.util.DisplayMetricsKt")
        }

        assertEquals(0, behavior.expandedOffset)
    }
}
