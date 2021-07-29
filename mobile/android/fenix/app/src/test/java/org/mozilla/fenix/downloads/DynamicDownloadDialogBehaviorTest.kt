/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

package org.mozilla.fenix.downloads

import android.animation.ValueAnimator
import android.view.View
import androidx.core.view.ViewCompat
import io.mockk.Runs
import io.mockk.every
import io.mockk.just
import io.mockk.mockk
import io.mockk.spyk
import io.mockk.verify
import mozilla.components.concept.engine.EngineView
import mozilla.components.concept.engine.InputResultDetail
import mozilla.components.support.test.robolectric.testContext
import org.junit.Assert.assertFalse
import org.junit.Assert.assertTrue
import org.junit.Test
import org.junit.runner.RunWith
import org.mozilla.fenix.helpers.FenixRobolectricTestRunner

@RunWith(FenixRobolectricTestRunner::class)
class DynamicDownloadDialogBehaviorTest {

    @Test
    fun `Starting a nested scroll should cancel an ongoing snap animation`() {
        val behavior = spyk(DynamicDownloadDialogBehavior<View>(testContext, attrs = null))
        every { behavior.shouldScroll } returns true

        val animator: ValueAnimator = mockk(relaxed = true)
        behavior.snapAnimator = animator

        val acceptsNestedScroll = behavior.onStartNestedScroll(
            coordinatorLayout = mockk(),
            child = mockk(),
            directTargetChild = mockk(),
            target = mockk(),
            axes = ViewCompat.SCROLL_AXIS_VERTICAL,
            type = ViewCompat.TYPE_TOUCH
        )

        assertTrue(acceptsNestedScroll)

        verify { animator.cancel() }
    }

    @Test
    fun `Behavior should not accept nested scrolls on the horizontal axis`() {
        val behavior = DynamicDownloadDialogBehavior<View>(testContext, attrs = null)

        val acceptsNestedScroll = behavior.onStartNestedScroll(
            coordinatorLayout = mockk(),
            child = mockk(),
            directTargetChild = mockk(),
            target = mockk(),
            axes = ViewCompat.SCROLL_AXIS_HORIZONTAL,
            type = ViewCompat.TYPE_TOUCH
        )

        assertFalse(acceptsNestedScroll)
    }

    @Test
    fun `Behavior will snap the dialog up if it is more than 50 percent visible`() {
        val behavior = spyk(
            DynamicDownloadDialogBehavior<View>(
                testContext, attrs = null,
                bottomToolbarHeight = 10f
            )
        )
        every { behavior.shouldScroll } returns true

        val animator: ValueAnimator = mockk(relaxed = true)
        behavior.snapAnimator = animator

        behavior.expanded = false

        val child = mockk<View> {
            every { height } returns 100
            every { translationY } returns 59f
        }

        behavior.onStartNestedScroll(
            coordinatorLayout = mockk(),
            child = child,
            directTargetChild = mockk(),
            target = mockk(),
            axes = ViewCompat.SCROLL_AXIS_VERTICAL,
            type = ViewCompat.TYPE_TOUCH
        )

        assertTrue(behavior.shouldSnapAfterScroll)

        verify(exactly = 0) { animator.start() }

        behavior.onStopNestedScroll(
            coordinatorLayout = mockk(),
            child = child,
            target = mockk(),
            type = 0
        )

        verify { behavior.animateSnap(child, DynamicDownloadDialogBehavior.SnapDirection.UP) }

        verify { animator.start() }
    }

    @Test
    fun `Behavior will snap the dialog down if translationY is at least equal to half the toolbarHeight`() {
        val behavior = spyk(
            DynamicDownloadDialogBehavior<View>(
                testContext, attrs = null,
                bottomToolbarHeight = 10f
            )
        )
        every { behavior.shouldScroll } returns true

        val animator: ValueAnimator = mockk(relaxed = true)
        behavior.snapAnimator = animator

        behavior.expanded = true

        val child = mockk<View> {
            every { height } returns 100
            every { translationY } returns 5f
        }

        behavior.onStartNestedScroll(
            coordinatorLayout = mockk(),
            child = child,
            directTargetChild = mockk(),
            target = mockk(),
            axes = ViewCompat.SCROLL_AXIS_VERTICAL,
            type = ViewCompat.TYPE_TOUCH
        )

        assertTrue(behavior.shouldSnapAfterScroll)

        verify(exactly = 0) { animator.start() }

        behavior.onStopNestedScroll(
            coordinatorLayout = mockk(),
            child = child,
            target = mockk(),
            type = 0
        )

        verify { behavior.animateSnap(child, DynamicDownloadDialogBehavior.SnapDirection.DOWN) }

        verify { animator.start() }
    }

    @Test
    fun `Behavior will apply translation to the dialog for nested scroll`() {
        val behavior = spyk(DynamicDownloadDialogBehavior<View>(testContext, attrs = null))
        every { behavior.shouldScroll } returns true

        val child = mockk<View> {
            every { height } returns 100
            every { translationY } returns 0f
            every { translationY = any() } returns Unit
        }

        behavior.onNestedPreScroll(
            coordinatorLayout = mockk(),
            child = child,
            target = mockk(),
            dx = 0,
            dy = 25,
            consumed = IntArray(0),
            type = 0
        )

        verify { child.translationY = 25f }
    }

    @Test
    fun `Behavior will animateSnap UP when forceExpand is called`() {
        val behavior = spyk(DynamicDownloadDialogBehavior<View>(testContext, attrs = null))
        val dynamicDialogView: View = mockk(relaxed = true)
        every { behavior.shouldScroll } returns true

        behavior.forceExpand(dynamicDialogView)

        verify {
            behavior.animateSnap(
                dynamicDialogView,
                DynamicDownloadDialogBehavior.SnapDirection.UP
            )
        }
    }

    @Test
    fun `GIVEN a null InputResultDetail from the EngineView WHEN shouldScroll is called THEN it returns false`() {
        val behavior = DynamicDownloadDialogBehavior<View>(testContext, null, 10f)

        behavior.engineView = null
        assertFalse(behavior.shouldScroll)

        behavior.engineView = mockk()
        every { behavior.engineView?.getInputResultDetail() } returns null
        assertFalse(behavior.shouldScroll)
    }

    @Test
    fun `GIVEN an InputResultDetail with the right values WHEN shouldScroll is called THEN it returns true`() {
        val behavior = DynamicDownloadDialogBehavior<View>(testContext, null, 10f)
        val engineView: EngineView = mockk()
        behavior.engineView = engineView
        val validInputResultDetail: InputResultDetail = mockk()
        every { engineView.getInputResultDetail() } returns validInputResultDetail

        every { validInputResultDetail.canScrollToBottom() } returns true
        every { validInputResultDetail.canScrollToTop() } returns false
        assertTrue(behavior.shouldScroll)

        every { validInputResultDetail.canScrollToBottom() } returns false
        every { validInputResultDetail.canScrollToTop() } returns true
        assertTrue(behavior.shouldScroll)

        every { validInputResultDetail.canScrollToBottom() } returns true
        every { validInputResultDetail.canScrollToTop() } returns true
        assertTrue(behavior.shouldScroll)
    }

    @Test
    fun `GIVEN a gesture that doesn't scroll the toolbar WHEN startNestedScroll THEN toolbar is expanded and nested scroll not accepted`() {
        val behavior = spyk(DynamicDownloadDialogBehavior<View>(testContext, null, 10f))
        val engineView: EngineView = mockk()
        behavior.engineView = engineView
        val inputResultDetail: InputResultDetail = mockk()
        val animator: ValueAnimator = mockk(relaxed = true)
        behavior.snapAnimator = animator
        every { behavior.shouldScroll } returns false
        every { behavior.forceExpand(any()) } just Runs
        every { engineView.getInputResultDetail() } returns inputResultDetail
        every { inputResultDetail.isTouchUnhandled() } returns true

        val childView: View = mockk()
        val acceptsNestedScroll = behavior.onStartNestedScroll(
            coordinatorLayout = mockk(),
            child = childView,
            directTargetChild = mockk(),
            target = mockk(),
            axes = ViewCompat.SCROLL_AXIS_VERTICAL,
            type = ViewCompat.TYPE_TOUCH
        )

        verify { behavior.forceExpand(childView) }
        verify { animator.cancel() }
        assertFalse(acceptsNestedScroll)
    }
}
