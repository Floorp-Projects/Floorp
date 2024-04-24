/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

package mozilla.components.ui.widgets.behavior

import android.content.Context
import android.graphics.Bitmap
import android.view.MotionEvent.ACTION_DOWN
import android.view.MotionEvent.ACTION_MOVE
import android.view.View
import android.widget.FrameLayout
import androidx.coordinatorlayout.widget.CoordinatorLayout
import androidx.core.view.ViewCompat
import androidx.test.ext.junit.runners.AndroidJUnit4
import mozilla.components.concept.engine.EngineSession
import mozilla.components.concept.engine.EngineView
import mozilla.components.concept.engine.INPUT_UNHANDLED
import mozilla.components.concept.engine.InputResultDetail
import mozilla.components.concept.engine.selection.SelectionActionDelegate
import mozilla.components.support.test.any
import mozilla.components.support.test.mock
import mozilla.components.support.test.robolectric.testContext
import org.junit.Assert.assertEquals
import org.junit.Assert.assertFalse
import org.junit.Assert.assertTrue
import org.junit.Test
import org.junit.runner.RunWith
import org.mockito.ArgumentMatchers.anyFloat
import org.mockito.ArgumentMatchers.anyInt
import org.mockito.Mockito.doReturn
import org.mockito.Mockito.never
import org.mockito.Mockito.spy
import org.mockito.Mockito.verify
import org.mockito.Mockito.`when`

@RunWith(AndroidJUnit4::class)
class EngineViewScrollingBehaviorTest {
    @Test
    fun `onStartNestedScroll should attempt scrolling only if browserToolbar is valid`() {
        val behavior = spy(EngineViewScrollingBehavior(testContext, null, ViewPosition.BOTTOM))
        doReturn(true).`when`(behavior).shouldScroll

        behavior.dynamicScrollView = null
        var acceptsNestedScroll = behavior.onStartNestedScroll(
            coordinatorLayout = mock(),
            child = mock(),
            directTargetChild = mock(),
            target = mock(),
            axes = ViewCompat.SCROLL_AXIS_VERTICAL,
            type = ViewCompat.TYPE_TOUCH,
        )
        assertFalse(acceptsNestedScroll)
        verify(behavior, never()).startNestedScroll(anyInt(), anyInt(), any())

        behavior.dynamicScrollView = mock()
        acceptsNestedScroll = behavior.onStartNestedScroll(
            coordinatorLayout = mock(),
            child = mock(),
            directTargetChild = mock(),
            target = mock(),
            axes = ViewCompat.SCROLL_AXIS_VERTICAL,
            type = ViewCompat.TYPE_TOUCH,
        )
        assertTrue(acceptsNestedScroll)
        verify(behavior).startNestedScroll(anyInt(), anyInt(), any())
    }

    @Test
    fun `startNestedScroll should cancel an ongoing snap animation`() {
        val behavior = spy(EngineViewScrollingBehavior(testContext, null, ViewPosition.BOTTOM))
        val yTranslator: ViewYTranslator = mock()
        behavior.yTranslator = yTranslator
        doReturn(true).`when`(behavior).shouldScroll

        val acceptsNestedScroll = behavior.startNestedScroll(
            axes = ViewCompat.SCROLL_AXIS_VERTICAL,
            type = ViewCompat.TYPE_TOUCH,
            view = mock(),
        )

        assertTrue(acceptsNestedScroll)
        verify(yTranslator).cancelInProgressTranslation()
    }

    @Test
    fun `startNestedScroll should not accept nested scrolls on the horizontal axis`() {
        val behavior = spy(EngineViewScrollingBehavior(testContext, null, ViewPosition.BOTTOM))
        doReturn(true).`when`(behavior).shouldScroll

        var acceptsNestedScroll = behavior.startNestedScroll(
            axes = ViewCompat.SCROLL_AXIS_VERTICAL,
            type = ViewCompat.TYPE_TOUCH,
            view = mock(),
        )
        assertTrue(acceptsNestedScroll)

        acceptsNestedScroll = behavior.startNestedScroll(
            axes = ViewCompat.SCROLL_AXIS_HORIZONTAL,
            type = ViewCompat.TYPE_TOUCH,
            view = mock(),
        )
        assertFalse(acceptsNestedScroll)
    }

    @Test
    fun `GIVEN a gesture that doesn't scroll the toolbar WHEN startNestedScroll THEN toolbar is expanded and nested scroll not accepted`() {
        val behavior = spy(EngineViewScrollingBehavior(testContext, null, ViewPosition.BOTTOM))
        val engineView: EngineView = mock()
        val inputResultDetail: InputResultDetail = mock()
        val yTranslator: ViewYTranslator = mock()
        behavior.yTranslator = yTranslator
        doReturn(false).`when`(behavior).shouldScroll
        doReturn(true).`when`(inputResultDetail).isTouchUnhandled()
        behavior.engineView = engineView
        doReturn(inputResultDetail).`when`(engineView).getInputResultDetail()

        val acceptsNestedScroll = behavior.startNestedScroll(
            axes = ViewCompat.SCROLL_AXIS_VERTICAL,
            type = ViewCompat.TYPE_TOUCH,
            view = mock(),
        )

        verify(yTranslator).cancelInProgressTranslation()
        verify(yTranslator).expandWithAnimation(any())
        assertFalse(acceptsNestedScroll)
    }

    @Test
    fun `Behavior should not accept nested scrolls on the horizontal axis`() {
        val behavior = spy(EngineViewScrollingBehavior(testContext, null, ViewPosition.BOTTOM))
        behavior.dynamicScrollView = mock()
        doReturn(true).`when`(behavior).shouldScroll

        var acceptsNestedScroll = behavior.onStartNestedScroll(
            coordinatorLayout = mock(),
            child = mock(),
            directTargetChild = mock(),
            target = mock(),
            axes = ViewCompat.SCROLL_AXIS_VERTICAL,
            type = ViewCompat.TYPE_TOUCH,
        )
        assertTrue(acceptsNestedScroll)

        acceptsNestedScroll = behavior.onStartNestedScroll(
            coordinatorLayout = mock(),
            child = mock(),
            directTargetChild = mock(),
            target = mock(),
            axes = ViewCompat.SCROLL_AXIS_HORIZONTAL,
            type = ViewCompat.TYPE_TOUCH,
        )
        assertFalse(acceptsNestedScroll)
    }

    @Test
    fun `Behavior should delegate the onStartNestedScroll logic`() {
        val behavior = spy(EngineViewScrollingBehavior(testContext, null, ViewPosition.BOTTOM))
        val view: View = mock()
        behavior.dynamicScrollView = view
        val inputType = ViewCompat.TYPE_TOUCH
        val axes = ViewCompat.SCROLL_AXIS_VERTICAL

        behavior.onStartNestedScroll(
            coordinatorLayout = mock(),
            child = view,
            directTargetChild = mock(),
            target = mock(),
            axes = axes,
            type = inputType,
        )

        verify(behavior).startNestedScroll(axes, inputType, view)
    }

    @Test
    fun `onStopNestedScroll should attempt stopping nested scrolling only if browserToolbar is valid`() {
        val behavior = spy(EngineViewScrollingBehavior(testContext, null, ViewPosition.BOTTOM))

        behavior.dynamicScrollView = null
        behavior.onStopNestedScroll(
            coordinatorLayout = mock(),
            child = mock(),
            target = mock(),
            type = 0,
        )
        verify(behavior, never()).stopNestedScroll(anyInt(), any())

        behavior.dynamicScrollView = mock()
        behavior.onStopNestedScroll(
            coordinatorLayout = mock(),
            child = mock(),
            target = mock(),
            type = 0,
        )
        verify(behavior).stopNestedScroll(anyInt(), any())
    }

    @Test
    fun `Behavior should delegate the onStopNestedScroll logic`() {
        val behavior = spy(EngineViewScrollingBehavior(testContext, null, ViewPosition.BOTTOM))
        val inputType = ViewCompat.TYPE_TOUCH
        val view: View = mock()

        behavior.dynamicScrollView = null
        behavior.onStopNestedScroll(
            coordinatorLayout = mock(),
            child = view,
            target = mock(),
            type = inputType,
        )
        verify(behavior, never()).stopNestedScroll(inputType, view)
    }

    @Test
    fun `stopNestedScroll will snap toolbar up if toolbar is more than 50 percent visible`() {
        val behavior = spy(EngineViewScrollingBehavior(testContext, null, ViewPosition.BOTTOM))
        val yTranslator: ViewYTranslator = mock()
        behavior.yTranslator = yTranslator
        behavior.dynamicScrollView = mock()
        doReturn(true).`when`(behavior).shouldScroll

        val child = mock<View>()
        doReturn(100).`when`(child).height
        doReturn(10f).`when`(child).translationY

        behavior.onStartNestedScroll(
            coordinatorLayout = mock(),
            child = child,
            directTargetChild = mock(),
            target = mock(),
            axes = ViewCompat.SCROLL_AXIS_VERTICAL,
            type = ViewCompat.TYPE_TOUCH,
        )

        assertTrue(behavior.shouldSnapAfterScroll)
        verify(yTranslator).cancelInProgressTranslation()
        verify(yTranslator, never()).expandWithAnimation(any())
        verify(yTranslator, never()).collapseWithAnimation(any())

        behavior.stopNestedScroll(0, child)

        verify(yTranslator).snapWithAnimation(child)
    }

    @Test
    fun `stopNestedScroll will snap toolbar down if toolbar is less than 50 percent visible`() {
        val behavior = spy(EngineViewScrollingBehavior(testContext, null, ViewPosition.BOTTOM))
        doReturn(true).`when`(behavior).shouldScroll
        val yTranslator: ViewYTranslator = mock()
        behavior.yTranslator = yTranslator

        val child = mock<View>()
        behavior.dynamicScrollView = child
        doReturn(100).`when`(child).height
        doReturn(90f).`when`(child).translationY

        behavior.onStartNestedScroll(
            coordinatorLayout = mock(),
            child = child,
            directTargetChild = mock(),
            target = mock(),
            axes = ViewCompat.SCROLL_AXIS_VERTICAL,
            type = ViewCompat.TYPE_TOUCH,
        )

        assertTrue(behavior.shouldSnapAfterScroll)
        verify(yTranslator).cancelInProgressTranslation()
        verify(yTranslator, never()).expandWithAnimation(any())
        verify(yTranslator, never()).collapseWithAnimation(any())

        behavior.stopNestedScroll(0, child)

        verify(yTranslator).snapWithAnimation(child)
    }

    @Test
    fun `onStopNestedScroll should snap the toolbar only if browserToolbar is valid`() {
        val behavior = spy(EngineViewScrollingBehavior(testContext, null, ViewPosition.BOTTOM))
        behavior.dynamicScrollView = null

        behavior.onStopNestedScroll(
            coordinatorLayout = mock(),
            child = mock(),
            target = mock(),
            type = ViewCompat.TYPE_TOUCH,
        )

        verify(behavior, never()).stopNestedScroll(anyInt(), any())
    }

    @Test
    fun `Behavior will intercept MotionEvents and pass them to the custom gesture detector`() {
        val behavior = spy(EngineViewScrollingBehavior(testContext, null, ViewPosition.BOTTOM))
        val gestureDetector: BrowserGestureDetector = mock()
        behavior.initGesturesDetector(gestureDetector)
        behavior.dynamicScrollView = mock()
        val downEvent = TestUtils.getMotionEvent(ACTION_DOWN)

        behavior.onInterceptTouchEvent(mock(), mock(), downEvent)

        verify(gestureDetector).handleTouchEvent(downEvent)
    }

    @Test
    fun `Behavior should only dispatch MotionEvents to the gesture detector only if browserToolbar is valid`() {
        val behavior = spy(EngineViewScrollingBehavior(testContext, null, ViewPosition.BOTTOM))
        val gestureDetector: BrowserGestureDetector = mock()
        behavior.initGesturesDetector(gestureDetector)
        val downEvent = TestUtils.getMotionEvent(ACTION_DOWN)

        behavior.onInterceptTouchEvent(mock(), mock(), downEvent)

        verify(gestureDetector, never()).handleTouchEvent(downEvent)
    }

    @Test
    fun `Behavior will apply translation to toolbar only for vertical scrolls`() {
        val behavior = spy(EngineViewScrollingBehavior(testContext, null, ViewPosition.BOTTOM))
        behavior.initGesturesDetector(behavior.createGestureDetector())
        val child = spy(View(testContext, null, 0))
        behavior.dynamicScrollView = child
        val downEvent = TestUtils.getMotionEvent(ACTION_DOWN, 0f, 0f)
        val moveEvent = TestUtils.getMotionEvent(ACTION_MOVE, 0f, 100f, downEvent)

        behavior.onInterceptTouchEvent(mock(), mock(), downEvent)
        behavior.onInterceptTouchEvent(mock(), mock(), moveEvent)

        verify(behavior).tryToScrollVertically(-100f)
    }

    @Test
    fun `GIVEN a null InputResultDetail from the EngineView WHEN shouldScroll is called THEN it returns false`() {
        val behavior = EngineViewScrollingBehavior(testContext, null, ViewPosition.BOTTOM)
        behavior.engineView = null
        assertFalse(behavior.shouldScroll)
        behavior.engineView = mock()
        `when`(behavior.engineView!!.getInputResultDetail()).thenReturn(null)

        assertFalse(behavior.shouldScroll)
    }

    @Test
    fun `GIVEN an InputResultDetail with the right values and scroll enabled WHEN shouldScroll is called THEN it returns true`() {
        val behavior = EngineViewScrollingBehavior(testContext, null, ViewPosition.BOTTOM)
        val engineView: EngineView = mock()
        behavior.engineView = engineView
        behavior.isScrollEnabled = true
        val validInputResultDetail: InputResultDetail = mock()
        doReturn(validInputResultDetail).`when`(engineView).getInputResultDetail()

        doReturn(true).`when`(validInputResultDetail).canScrollToBottom()
        doReturn(false).`when`(validInputResultDetail).canScrollToTop()
        assertTrue(behavior.shouldScroll)

        doReturn(false).`when`(validInputResultDetail).canScrollToBottom()
        doReturn(true).`when`(validInputResultDetail).canScrollToTop()
        assertTrue(behavior.shouldScroll)

        doReturn(true).`when`(validInputResultDetail).canScrollToBottom()
        doReturn(true).`when`(validInputResultDetail).canScrollToTop()
        assertTrue(behavior.shouldScroll)
    }

    @Test
    fun `GIVEN an InputResultDetail with the right values but with scroll disabled WHEN shouldScroll is called THEN it returns false`() {
        val behavior = EngineViewScrollingBehavior(testContext, null, ViewPosition.BOTTOM)
        behavior.engineView = mock()
        behavior.isScrollEnabled = false
        val validInputResultDetail: InputResultDetail = mock()
        doReturn(true).`when`(validInputResultDetail).canScrollToBottom()
        doReturn(true).`when`(validInputResultDetail).canScrollToTop()

        assertFalse(behavior.shouldScroll)
    }

    @Test
    fun `GIVEN scroll enabled but EngineView cannot scroll to bottom WHEN shouldScroll is called THEN it returns false`() {
        val behavior = EngineViewScrollingBehavior(testContext, null, ViewPosition.BOTTOM)
        behavior.engineView = mock()
        behavior.isScrollEnabled = true
        val validInputResultDetail: InputResultDetail = mock()
        doReturn(false).`when`(validInputResultDetail).canScrollToBottom()
        doReturn(true).`when`(validInputResultDetail).canScrollToTop()

        assertFalse(behavior.shouldScroll)
    }

    @Test
    fun `GIVEN scroll enabled but EngineView cannot scroll to top WHEN shouldScroll is called THEN it returns false`() {
        val behavior = EngineViewScrollingBehavior(testContext, null, ViewPosition.BOTTOM)
        behavior.engineView = mock()
        behavior.isScrollEnabled = true
        val validInputResultDetail: InputResultDetail = mock()
        doReturn(true).`when`(validInputResultDetail).canScrollToBottom()
        doReturn(false).`when`(validInputResultDetail).canScrollToTop()

        assertFalse(behavior.shouldScroll)
    }

    @Test
    fun `Behavior will vertically scroll nested scroll started and EngineView handled the event`() {
        val behavior = spy(EngineViewScrollingBehavior(testContext, null, ViewPosition.BOTTOM))
        val yTranslator: ViewYTranslator = mock()
        behavior.yTranslator = yTranslator
        doReturn(true).`when`(behavior).shouldScroll
        val child = spy(View(testContext, null, 0))
        behavior.dynamicScrollView = child
        doReturn(100).`when`(child).height
        doReturn(0f).`when`(child).translationY
        behavior.startedScroll = true

        behavior.tryToScrollVertically(25f)

        verify(yTranslator).translate(child, 25f)
    }

    @Test
    fun `Behavior will not scroll vertically if startedScroll is false`() {
        val behavior = spy(EngineViewScrollingBehavior(testContext, null, ViewPosition.BOTTOM))
        val yTranslator: ViewYTranslator = mock()
        behavior.yTranslator = yTranslator
        doReturn(true).`when`(behavior).shouldScroll
        val child = spy(View(testContext, null, 0))
        behavior.dynamicScrollView = child
        doReturn(100).`when`(child).height
        doReturn(0f).`when`(child).translationY
        behavior.startedScroll = false

        behavior.tryToScrollVertically(25f)

        verify(yTranslator, never()).translate(any(), anyFloat())
    }

    @Test
    fun `Behavior will not scroll vertically if EngineView did not handled the event`() {
        val behavior = spy(EngineViewScrollingBehavior(testContext, null, ViewPosition.BOTTOM))
        val yTranslator: ViewYTranslator = mock()
        behavior.yTranslator = yTranslator
        doReturn(false).`when`(behavior).shouldScroll
        val child = spy(View(testContext, null, 0))
        behavior.dynamicScrollView = child
        doReturn(100).`when`(child).height
        doReturn(0f).`when`(child).translationY

        behavior.tryToScrollVertically(25f)

        verify(yTranslator, never()).translate(any(), anyFloat())
    }

    @Test
    fun `forceExpand should delegate the translator`() {
        val behavior = spy(EngineViewScrollingBehavior(testContext, null, ViewPosition.BOTTOM))
        val yTranslator: ViewYTranslator = mock()
        behavior.yTranslator = yTranslator
        val view: View = mock()

        behavior.forceExpand(view)

        verify(yTranslator).expandWithAnimation(view)
    }

    @Test
    fun `forceCollapse should delegate the translator`() {
        val behavior = spy(EngineViewScrollingBehavior(testContext, null, ViewPosition.BOTTOM))
        val yTranslator: ViewYTranslator = mock()
        behavior.yTranslator = yTranslator
        val view: View = mock()

        behavior.forceCollapse(view)

        verify(yTranslator).collapseWithAnimation(view)
    }

    @Test
    fun `Behavior will forceExpand when scrolling up and !shouldScroll if the touch was handled in the browser`() {
        val behavior = spy(EngineViewScrollingBehavior(testContext, null, ViewPosition.BOTTOM))
        val yTranslator: ViewYTranslator = mock()
        behavior.yTranslator = yTranslator
        behavior.initGesturesDetector(behavior.createGestureDetector())
        val view: View = spy(View(testContext, null, 0))
        behavior.dynamicScrollView = view
        val engineView: EngineView = mock()
        behavior.engineView = engineView
        val handledTouchInput = InputResultDetail.newInstance().copy(INPUT_UNHANDLED)
        doReturn(handledTouchInput).`when`(engineView).getInputResultDetail()

        doReturn(100).`when`(view).height
        doReturn(100f).`when`(view).translationY

        val downEvent = TestUtils.getMotionEvent(ACTION_DOWN, 0f, 0f)
        val moveEvent = TestUtils.getMotionEvent(ACTION_MOVE, 0f, 30f, downEvent)

        behavior.onInterceptTouchEvent(mock(), mock(), downEvent)
        behavior.onInterceptTouchEvent(mock(), mock(), moveEvent)

        verify(behavior).tryToScrollVertically(-30f)
        verify(yTranslator).forceExpandIfNotAlready(view, -30f)
    }

    @Test
    fun `Behavior will not forceExpand when scrolling up and !shouldScroll if the touch was not yet handled in the browser`() {
        val behavior = spy(EngineViewScrollingBehavior(testContext, null, ViewPosition.BOTTOM))
        val yTranslator: ViewYTranslator = mock()
        behavior.yTranslator = yTranslator
        behavior.initGesturesDetector(behavior.createGestureDetector())
        val view: View = spy(View(testContext, null, 0))
        behavior.dynamicScrollView = view
        val engineView: EngineView = mock()
        behavior.engineView = engineView
        val handledTouchInput = InputResultDetail.newInstance()
        doReturn(handledTouchInput).`when`(engineView).getInputResultDetail()

        doReturn(100).`when`(view).height
        doReturn(100f).`when`(view).translationY

        val downEvent = TestUtils.getMotionEvent(ACTION_DOWN, 0f, 0f)
        val moveEvent = TestUtils.getMotionEvent(ACTION_MOVE, 0f, 30f, downEvent)

        behavior.onInterceptTouchEvent(mock(), mock(), downEvent)
        behavior.onInterceptTouchEvent(mock(), mock(), moveEvent)

        verify(behavior).tryToScrollVertically(-30f)
        verify(yTranslator, never()).forceExpandIfNotAlready(view, -30f)
    }

    @Test
    fun `onLayoutChild initializes browserToolbar and engineView`() {
        val view = View(testContext)
        val engineView = createDummyEngineView(testContext).asView()
        val container = CoordinatorLayout(testContext).apply {
            addView(View(testContext))
            addView(engineView)
        }
        val behavior = spy(EngineViewScrollingBehavior(testContext, null, ViewPosition.BOTTOM))

        behavior.onLayoutChild(container, view, View.LAYOUT_DIRECTION_LTR)

        assertEquals(view, behavior.dynamicScrollView)
        assertEquals(engineView, behavior.engineView)
    }

    @Test
    fun `enableScrolling sets isScrollEnabled to true`() {
        val behavior = EngineViewScrollingBehavior(testContext, null, ViewPosition.BOTTOM)

        assertFalse(behavior.isScrollEnabled)
        behavior.enableScrolling()

        assertTrue(behavior.isScrollEnabled)
    }

    @Test
    fun `disableScrolling sets isScrollEnabled to false`() {
        val behavior = EngineViewScrollingBehavior(testContext, null, ViewPosition.BOTTOM)
        behavior.isScrollEnabled = true

        assertTrue(behavior.isScrollEnabled)
        behavior.disableScrolling()

        assertFalse(behavior.isScrollEnabled)
    }

    private fun createDummyEngineView(context: Context): EngineView = DummyEngineView(context)

    open class DummyEngineView(context: Context) : FrameLayout(context), EngineView {
        override fun setVerticalClipping(clippingHeight: Int) {}
        override fun setDynamicToolbarMaxHeight(height: Int) {}
        override fun setActivityContext(context: Context?) {}
        override fun captureThumbnail(onFinish: (Bitmap?) -> Unit) = Unit
        override fun render(session: EngineSession) {}
        override fun release() {}
        override var selectionActionDelegate: SelectionActionDelegate? = null
    }
}
