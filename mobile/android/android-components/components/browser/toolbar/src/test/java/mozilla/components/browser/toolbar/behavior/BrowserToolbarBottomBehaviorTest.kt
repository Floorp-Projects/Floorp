/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

package mozilla.components.browser.toolbar.behavior

import android.animation.ValueAnimator
import android.view.Gravity
import androidx.coordinatorlayout.widget.CoordinatorLayout
import androidx.core.view.ViewCompat
import androidx.test.ext.junit.runners.AndroidJUnit4
import com.google.android.material.snackbar.Snackbar
import mozilla.components.browser.toolbar.BrowserToolbar
import mozilla.components.support.test.mock
import mozilla.components.support.test.robolectric.testContext
import org.junit.Assert.assertEquals
import org.junit.Assert.assertFalse
import org.junit.Assert.assertTrue
import org.junit.Test
import org.junit.runner.RunWith
import org.mockito.Mockito.doReturn
import org.mockito.Mockito.never
import org.mockito.Mockito.spy
import org.mockito.Mockito.verify

@RunWith(AndroidJUnit4::class)
class BrowserToolbarBottomBehaviorTest {

    @Test
    fun `Starting a nested scroll should cancel an ongoing snap animation`() {
        val behavior = BrowserToolbarBottomBehavior(testContext, attrs = null)

        val animator: ValueAnimator = mock()
        behavior.snapAnimator = animator

        val acceptsNestedScroll = behavior.onStartNestedScroll(
            coordinatorLayout = mock(),
            child = mock(),
            directTargetChild = mock(),
            target = mock(),
            axes = ViewCompat.SCROLL_AXIS_VERTICAL,
            type = ViewCompat.TYPE_TOUCH)

        assertTrue(acceptsNestedScroll)

        verify(animator).cancel()
    }

    @Test
    fun `Behavior should not accept nested scrolls on the horizontal axis`() {
        val behavior = BrowserToolbarBottomBehavior(testContext, attrs = null)

        val acceptsNestedScroll = behavior.onStartNestedScroll(
            coordinatorLayout = mock(),
            child = mock(),
            directTargetChild = mock(),
            target = mock(),
            axes = ViewCompat.SCROLL_AXIS_HORIZONTAL,
            type = ViewCompat.TYPE_TOUCH)

        assertFalse(acceptsNestedScroll)
    }

    @Test
    fun `Behavior will snap toolbar up if toolbar is more than 50% visible`() {
        val behavior = spy(BrowserToolbarBottomBehavior(testContext, attrs = null))

        val animator: ValueAnimator = mock()
        behavior.snapAnimator = animator

        val child = mock<BrowserToolbar>()
        doReturn(100).`when`(child).height
        doReturn(10f).`when`(child).translationY

        behavior.onStartNestedScroll(
            coordinatorLayout = mock(),
            child = child,
            directTargetChild = mock(),
            target = mock(),
            axes = ViewCompat.SCROLL_AXIS_VERTICAL,
            type = ViewCompat.TYPE_TOUCH)

        assertTrue(behavior.shouldSnapAfterScroll)

        verify(animator, never()).start()

        behavior.onStopNestedScroll(
            coordinatorLayout = mock(),
            child = child,
            target = mock(),
            type = 0)

        verify(behavior).animateSnap(child, SnapDirection.UP)

        verify(animator).start()
    }

    @Test
    fun `Behavior will snap toolbar down if toolbar is less than 50% visible`() {
        val behavior = spy(BrowserToolbarBottomBehavior(testContext, attrs = null))

        val animator: ValueAnimator = mock()
        behavior.snapAnimator = animator

        val child = mock<BrowserToolbar>()
        doReturn(100).`when`(child).height
        doReturn(90f).`when`(child).translationY

        behavior.onStartNestedScroll(
            coordinatorLayout = mock(),
            child = child,
            directTargetChild = mock(),
            target = mock(),
            axes = ViewCompat.SCROLL_AXIS_VERTICAL,
            type = ViewCompat.TYPE_TOUCH)

        assertTrue(behavior.shouldSnapAfterScroll)

        verify(animator, never()).start()

        behavior.onStopNestedScroll(
            coordinatorLayout = mock(),
            child = child,
            target = mock(),
            type = 0)

        verify(behavior).animateSnap(child, SnapDirection.DOWN)

        verify(animator).start()
    }

    @Test
    fun `Behavior will apply translation to toolbar for nested scroll`() {
        val behavior = spy(BrowserToolbarBottomBehavior(testContext, attrs = null))

        val child = mock<BrowserToolbar>()
        doReturn(100).`when`(child).height
        doReturn(0f).`when`(child).translationY

        behavior.onNestedPreScroll(
            coordinatorLayout = mock(),
            child = child,
            target = mock(),
            dx = 0,
            dy = 25,
            consumed = IntArray(0),
            type = 0)

        verify(child).translationY = 25f
    }

    @Test
    fun `Behavior will position snackbar above toolbar`() {
        val behavior = BrowserToolbarBottomBehavior(testContext, attrs = null)

        val toolbar: BrowserToolbar = mock()
        doReturn(4223).`when`(toolbar).id

        val layoutParams: CoordinatorLayout.LayoutParams = CoordinatorLayout.LayoutParams(0, 0)

        val snackbarLayout = Snackbar.SnackbarLayout(testContext)
        snackbarLayout.layoutParams = layoutParams

        behavior.layoutDependsOn(
            parent = mock(),
            child = toolbar,
            dependency = snackbarLayout
        )

        assertEquals(4223, layoutParams.anchorId)
        assertEquals(Gravity.TOP or Gravity.CENTER_HORIZONTAL, layoutParams.anchorGravity)
        assertEquals(Gravity.TOP or Gravity.CENTER_HORIZONTAL, layoutParams.gravity)
    }
}