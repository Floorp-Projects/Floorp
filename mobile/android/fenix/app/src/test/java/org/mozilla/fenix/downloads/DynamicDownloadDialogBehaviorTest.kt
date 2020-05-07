/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

package org.mozilla.fenix.downloads

import android.animation.ValueAnimator
import android.view.View
import androidx.core.view.ViewCompat
import mozilla.components.support.test.mock
import mozilla.components.support.test.robolectric.testContext
import org.junit.Assert.assertFalse
import org.junit.Assert.assertTrue
import org.junit.Test
import org.junit.runner.RunWith
import org.mockito.Mockito.doReturn
import org.mockito.Mockito.spy
import org.mockito.Mockito.verify
import org.mockito.Mockito.never
import org.mozilla.fenix.helpers.FenixRobolectricTestRunner

@RunWith(FenixRobolectricTestRunner::class)
class DynamicDownloadDialogBehaviorTest {

    @Test
    fun `Starting a nested scroll should cancel an ongoing snap animation`() {
        val behavior = spy(DynamicDownloadDialogBehavior<View>(testContext, attrs = null))
        doReturn(true).`when`(behavior).shouldScroll

        val animator: ValueAnimator = mock()
        behavior.snapAnimator = animator

        val acceptsNestedScroll = behavior.onStartNestedScroll(
            coordinatorLayout = mock(),
            child = mock(),
            directTargetChild = mock(),
            target = mock(),
            axes = ViewCompat.SCROLL_AXIS_VERTICAL,
            type = ViewCompat.TYPE_TOUCH
        )

        assertTrue(acceptsNestedScroll)

        verify(animator).cancel()
    }

    @Test
    fun `Behavior should not accept nested scrolls on the horizontal axis`() {
        val behavior = DynamicDownloadDialogBehavior<View>(testContext, attrs = null)

        val acceptsNestedScroll = behavior.onStartNestedScroll(
            coordinatorLayout = mock(),
            child = mock(),
            directTargetChild = mock(),
            target = mock(),
            axes = ViewCompat.SCROLL_AXIS_HORIZONTAL,
            type = ViewCompat.TYPE_TOUCH
        )

        assertFalse(acceptsNestedScroll)
    }

    @Test
    fun `Behavior will snap the dialog up if it is more than 50% visible`() {
        val behavior = spy(DynamicDownloadDialogBehavior<View>(testContext, attrs = null,
        bottomToolbarHeight = 10f))
        doReturn(true).`when`(behavior).shouldScroll

        val animator: ValueAnimator = mock()
        behavior.snapAnimator = animator

        behavior.expanded = false

        val child = mock<View>()
        doReturn(100).`when`(child)?.height
        doReturn(59f).`when`(child)?.translationY

        behavior.onStartNestedScroll(
            coordinatorLayout = mock(),
            child = child,
            directTargetChild = mock(),
            target = mock(),
            axes = ViewCompat.SCROLL_AXIS_VERTICAL,
            type = ViewCompat.TYPE_TOUCH
        )

        assertTrue(behavior.shouldSnapAfterScroll)

        verify(animator, never()).start()

        behavior.onStopNestedScroll(
            coordinatorLayout = mock(),
            child = child,
            target = mock(),
            type = 0
        )

        verify(behavior).animateSnap(child, DynamicDownloadDialogBehavior.SnapDirection.UP)

        verify(animator).start()
    }

    @Test
    fun `Behavior will snap the dialog down if translationY is at least equal to half the toolbarHeight`() {
        val behavior = spy(DynamicDownloadDialogBehavior<View>(testContext, attrs = null,
        bottomToolbarHeight = 10f))
        doReturn(true).`when`(behavior).shouldScroll

        val animator: ValueAnimator = mock()
        behavior.snapAnimator = animator

        behavior.expanded = true

        val child = mock<View>()
        doReturn(100).`when`(child).height
        doReturn(5f).`when`(child).translationY

        behavior.onStartNestedScroll(
            coordinatorLayout = mock(),
            child = child,
            directTargetChild = mock(),
            target = mock(),
            axes = ViewCompat.SCROLL_AXIS_VERTICAL,
            type = ViewCompat.TYPE_TOUCH
        )

        assertTrue(behavior.shouldSnapAfterScroll)

        verify(animator, never()).start()

        behavior.onStopNestedScroll(
            coordinatorLayout = mock(),
            child = child,
            target = mock(),
            type = 0
        )

        verify(behavior).animateSnap(child, DynamicDownloadDialogBehavior.SnapDirection.DOWN)

        verify(animator).start()
    }

    @Test
    fun `Behavior will apply translation to the dialog for nested scroll`() {
        val behavior = spy(DynamicDownloadDialogBehavior<View>(testContext, attrs = null))
        doReturn(true).`when`(behavior).shouldScroll

        val child = mock<View>()
        doReturn(100).`when`(child).height
        doReturn(0f).`when`(child).translationY

        behavior.onNestedPreScroll(
            coordinatorLayout = mock(),
            child = child,
            target = mock(),
            dx = 0,
            dy = 25,
            consumed = IntArray(0),
            type = 0
        )

        verify(child).translationY = 25f
    }

    @Test
    fun `Behavior will animateSnap UP when forceExpand is called`() {
        val behavior = spy(DynamicDownloadDialogBehavior<View>(testContext, attrs = null))
        val dynamicDialogView: View = mock()
        doReturn(true).`when`(behavior).shouldScroll

        behavior.forceExpand(dynamicDialogView)

        verify(behavior).animateSnap(
            dynamicDialogView,
            DynamicDownloadDialogBehavior.SnapDirection.UP
        )
    }
}
