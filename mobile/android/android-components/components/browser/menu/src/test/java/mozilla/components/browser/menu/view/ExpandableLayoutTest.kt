/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

package mozilla.components.browser.menu.view

import android.animation.ValueAnimator
import android.graphics.Rect
import android.view.MotionEvent
import android.view.MotionEvent.ACTION_CANCEL
import android.view.MotionEvent.ACTION_DOWN
import android.view.MotionEvent.ACTION_MOVE
import android.view.MotionEvent.ACTION_UP
import android.view.View
import android.view.ViewConfiguration
import android.view.ViewGroup
import android.view.animation.AccelerateDecelerateInterpolator
import android.widget.FrameLayout
import androidx.core.view.marginBottom
import androidx.core.view.marginLeft
import androidx.core.view.marginRight
import androidx.core.view.marginTop
import androidx.core.view.updateLayoutParams
import androidx.test.ext.junit.runners.AndroidJUnit4
import mozilla.components.support.test.any
import mozilla.components.support.test.mock
import mozilla.components.support.test.robolectric.testContext
import org.junit.Assert.assertEquals
import org.junit.Assert.assertFalse
import org.junit.Assert.assertSame
import org.junit.Assert.assertTrue
import org.junit.Test
import org.junit.runner.RunWith
import org.mockito.ArgumentMatchers.anyInt
import org.mockito.Mockito.doAnswer
import org.mockito.Mockito.doReturn
import org.mockito.Mockito.never
import org.mockito.Mockito.spy
import org.mockito.Mockito.verify

@RunWith(AndroidJUnit4::class)
class ExpandableLayoutTest {
    @Test
    fun `GIVEN ExpandableLayout WHEN wrapContentInExpandableView is called THEN it should properly setup a new ExpandableLayout`() {
        val wrappedView = FrameLayout(testContext)
        val blankTouchListener: (() -> Unit) = mock()
        wrappedView.layoutParams = ViewGroup.MarginLayoutParams(11, 12).apply {
            setMargins(13, 14, 15, 16)
        }

        val result = ExpandableLayout.wrapContentInExpandableView(
            wrappedView, 42, blankTouchListener
        )

        assertEquals(FrameLayout.LayoutParams.WRAP_CONTENT, result.wrappedView.layoutParams.height)
        assertEquals(FrameLayout.LayoutParams.WRAP_CONTENT, result.wrappedView.layoutParams.width)
        assertEquals(13, result.wrappedView.marginLeft)
        assertEquals(14, result.wrappedView.marginTop)
        assertEquals(15, result.wrappedView.marginRight)
        assertEquals(16, result.wrappedView.marginBottom)
        assertEquals(1, result.childCount)
        assertSame(wrappedView, result.wrappedView)
        assertSame(blankTouchListener, result.blankTouchListener)

        // Also test the default configuration of a newly built ExpandableLayout.
        assertEquals(42, result.lastVisibleItemIndexWhenCollapsed)
        assertEquals(ExpandableLayout.NOT_CALCULATED_DEFAULT_HEIGHT, result.collapsedHeight)
        assertEquals(ExpandableLayout.NOT_CALCULATED_DEFAULT_HEIGHT, result.expandedHeight)
        assertEquals(ExpandableLayout.NOT_CALCULATED_DEFAULT_HEIGHT, result.parentHeight)
        assertTrue(result.isCollapsed)
        assertFalse(result.isExpandInProgress)
        assertEquals(ViewConfiguration.get(testContext).scaledTouchSlop.toFloat(), result.touchSlop)
        assertEquals(ExpandableLayout.NOT_CALCULATED_Y_TOUCH_COORD, result.initialYCoord)
    }

    @Test
    fun `GIVEN ExpandableLayout WHEN onMeasure is called THEN it delegates the parent for measuring`() {
        val expandableLayout = spy(ExpandableLayout.wrapContentInExpandableView(
            FrameLayout(testContext), 1) { }
        )
        expandableLayout.isCollapsed = false

        expandableLayout.measure(123, 123)

        verify(expandableLayout).callParentOnMeasure(123, 123)
    }

    @Test
    fun `GIVEN ExpandableLayout in the collapsed state and the height values available WHEN onMeasure is called THEN it will trigger collapse()`() {
        val expandableLayout = spy(ExpandableLayout.wrapContentInExpandableView(
            FrameLayout(testContext), 1) { }
        )
        expandableLayout.isCollapsed = true
        doReturn(100).`when`(expandableLayout).getOrCalculateCollapsedHeight()
        doReturn(100).`when`(expandableLayout).getOrCalculateExpandedHeight(anyInt())

        expandableLayout.measure(123, 123)

        verify(expandableLayout).collapse()
    }

    @Test
    fun `GIVEN ExpandableLayout not in the collapsed state but all height values known WHEN onMeasure is called THEN collapse() is not called`() {
        val expandableLayout = spy(ExpandableLayout.wrapContentInExpandableView(
            FrameLayout(testContext), 1) { }
        )
        expandableLayout.isCollapsed = false
        doReturn(100).`when`(expandableLayout).getOrCalculateCollapsedHeight()
        doReturn(100).`when`(expandableLayout).getOrCalculateExpandedHeight(anyInt())

        expandableLayout.measure(123, 123)

        verify(expandableLayout, never()).collapse()
    }

    @Test
    fun `GIVEN ExpandableLayout in the collapsed state but with collapsedHeight unknown WHEN onMeasure is called THEN collapse() is be called`() {
        val expandableLayout = spy(ExpandableLayout.wrapContentInExpandableView(
            FrameLayout(testContext), 1) { }
        )
        expandableLayout.isCollapsed = true
        doReturn(ExpandableLayout.NOT_CALCULATED_DEFAULT_HEIGHT).`when`(expandableLayout).getOrCalculateCollapsedHeight()
        doReturn(100).`when`(expandableLayout).getOrCalculateExpandedHeight(anyInt())

        expandableLayout.measure(123, 123)

        verify(expandableLayout, never()).collapse()
    }

    @Test
    fun `GIVEN ExpandableLayout in the collapsed state but with expandedHeight unknown WHEN onMeasure is called THEN collapse() is not be called`() {
        val expandableLayout = spy(ExpandableLayout.wrapContentInExpandableView(
            FrameLayout(testContext), 1) { }
        )
        expandableLayout.isCollapsed = true
        doReturn(100).`when`(expandableLayout).getOrCalculateCollapsedHeight()
        doReturn(ExpandableLayout.NOT_CALCULATED_DEFAULT_HEIGHT).`when`(expandableLayout).getOrCalculateExpandedHeight(anyInt())

        expandableLayout.measure(123, 123)

        verify(expandableLayout, never()).collapse()
    }

    @Test
    fun `GIVEN ExpandableLayout WHEN onInterceptTouchEvent is called for an event for which should not be intercepted THEN super is called`() {
        val expandableLayout = spy(ExpandableLayout.wrapContentInExpandableView(
            FrameLayout(testContext), 1) { }
        )
        val event: MotionEvent = mock()
        doReturn(false).`when`(expandableLayout).shouldInterceptTouches()

        expandableLayout.onInterceptTouchEvent(event)

        verify(expandableLayout).callParentOnInterceptTouchEvent(event)
    }

    @Test
    fun `GIVEN ExpandableLayout WHEN onInterceptTouchEvent is called for ACTION_CANCEL or ACTION_UP THEN the events are not intercepted`() {
        val expandableLayout = spy(ExpandableLayout.wrapContentInExpandableView(
            FrameLayout(testContext), 1) { }
        )
        val actionCancel = MotionEvent.obtain(0, 0, ACTION_UP, 0f, 0f, 0)
        val actionUp = MotionEvent.obtain(0, 0, ACTION_CANCEL, 0f, 0f, 0)
        doReturn(true).`when`(expandableLayout).shouldInterceptTouches()

        assertFalse(expandableLayout.onInterceptTouchEvent(actionCancel))
        assertFalse(expandableLayout.onInterceptTouchEvent(actionUp))

        verify(expandableLayout, never()).callParentOnInterceptTouchEvent(any())
    }

    @Test
    fun `GIVEN the wrappedView is in the expand process WHEN onInterceptTouchEvent is called while for new touches THEN they are intercepted`() {
        val expandableLayout = spy(ExpandableLayout.wrapContentInExpandableView(
            FrameLayout(testContext), 1) { }
        )
        val actionDown = MotionEvent.obtain(0, 0, ACTION_DOWN, 0f, 0f, 0)
        doReturn(true).`when`(expandableLayout).shouldInterceptTouches()
        expandableLayout.isExpandInProgress = true

        assertTrue(expandableLayout.onInterceptTouchEvent(actionDown))
        verify(expandableLayout, never()).callParentOnInterceptTouchEvent(any())
    }

    @Test
    fun `GIVEN the wrappedView not in the expand process WHEN onInterceptTouchEvent is called for new touches THEN they are not intercepted`() {
        val expandableLayout = spy(ExpandableLayout.wrapContentInExpandableView(
            FrameLayout(testContext), 1) { }
        )
        val actionDown = MotionEvent.obtain(0, 0, ACTION_DOWN, 0f, 0f, 0)
        doReturn(true).`when`(expandableLayout).shouldInterceptTouches()
        expandableLayout.isExpandInProgress = false

        assertFalse(expandableLayout.onInterceptTouchEvent(actionDown))
        verify(expandableLayout, never()).callParentOnInterceptTouchEvent(any())
    }

    @Test
    fun `GIVEN blankTouchListener set WHEN onInterceptTouchEvent is called for a touch that does not intersect wrappedView's bounds THEN blankTouchListener is called`() {
        var listenerCalled = false
        val listener = spy { listenerCalled = true }
        val expandableLayout = spy(ExpandableLayout.wrapContentInExpandableView(
            FrameLayout(testContext), 1, listener)
        )
        doReturn(false).`when`(expandableLayout).isTouchingTheWrappedView(any())
        val actionDown = MotionEvent.obtain(0, 0, ACTION_DOWN, 0f, 0f, 0)

        expandableLayout.onInterceptTouchEvent(actionDown)

        assertTrue(listenerCalled)
    }

    @Test
    fun `GIVEN blankTouchListener set WHEN onInterceptTouchEvent is called for a touch that intersects wrappedView's bounds THEN blankTouchListener is not called`() {
        var listenerCalled = false
        val listener = spy { listenerCalled = true }
        val expandableLayout = spy(ExpandableLayout.wrapContentInExpandableView(
            FrameLayout(testContext), 1, listener)
        )
        doReturn(true).`when`(expandableLayout).isTouchingTheWrappedView(any())
        val actionDown = MotionEvent.obtain(0, 0, ACTION_DOWN, 0f, 0f, 0)

        expandableLayout.onInterceptTouchEvent(actionDown)

        assertFalse(listenerCalled)
    }

    @Test
    fun `GIVEN initialYCoord set WHEN onInterceptTouchEvent is called for ACTION_DOWN THEN initialYCoord will be reset to the new value`() {
        val expandableLayout = ExpandableLayout.wrapContentInExpandableView(
            FrameLayout(testContext), 1
        ) { }

        val actionDown1 = MotionEvent.obtain(0, 0, ACTION_DOWN, 0f, 22f, 0)
        expandableLayout.onInterceptTouchEvent(actionDown1)
        assertEquals(22f, expandableLayout.initialYCoord)

        val actionDown2 = MotionEvent.obtain(0, 0, ACTION_DOWN, 0f, -33f, 0)
        expandableLayout.onInterceptTouchEvent(actionDown2)
        assertEquals(-33f, expandableLayout.initialYCoord)
    }

    @Test
    fun `GIVEN ExpandableLayout is in the expand process WHEN onInterceptTouchEvent is called for scroll events THEN these events are intercepted`() {
        val expandableLayout = spy(ExpandableLayout.wrapContentInExpandableView(
            FrameLayout(testContext), 1) { }
        )
        val actionDown = MotionEvent.obtain(0, 0, ACTION_MOVE, 0f, 0f, 0)
        doReturn(true).`when`(expandableLayout).shouldInterceptTouches()
        expandableLayout.isExpandInProgress = true

        assertTrue(expandableLayout.onInterceptTouchEvent(actionDown))
        verify(expandableLayout, never()).expand()
        verify(expandableLayout, never()).callParentOnInterceptTouchEvent(any())
    }

    @Test
    fun `GIVEN the wrappedView is expanded but smaller than the parent WHEN onInterceptTouchEvent is called for scroll events THEN these events are intercepted`() {
        val expandableLayout = spy(ExpandableLayout.wrapContentInExpandableView(
            FrameLayout(testContext), 1) { }
        )
        val actionDown = MotionEvent.obtain(0, 0, ACTION_MOVE, 0f, 0f, 0)
        doReturn(true).`when`(expandableLayout).shouldInterceptTouches()
        expandableLayout.isExpandInProgress = false
        expandableLayout.isCollapsed = false

        assertTrue(expandableLayout.onInterceptTouchEvent(actionDown))
        verify(expandableLayout, never()).expand()
        verify(expandableLayout, never()).callParentOnInterceptTouchEvent(any())
    }

    @Test
    fun `GIVEN the wrappedView is not expanding WHEN onInterceptTouchEvent is called for an event that is not a scroll THEN this event is not intercepted`() {
        val expandableLayout = spy(ExpandableLayout.wrapContentInExpandableView(
            FrameLayout(testContext), 1) { }
        )
        val actionDown = MotionEvent.obtain(0, 0, ACTION_MOVE, 0f, 0f, 0)
        doReturn(true).`when`(expandableLayout).shouldInterceptTouches()
        doReturn(false).`when`(expandableLayout).isScrollingUp(any())
        expandableLayout.isExpandInProgress = false

        assertFalse(expandableLayout.onInterceptTouchEvent(actionDown))
        verify(expandableLayout, never()).expand()
        verify(expandableLayout, never()).callParentOnInterceptTouchEvent(any())
    }

    @Test
    fun `GIVEN the wrappedView is not expanding WHEN onInterceptTouchEvent is called for scroll up events THEN the events are intercepted and expand() is called`() {
        val expandableLayout = spy(ExpandableLayout.wrapContentInExpandableView(
            FrameLayout(testContext), 1) { }
        )
        val actionDown = MotionEvent.obtain(0, 0, ACTION_MOVE, 0f, 0f, 0)
        doReturn(true).`when`(expandableLayout).shouldInterceptTouches()
        doReturn(true).`when`(expandableLayout).isScrollingUp(any())
        expandableLayout.isExpandInProgress = false

        assertTrue(expandableLayout.onInterceptTouchEvent(actionDown))
        verify(expandableLayout).expand()
        verify(expandableLayout, never()).callParentOnInterceptTouchEvent(any())
    }

    @Test
    fun `GIVEN isTouchForWrappedView() WHEN called with a touch event that is within the wrappedView bounds THEN it returns true`() {
        val wrappedView = spy(FrameLayout(testContext))
        doAnswer {
            val rect = it.arguments[0] as Rect
            rect.set(0, 0, 100, 100)
        }.`when`(wrappedView).getHitRect(any())
        val expandableLayout = ExpandableLayout.wrapContentInExpandableView(
            wrappedView, 1
        ) { }
        val inBoundsEvent = MotionEvent.obtain(0, 0, ACTION_DOWN, 5f, 5f, 0)

        assertTrue(expandableLayout.isTouchingTheWrappedView(inBoundsEvent))
    }

    @Test
    fun `GIVEN isTouchForWrappedView WHEN called with a touch event that is not within the wrappedView bounds THEN it returns false`() {
        val wrappedView = spy(FrameLayout(testContext))
        doAnswer {
            val rect = it.arguments[0] as Rect
            rect.set(0, 0, 100, 100)
        }.`when`(wrappedView).getHitRect(any())
        val expandableLayout = ExpandableLayout.wrapContentInExpandableView(
            wrappedView, 1
        ) { }
        val outOfBoundsEvent = MotionEvent.obtain(0, 0, ACTION_DOWN, 105f, 105f, 0)

        assertFalse(expandableLayout.isTouchingTheWrappedView(outOfBoundsEvent))
    }

    @Test
    fun `GIVEN ExpandableLayout in the collapsed state WHEN shouldInterceptTouches is called THEN it returns true`() {
        val expandableLayout = ExpandableLayout.wrapContentInExpandableView(
            FrameLayout(testContext), 1
        ) { }
        expandableLayout.isCollapsed = true

        assertTrue(expandableLayout.shouldInterceptTouches())
    }

    @Test
    fun `GIVEN ExpandableLayout not collapsed and the wrappedView smaller than it's parent WHEN shouldInterceptTouches is called THEN it returns true`() {
        val expandableLayout = ExpandableLayout.wrapContentInExpandableView(
            FrameLayout(testContext), 1
        ) { }
        expandableLayout.isCollapsed = false
        expandableLayout.parentHeight = 101
        expandableLayout.expandedHeight = 100

        assertTrue(expandableLayout.shouldInterceptTouches())
    }

    @Test
    fun `GIVEN ExpandableLayout not collapsed and the wrappedView bigger than it's parent WHEN shouldInterceptTouches is called THEN it returns false`() {
        val expandableLayout = ExpandableLayout.wrapContentInExpandableView(
            FrameLayout(testContext), 1
        ) { }
        expandableLayout.isCollapsed = false
        expandableLayout.parentHeight = 101
        expandableLayout.expandedHeight = 102

        assertFalse(expandableLayout.shouldInterceptTouches())
    }

    @Test
    fun `GIVEN ExpandableLayout WHEN collapse is called THEN it sets a positive translation and a smaller height for the wrappedView`() {
        val expandableLayout = ExpandableLayout.wrapContentInExpandableView(
            spy(FrameLayout(testContext)), 1
        ) { }
        expandableLayout.wrappedView.updateLayoutParams {
            height = 100
        }
        expandableLayout.parentHeight = 200
        expandableLayout.collapsedHeight = 50

        expandableLayout.collapse()

        // If the available height is 200, with the layout starting at 0,0
        // to properly "anchor" a 50px height wrappedView we need to translate it 150px.
        verify(expandableLayout.wrappedView).translationY = 150f
        assertEquals(50, expandableLayout.wrappedView.layoutParams.height)
    }

    @Test
    fun `GIVEN ExpandableLayout WHEN getExpandViewAnimator is called THEN it returns a new ValueAnimator`() {
        val expandableLayout = ExpandableLayout.wrapContentInExpandableView(
            FrameLayout(testContext), 1
        ) { }

        val result = expandableLayout.getExpandViewAnimator(100)

        assertTrue(result.interpolator is AccelerateDecelerateInterpolator)
        assertEquals(ExpandableLayout.DEFAULT_DURATION_EXPAND_ANIMATOR, result.duration)
    }

    @Test
    fun `GIVEN ExpandableLayout WHEN expand is called THEN it updates the translationY and height to show the wrappedView with expandedHeight`() {
        val expandableLayout = spy(ExpandableLayout.wrapContentInExpandableView(
            FrameLayout(testContext), 1) { }
        )
        val animator = ValueAnimator.ofInt(0, 100)
        doAnswer {
            animator
        }.`when`(expandableLayout).getExpandViewAnimator(anyInt())
        expandableLayout.expandedHeight = 100
        expandableLayout.collapsedHeight = 50
        expandableLayout.wrappedView.translationY = 50f

        expandableLayout.expand()
        animator.end()

        verify(expandableLayout).getExpandViewAnimator(50)
        assertEquals(-50f, expandableLayout.wrappedView.translationY)
        assertEquals(150, expandableLayout.wrappedView.layoutParams.height)
        assertTrue(System.currentTimeMillis() > 0)
    }

    @Test
    fun `GIVEN collapsedHeight if already calculated WHEN getOrCalculateCollapsedHeight is called THEN it returns collapsedHeight`() {
        val expandableLayout = spy(ExpandableLayout.wrapContentInExpandableView(
            FrameLayout(testContext), 1) { }
        )
        expandableLayout.collapsedHeight = 123

        val result = expandableLayout.getOrCalculateCollapsedHeight()

        verify(expandableLayout, never()).calculateCollapsedHeight()
        assertEquals(123, result)
    }

    @Test
    fun `GIVEN collapsedHeight is not already calculated WHEN getOrCalculateCollapsedHeight is called THEN it delegates calculateCollapsedHeight and returns the value from that`() {
        val expandableLayout = spy(ExpandableLayout.wrapContentInExpandableView(
            FrameLayout(testContext), 1) { }
        )
        doReturn(42).`when`(expandableLayout).calculateCollapsedHeight()

        val result = expandableLayout.getOrCalculateCollapsedHeight()

        verify(expandableLayout).calculateCollapsedHeight()
        assertEquals(42, result)
    }

    @Test
    fun `GIVEN expandedHeight not calculated WHEN getOrCalculateExpandedHeight is called THEN it sets expandedHeight with the value of measuredHeight`() {
        val wrappedView = spy(FrameLayout(testContext))
        val expandableLayout = spy(ExpandableLayout.wrapContentInExpandableView(
            wrappedView, 1) { }
        )

        doReturn(100).`when`(wrappedView).measuredHeight
        assertEquals(100, expandableLayout.getOrCalculateExpandedHeight(0))
        assertEquals(100, expandableLayout.expandedHeight)

        doReturn(200).`when`(wrappedView).measuredHeight
        assertEquals(100, expandableLayout.getOrCalculateExpandedHeight(0))
        assertEquals(100, expandableLayout.expandedHeight)
    }

    @Test
    fun `GIVEN parentHeight not already calculated WHEN getOrCalculateExpandedHeight is called THEN it sets parentHeight with the value from the heightSpec size`() {
        val expandableLayout = spy(ExpandableLayout.wrapContentInExpandableView(
            FrameLayout(testContext), 1) { }
        )

        expandableLayout.getOrCalculateExpandedHeight(View.MeasureSpec.makeMeasureSpec(123, View.MeasureSpec.EXACTLY))
        assertEquals(123, expandableLayout.parentHeight)

        expandableLayout.getOrCalculateExpandedHeight(View.MeasureSpec.makeMeasureSpec(321, View.MeasureSpec.EXACTLY))
        assertEquals(123, expandableLayout.parentHeight)
    }

    @Test
    fun `GIVEN parentHeight not calculated WHEN getOrCalculateExpandedHeight is called with a parent height THEN it sets the expandedHeight as the minimum of between expandedHeight and parent height`() {
        val expandableLayout = spy(ExpandableLayout.wrapContentInExpandableView(
            FrameLayout(testContext), 1) { }
        )
        expandableLayout.expandedHeight = 123

        expandableLayout.getOrCalculateExpandedHeight(View.MeasureSpec.makeMeasureSpec(101, View.MeasureSpec.EXACTLY))
        assertEquals(101, expandableLayout.expandedHeight)

        expandableLayout.getOrCalculateExpandedHeight(View.MeasureSpec.makeMeasureSpec(222, View.MeasureSpec.EXACTLY))
        assertEquals(101, expandableLayout.expandedHeight)
    }

    @Test
    fun `GIVEN getOrCalculateExpandedHeight() WHEN calculating the collapsed height to be bigger than the available screen height THEN it cancels collapsing`() {
        val expandableLayout = spy(ExpandableLayout.wrapContentInExpandableView(
            FrameLayout(testContext), 1) { }
        )
        expandableLayout.collapsedHeight = 50
        expandableLayout.expandedHeight = 100

        var result = expandableLayout.getOrCalculateExpandedHeight(View.MeasureSpec.makeMeasureSpec(100, View.MeasureSpec.EXACTLY))
        assertEquals(100, result)
        assertTrue(expandableLayout.isCollapsed)
        assertFalse(expandableLayout.isExpandInProgress)

        // Reset parent height. Simulate entirely new calculations to have the code passing an if check
        expandableLayout.parentHeight = -1
        expandableLayout.collapsedHeight = 1_000
        result = expandableLayout.getOrCalculateExpandedHeight(View.MeasureSpec.makeMeasureSpec(100, View.MeasureSpec.EXACTLY))
        assertEquals(100, result)
        assertFalse(expandableLayout.isCollapsed)
        assertFalse(expandableLayout.isExpandInProgress)
    }

    @Test
    fun `GIVEN a set touchSlop WHEN isScrollingUp calculates that is was exceeded THEN it returns true `() {
        val expandableLayout = ExpandableLayout.wrapContentInExpandableView(
            FrameLayout(testContext), 1
        ) { }
        expandableLayout.initialYCoord = 0f
        expandableLayout.touchSlop = 10f

        var distanceScrolledDown = 11f
        assertFalse(expandableLayout.isScrollingUp(MotionEvent.obtain(0, 0, ACTION_MOVE, 0f, distanceScrolledDown, 0)))
        distanceScrolledDown = 5f
        assertFalse(expandableLayout.isScrollingUp(MotionEvent.obtain(0, 0, ACTION_MOVE, 0f, distanceScrolledDown, 0)))

        var distanceScrolledUp = -11f
        assertTrue(expandableLayout.isScrollingUp(MotionEvent.obtain(0, 0, ACTION_MOVE, 0f, distanceScrolledUp, 0)))
        distanceScrolledUp = -5f
        assertFalse(expandableLayout.isScrollingUp(MotionEvent.obtain(0, 0, ACTION_MOVE, 0f, distanceScrolledUp, 0)))
    }

    @Test
    fun `GIVEN a list of items WHEN calculateCollapsedHeight is called with an item index not found in the items list THEN it returns the value of measuredHeight`() {
        val list = FrameLayout(testContext).apply {
            addView(mock())
            addView(mock())
        }
        val wrappedView = FrameLayout(testContext).apply { addView(list) }
        val measuredHeight = -42
        val expandableLayout = spy(ExpandableLayout.wrapContentInExpandableView(
            wrappedView, 0) { }
        )

        doReturn(measuredHeight).`when`(expandableLayout).measuredHeight

        assertEquals(measuredHeight, expandableLayout.calculateCollapsedHeight())

        // Here we test the list of two items collapsed to the first.
        expandableLayout.lastVisibleItemIndexWhenCollapsed = 1
        assertEquals(0, expandableLayout.calculateCollapsedHeight())

        expandableLayout.lastVisibleItemIndexWhenCollapsed = 2
        assertEquals(measuredHeight, expandableLayout.calculateCollapsedHeight())
    }

    @Test
    fun `GIVEN calculateCollapsedHeight() WHEN called THEN it returns the distance between parent top and half of the SpecialView`() {
        val viewHeightForEachProperty = 1_000
        val listHeightForEachProperty = 100
        val itemHeightForEachProperty = 10
        // Adding Views and creating spies in two stages because otherwise
        // the addView call for a spy will not get us the expected result.
        var list = FrameLayout(testContext).apply {
            addView(spy(View(testContext)).configureWithHeight(itemHeightForEachProperty))
            addView(spy(View(testContext)).configureWithHeight(itemHeightForEachProperty))
            addView(spy(View(testContext)).configureWithHeight(itemHeightForEachProperty))
        }
        list = spy(list).configureWithHeight(listHeightForEachProperty)
        var wrappedView = FrameLayout(testContext).apply { addView(list) }
        wrappedView = spy(wrappedView).configureWithHeight(viewHeightForEachProperty)
        val expandableLayout = ExpandableLayout.wrapContentInExpandableView(wrappedView, 1) { }

        val result = expandableLayout.calculateCollapsedHeight()

        var expected = 0
        expected += viewHeightForEachProperty * 4 // marginTop + marginBottom + paddingTop + paddingBottom
        expected += listHeightForEachProperty * 4 // marginTop + marginBottom + paddingTop + paddingBottom
        expected += itemHeightForEachProperty * 5 // height + marginTop + marginBottom + paddingTop + paddingBottom for the top item shown in entirety
        expected += itemHeightForEachProperty * 2 // marginTop + paddingTop for the special view
        expected += itemHeightForEachProperty / 2 // as per the specs, show only half of the special view
        assertEquals(expected, result)
    }
}

/**
 * Convenience method to set the same value - the received [height] parameter as the
 * height, margins and paddings values for the current View.
 */
fun <V> V.configureWithHeight(height: Int): V where V : View {
    doReturn(height).`when`(this).measuredHeight
    layoutParams = ViewGroup.MarginLayoutParams(height, height).apply {
        setMargins(height, height, height, height)
    }
    setPadding(height, height, height, height)

    return this
}
