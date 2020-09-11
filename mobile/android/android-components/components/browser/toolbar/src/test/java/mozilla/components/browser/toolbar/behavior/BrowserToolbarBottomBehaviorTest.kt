/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

package mozilla.components.browser.toolbar.behavior

import android.animation.ValueAnimator
import android.content.Context
import android.graphics.Bitmap
import android.view.Gravity
import android.view.MotionEvent.ACTION_DOWN
import android.view.MotionEvent.ACTION_MOVE
import android.widget.FrameLayout
import androidx.coordinatorlayout.widget.CoordinatorLayout
import androidx.core.view.ViewCompat
import androidx.test.ext.junit.runners.AndroidJUnit4
import com.google.android.material.snackbar.Snackbar
import mozilla.components.browser.toolbar.BrowserToolbar
import mozilla.components.concept.engine.EngineSession
import mozilla.components.concept.engine.EngineView
import mozilla.components.concept.engine.selection.SelectionActionDelegate
import mozilla.components.support.test.mock
import mozilla.components.support.test.robolectric.testContext
import org.junit.Assert.assertEquals
import org.junit.Assert.assertFalse
import org.junit.Assert.assertTrue
import org.junit.Test
import org.junit.runner.RunWith
import org.mockito.ArgumentMatchers.anyFloat
import org.mockito.Mockito.`when`
import org.mockito.Mockito.doReturn
import org.mockito.Mockito.never
import org.mockito.Mockito.spy
import org.mockito.Mockito.verify

@RunWith(AndroidJUnit4::class)
class BrowserToolbarBottomBehaviorTest {

    @Test
    fun `Starting a nested scroll should cancel an ongoing snap animation`() {
        val behavior = spy(BrowserToolbarBottomBehavior(testContext, attrs = null))
        doReturn(true).`when`(behavior).shouldScroll

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
        doReturn(true).`when`(behavior).shouldScroll

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
        doReturn(true).`when`(behavior).shouldScroll

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
    fun `Behavior will intercept MotionEvents and pass them to the custom gesture detector`() {
        val behavior = spy(BrowserToolbarBottomBehavior(testContext, attrs = null))
        val gestureDetector: BrowserGestureDetector = mock()
        behavior.initGesturesDetector(gestureDetector)
        val downEvent = TestUtils.getMotionEvent(ACTION_DOWN)

        behavior.onInterceptTouchEvent(mock(), mock(), downEvent)

        verify(gestureDetector).handleTouchEvent(downEvent)
    }

    @Test
    fun `Behavior will apply translation to toolbar only for vertical scrolls`() {
        val behavior = spy(BrowserToolbarBottomBehavior(testContext, attrs = null))
        behavior.initGesturesDetector(behavior.createGestureDetector())
        val child = spy(BrowserToolbar(testContext, null, 0))
        behavior.browserToolbar = child
        val downEvent = TestUtils.getMotionEvent(ACTION_DOWN, 0f, 0f)
        val moveEvent = TestUtils.getMotionEvent(ACTION_MOVE, 0f, 100f, downEvent)

        behavior.onInterceptTouchEvent(mock(), mock(), downEvent)
        behavior.onInterceptTouchEvent(mock(), mock(), moveEvent)

        verify(behavior).tryToScrollVertically(-100f)
    }

    @Test
    fun `Behaviour shouldScroll if EngineView handled the MotionEvent`() {
        val behavior = BrowserToolbarBottomBehavior(testContext, attrs = null)
        val engineView: EngineView = mock()
        `when`(engineView.getInputResult()).thenReturn(EngineView.InputResult.INPUT_RESULT_HANDLED)
        behavior.engineView = engineView

        assertTrue(behavior.shouldScroll)
    }

    @Test
    fun `Behaviour !shouldScroll if EngineView didn't handle the MotionEvent`() {
        val behavior = BrowserToolbarBottomBehavior(testContext, attrs = null)
        val engineView: EngineView = mock()
        `when`(engineView.getInputResult()).thenReturn(EngineView.InputResult.INPUT_RESULT_UNHANDLED)
        behavior.engineView = engineView

        assertFalse(behavior.shouldScroll)
    }

    @Test
    fun `Behaviour !shouldScroll if EngineView didn't handle the MotionEvent but the website`() {
        val behavior = BrowserToolbarBottomBehavior(testContext, attrs = null)
        val engineView: EngineView = mock()
        `when`(engineView.getInputResult()).thenReturn(EngineView.InputResult.INPUT_RESULT_HANDLED_CONTENT)
        behavior.engineView = engineView

        assertFalse(behavior.shouldScroll)
    }

    @Test
    fun `Behavior will vertically scroll for such and event and if EngineView handled the event`() {
        val behavior = spy(BrowserToolbarBottomBehavior(testContext, attrs = null))
        doReturn(true).`when`(behavior).shouldScroll
        val child = spy(BrowserToolbar(testContext, null, 0))
        behavior.browserToolbar = child
        doReturn(100).`when`(child).height
        doReturn(0f).`when`(child).translationY
        behavior.startedScroll = true

        behavior.tryToScrollVertically(25f)

        verify(child).translationY = 25f
    }

    @Test
    fun `Behavior will not scroll vertically if startedScroll is false`() {
        val behavior = spy(BrowserToolbarBottomBehavior(testContext, attrs = null))
        doReturn(true).`when`(behavior).shouldScroll
        val child = spy(BrowserToolbar(testContext, null, 0))
        behavior.browserToolbar = child
        doReturn(100).`when`(child).height
        doReturn(0f).`when`(child).translationY
        behavior.startedScroll = false

        behavior.tryToScrollVertically(25f)

        verify(child, never()).translationY
    }

    @Test
    fun `Behavior will not scroll vertically if EngineView did not handled the event`() {
        val behavior = spy(BrowserToolbarBottomBehavior(testContext, attrs = null))
        doReturn(false).`when`(behavior).shouldScroll
        val child = spy(BrowserToolbar(testContext, null, 0))
        behavior.browserToolbar = child
        doReturn(100).`when`(child).height
        doReturn(0f).`when`(child).translationY

        behavior.tryToScrollVertically(25f)

        verify(child, never()).setTranslationY(anyFloat())
    }

    @Test
    fun `Behavior will snap toolbar first finishing translation animations if they are in progress`() {
        val behavior = BrowserToolbarBottomBehavior(testContext, attrs = null)
        val animator: ValueAnimator = mock()
        behavior.snapAnimator = animator
        doReturn(true).`when`(animator).isStarted
        val child = mock<BrowserToolbar>()
        behavior.browserToolbar = child
        doReturn(100).`when`(child).height
        doReturn(40f).`when`(child).translationY

        behavior.snapToolbarVertically()

        verify(animator).end()
    }

    @Test
    fun `Behavior can snap toolbar if it is translated to the bottom half`() {
        val behavior = BrowserToolbarBottomBehavior(testContext, attrs = null)
        val animator: ValueAnimator = mock()
        behavior.snapAnimator = animator
        doReturn(false).`when`(animator).isStarted
        val child = mock<BrowserToolbar>()
        behavior.browserToolbar = child
        doReturn(100).`when`(child).height
        doReturn(40f).`when`(child).translationY

        behavior.snapToolbarVertically()

        verify(child).translationY = 0f
    }

    @Test
    fun `Behavior can snap toolbar if it is translated to the top half`() {
        val behavior = BrowserToolbarBottomBehavior(testContext, attrs = null)
        val animator: ValueAnimator = mock()
        behavior.snapAnimator = animator
        doReturn(false).`when`(animator).isStarted
        val child = mock<BrowserToolbar>()
        behavior.browserToolbar = child
        doReturn(100).`when`(child).height
        doReturn(60f).`when`(child).translationY

        behavior.snapToolbarVertically()

        verify(child).translationY = 100f
    }

    @Test
    fun `Behavior will snap toolbar to top if it is translated to exactly half`() {
        val behavior = BrowserToolbarBottomBehavior(testContext, attrs = null)
        val animator: ValueAnimator = mock()
        behavior.snapAnimator = animator
        doReturn(false).`when`(animator).isStarted
        val child = mock<BrowserToolbar>()
        doReturn(100).`when`(child).height
        doReturn(50f).`when`(child).translationY
        behavior.browserToolbar = child

        behavior.snapToolbarVertically()

        verify(child).translationY = 100f
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

    @Test
    fun `Behavior will animateSnap UP when forceExpand is called`() {
        val behavior = spy(BrowserToolbarBottomBehavior(testContext, attrs = null))
        doReturn(true).`when`(behavior).shouldScroll
        val toolbar: BrowserToolbar = mock()

        behavior.forceExpand(toolbar)

        verify(behavior).animateSnap(toolbar, SnapDirection.UP)
    }

    @Test
    fun `Behavior will forceExpand when scrolling up and !shouldScroll`() {
        val behavior = spy(BrowserToolbarBottomBehavior(testContext, attrs = null))
        behavior.initGesturesDetector(behavior.createGestureDetector())
        doReturn(false).`when`(behavior).shouldScroll
        val toolbar: BrowserToolbar = spy(BrowserToolbar(testContext, null, 0))
        behavior.browserToolbar = toolbar
        val animator: ValueAnimator = mock()
        behavior.snapAnimator = animator
        doReturn(false).`when`(animator).isStarted

        doReturn(100).`when`(toolbar).height
        doReturn(100f).`when`(toolbar).translationY

        val downEvent = TestUtils.getMotionEvent(ACTION_DOWN, 0f, 0f)
        val moveEvent = TestUtils.getMotionEvent(ACTION_MOVE, 0f, 30f, downEvent)

        behavior.onInterceptTouchEvent(mock(), mock(), downEvent)
        behavior.onInterceptTouchEvent(mock(), mock(), moveEvent)

        verify(behavior).tryToScrollVertically(-30f)
        verify(behavior).forceExpand(toolbar)
    }

    @Test
    fun `onLayoutChild initializes browserToolbar and engineView`() {
        val toolbarView = BrowserToolbar(testContext)
        val engineView = createDummyEngineView(testContext).asView()
        val container = CoordinatorLayout(testContext).apply {
            addView(BrowserToolbar(testContext))
            addView(engineView)
        }
        val behavior = spy(BrowserToolbarBottomBehavior(testContext, attrs = null))

        behavior.onLayoutChild(container, toolbarView, ViewCompat.LAYOUT_DIRECTION_LTR)

        assertEquals(toolbarView, behavior.browserToolbar)
        assertEquals(engineView, behavior.engineView)
    }

    private fun createDummyEngineView(context: Context): EngineView = DummyEngineView(context)

    open class DummyEngineView(context: Context) : FrameLayout(context), EngineView {
        override fun setVerticalClipping(clippingHeight: Int) {}
        override fun setDynamicToolbarMaxHeight(height: Int) {}
        override fun captureThumbnail(onFinish: (Bitmap?) -> Unit) = Unit
        override fun render(session: EngineSession) {}
        override fun release() {}
        override var selectionActionDelegate: SelectionActionDelegate? = null
    }
}
