/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

package mozilla.components.ui.widgets

import android.view.MotionEvent
import android.view.MotionEvent.ACTION_CANCEL
import android.view.MotionEvent.ACTION_DOWN
import android.view.MotionEvent.ACTION_MOVE
import android.view.MotionEvent.ACTION_POINTER_DOWN
import android.view.MotionEvent.ACTION_UP
import android.view.View
import androidx.test.ext.junit.runners.AndroidJUnit4
import mozilla.components.support.test.mock
import mozilla.components.support.test.robolectric.testContext
import mozilla.components.ui.widgets.VerticalSwipeRefreshLayout.QuickScaleEvents
import org.junit.Assert.assertEquals
import org.junit.Assert.assertFalse
import org.junit.Assert.assertTrue
import org.junit.Before
import org.junit.Test
import org.junit.runner.RunWith
import org.mockito.Mockito.spy
import org.mockito.Mockito.times
import org.mockito.Mockito.verify

@RunWith(AndroidJUnit4::class)
class VerticalSwipeRefreshLayoutTest {
    private lateinit var swipeLayout: VerticalSwipeRefreshLayout

    @Before
    fun setup() {
        swipeLayout = VerticalSwipeRefreshLayout(testContext)
    }

    @Test
    fun `onInterceptTouchEvent should abort pull to refresh and return false if the View is disabled`() {
        swipeLayout = spy(swipeLayout)
        val secondFingerEvent = TestUtils.getMotionEvent(ACTION_POINTER_DOWN)

        swipeLayout.isEnabled = false
        assertFalse(swipeLayout.onInterceptTouchEvent(secondFingerEvent))
        verify(swipeLayout, times(0)).callSuperOnInterceptTouchEvent(secondFingerEvent)
    }

    @Test
    fun `onInterceptTouchEvent should abort pull to refresh and return false if zoom is in progress`() {
        swipeLayout = spy(swipeLayout)
        val downEvent = TestUtils.getMotionEvent(ACTION_DOWN, 0f, 0f)
        val pointerDownEvent =
            TestUtils.getMotionEvent(ACTION_POINTER_DOWN, 200f, 200f, previousEvent = downEvent)
        swipeLayout.isEnabled = true
        swipeLayout.setOnChildScrollUpCallback { _, _ -> true }

        swipeLayout.onInterceptTouchEvent(downEvent)
        verify(swipeLayout, times(1)).callSuperOnInterceptTouchEvent(downEvent)

        swipeLayout.onInterceptTouchEvent(pointerDownEvent)
        assertFalse(swipeLayout.onInterceptTouchEvent(pointerDownEvent))
        verify(swipeLayout, times(0)).callSuperOnInterceptTouchEvent(pointerDownEvent)
    }

    @Test
    fun `onInterceptTouchEvent should cleanup if ACTION_CANCEL`() {
        swipeLayout = spy(swipeLayout)
        val cancelEvent = TestUtils.getMotionEvent(
            ACTION_CANCEL,
            previousEvent = TestUtils.getMotionEvent(ACTION_DOWN),
        )
        swipeLayout.isEnabled = true
        swipeLayout.setOnChildScrollUpCallback { _, _ -> true }

        swipeLayout.onInterceptTouchEvent(cancelEvent)

        verify(swipeLayout).forgetQuickScaleEvents()
        verify(swipeLayout).callSuperOnInterceptTouchEvent(cancelEvent)
    }

    @Test
    fun `onInterceptTouchEvent should cleanup if quick scale ended`() {
        swipeLayout = spy(swipeLayout)
        val upEvent = TestUtils.getMotionEvent(
            ACTION_CANCEL,
            previousEvent = TestUtils.getMotionEvent(ACTION_DOWN),
        )
        swipeLayout.isEnabled = true
        swipeLayout.isQuickScaleInProgress = true
        swipeLayout.setOnChildScrollUpCallback { _, _ -> true }

        swipeLayout.onInterceptTouchEvent(upEvent)

        verify(swipeLayout).forgetQuickScaleEvents()
        verify(swipeLayout).callSuperOnInterceptTouchEvent(upEvent)
    }

    @Test
    fun `onInterceptTouchEvent should disable pull to refresh if quick scale is in progress`() {
        // default DOUBLE_TAP_TIMEOUT is 300ms

        swipeLayout = spy(swipeLayout)
        val firstDownEvent = TestUtils.getMotionEvent(ACTION_DOWN, eventTime = 100)
        val upEvent =
            TestUtils.getMotionEvent(ACTION_UP, eventTime = 200, previousEvent = firstDownEvent)
        val newDownEvent =
            TestUtils.getMotionEvent(ACTION_DOWN, eventTime = 500, previousEvent = upEvent)
        val previousEvents = QuickScaleEvents(firstDownEvent, upEvent, null)
        swipeLayout.quickScaleEvents = previousEvents
        swipeLayout.isQuickScaleInProgress = false

        assertFalse(swipeLayout.onInterceptTouchEvent(newDownEvent))
        assertTrue(swipeLayout.isQuickScaleInProgress)
        verify(swipeLayout).maybeAddDoubleTapEvent(newDownEvent)
        verify(swipeLayout, times(0)).callSuperOnInterceptTouchEvent(newDownEvent)
    }

    @Test
    fun `onInterceptTouchEvent should disable pull to refresh if move was more on the x axys`() {
        // default DOUBLE_TAP_TIMEOUT is 300ms

        swipeLayout = spy(swipeLayout)
        val downEvent = TestUtils.getMotionEvent(ACTION_DOWN, x = 0f, y = 0f, eventTime = 0)
        val moveEvent = TestUtils.getMotionEvent(
            ACTION_MOVE,
            x = 1f,
            y = 0f,
            eventTime = 100,
            previousEvent = downEvent,
        )
        swipeLayout.isEnabled = true
        swipeLayout.isQuickScaleInProgress = false
        swipeLayout.setOnChildScrollUpCallback { _, _ -> false }

        swipeLayout.onInterceptTouchEvent(downEvent)
        verify(swipeLayout).callSuperOnInterceptTouchEvent(downEvent)

        assertFalse(swipeLayout.onInterceptTouchEvent(moveEvent))
        verify(swipeLayout, times(0)).callSuperOnInterceptTouchEvent(moveEvent)
    }

    @Test
    fun `onInterceptTouchEvent should allow pull to refresh if move was more on the y axys`() {
        // default DOUBLE_TAP_TIMEOUT is 300ms

        swipeLayout = spy(swipeLayout)
        val downEvent = TestUtils.getMotionEvent(ACTION_DOWN, x = 0f, y = 0f, eventTime = 0)
        val moveEvent = TestUtils.getMotionEvent(
            ACTION_MOVE,
            x = 0f,
            y = 1f,
            eventTime = 100,
            previousEvent = downEvent,
        )
        swipeLayout.isEnabled = true
        swipeLayout.isQuickScaleInProgress = false
        swipeLayout.setOnChildScrollUpCallback { _, _ -> false }

        swipeLayout.onInterceptTouchEvent(downEvent)
        verify(swipeLayout).callSuperOnInterceptTouchEvent(downEvent)

        swipeLayout.onInterceptTouchEvent(moveEvent)
        verify(swipeLayout).callSuperOnInterceptTouchEvent(moveEvent)
    }

    @Test
    fun `Should not respond descendants initiated scrolls if this View is enabled`() {
        swipeLayout = spy(swipeLayout)
        val childView: View = mock()
        val targetView: View = mock()
        val scrollAxis = 0

        swipeLayout.isEnabled = true

        assertFalse(swipeLayout.onStartNestedScroll(childView, targetView, scrollAxis))
        verify(swipeLayout, times(0)).callSuperOnStartNestedScroll(
            childView,
            targetView,
            scrollAxis,
        )
    }

    @Test
    fun `Should delegate super#onStartNestedScroll if this View is not enabled`() {
        swipeLayout = spy(swipeLayout)
        val childView: View = mock()
        val targetView: View = mock()
        val scrollAxis = 0

        swipeLayout.isEnabled = false
        swipeLayout.onStartNestedScroll(childView, targetView, scrollAxis)

        verify(swipeLayout).callSuperOnStartNestedScroll(childView, targetView, scrollAxis)
    }

    @Test
    fun `maybeAddDoubleTapEvent should not modify quickScaleEvents if not for ACTION_DOWN or ACTION_UP`() {
        val emptyListOfEvents = QuickScaleEvents()
        swipeLayout.quickScaleEvents = emptyListOfEvents

        swipeLayout.maybeAddDoubleTapEvent(TestUtils.getMotionEvent(ACTION_POINTER_DOWN))

        assertEquals(emptyListOfEvents, swipeLayout.quickScaleEvents)
    }

    @Test
    fun `maybeAddDoubleTapEvent will add ACTION_UP as second event if there is already one event in sequence`() {
        val firstEvent = spy(TestUtils.getMotionEvent(ACTION_DOWN))
        val secondEvent =
            spy(TestUtils.getMotionEvent(ACTION_UP, eventTime = 133, previousEvent = firstEvent))
        val expectedResult = Triple<MotionEvent?, MotionEvent?, MotionEvent?>(
            firstEvent,
            secondEvent,
            null,
        )
        swipeLayout.quickScaleEvents = QuickScaleEvents(firstEvent, null, null)

        swipeLayout.maybeAddDoubleTapEvent(secondEvent)

        // A Triple assert or MotionEvent assert fails probably because of the copies made
        // Verifying the expected actions and eventTime should be good enough.
        assertEquals(expectedResult.first, swipeLayout.quickScaleEvents.firstDownEvent)
        assertEquals(
            expectedResult.second!!.actionMasked,
            swipeLayout.quickScaleEvents.upEvent!!.actionMasked,
        )
        assertEquals(
            expectedResult.second!!.eventTime,
            swipeLayout.quickScaleEvents.upEvent!!.eventTime,
        )
        assertEquals(null, swipeLayout.quickScaleEvents.secondDownEvent)
    }

    @Test
    fun `maybeAddDoubleTapEvent will not add ACTION_UP if there is not a first event already in sequence`() {
        val firstEvent = spy(TestUtils.getMotionEvent(ACTION_DOWN))
        val secondEvent =
            spy(TestUtils.getMotionEvent(ACTION_UP, eventTime = 133, previousEvent = firstEvent))
        val expectedResult = QuickScaleEvents()
        swipeLayout.quickScaleEvents = expectedResult

        swipeLayout.maybeAddDoubleTapEvent(secondEvent)

        assertEquals(null, swipeLayout.quickScaleEvents.firstDownEvent)
        assertEquals(null, swipeLayout.quickScaleEvents.upEvent)
        assertEquals(null, swipeLayout.quickScaleEvents.secondDownEvent)
    }

    @Test
    fun `maybeAddDoubleTapEvent will add the first ACTION_DOWN if the events list is otherwise empty`() {
        swipeLayout = spy(swipeLayout)
        val emptyListOfEvents = QuickScaleEvents()
        val downEvent = TestUtils.getMotionEvent(ACTION_DOWN, eventTime = 133)
        swipeLayout.quickScaleEvents = emptyListOfEvents

        swipeLayout.maybeAddDoubleTapEvent(downEvent)

        verify(swipeLayout).forgetQuickScaleEvents()
        assertEquals(downEvent.actionMasked, swipeLayout.quickScaleEvents.firstDownEvent!!.actionMasked)
        assertEquals(downEvent.eventTime, swipeLayout.quickScaleEvents.firstDownEvent!!.eventTime)
        assertEquals(null, swipeLayout.quickScaleEvents.upEvent)
        assertEquals(null, swipeLayout.quickScaleEvents.secondDownEvent)
    }

    @Test
    fun `maybeAddDoubleTapEvent will reset the first ACTION_DOWN if the events list does not contain other events`() {
        swipeLayout = spy(swipeLayout)
        val previousDownEvent = TestUtils.getMotionEvent(ACTION_DOWN, eventTime = 111)
        val previousEvents = QuickScaleEvents(previousDownEvent, null, null)
        val newDownEvent = TestUtils.getMotionEvent(ACTION_DOWN, eventTime = 222)
        swipeLayout.quickScaleEvents = previousEvents

        swipeLayout.maybeAddDoubleTapEvent(newDownEvent)

        verify(swipeLayout).forgetQuickScaleEvents()
        assertEquals(newDownEvent.actionMasked, swipeLayout.quickScaleEvents.firstDownEvent!!.actionMasked)
        assertEquals(newDownEvent.eventTime, swipeLayout.quickScaleEvents.firstDownEvent!!.eventTime)
        assertEquals(null, swipeLayout.quickScaleEvents.upEvent)
        assertEquals(null, swipeLayout.quickScaleEvents.secondDownEvent)
    }

    @Test
    fun `maybeAddDoubleTapEvent will reset ACTION_DOWN if timeout was reached`() {
        // default DOUBLE_TAP_TIMEOUT is 300ms

        swipeLayout = spy(swipeLayout)
        val firstDownEvent = TestUtils.getMotionEvent(ACTION_DOWN, eventTime = 100)
        val upEvent =
            TestUtils.getMotionEvent(ACTION_UP, eventTime = 200, previousEvent = firstDownEvent)
        val newDownEvent =
            TestUtils.getMotionEvent(ACTION_DOWN, eventTime = 501, previousEvent = upEvent)
        val previousEvents = QuickScaleEvents(firstDownEvent, upEvent, null)
        swipeLayout.quickScaleEvents = previousEvents

        swipeLayout.maybeAddDoubleTapEvent(newDownEvent)

        verify(swipeLayout).forgetQuickScaleEvents()
        assertEquals(newDownEvent.actionMasked, swipeLayout.quickScaleEvents.firstDownEvent!!.actionMasked)
        assertEquals(newDownEvent.eventTime, swipeLayout.quickScaleEvents.firstDownEvent!!.eventTime)
        assertEquals(null, swipeLayout.quickScaleEvents.upEvent)
        assertEquals(null, swipeLayout.quickScaleEvents.secondDownEvent)
    }

    @Test
    fun `maybeAddDoubleTapEvent will add a second ACTION_DOWN already have two events and timeout is not reached`() {
        // default DOUBLE_TAP_TIMEOUT is 300ms

        swipeLayout = spy(swipeLayout)
        val firstDownEvent = TestUtils.getMotionEvent(ACTION_DOWN, eventTime = 100)
        val upEvent =
            TestUtils.getMotionEvent(ACTION_UP, eventTime = 200, previousEvent = firstDownEvent)
        val newDownEvent =
            TestUtils.getMotionEvent(ACTION_DOWN, eventTime = 500, previousEvent = upEvent)
        val previousEvents = QuickScaleEvents(firstDownEvent, upEvent, null)
        swipeLayout.quickScaleEvents = previousEvents

        swipeLayout.maybeAddDoubleTapEvent(newDownEvent)

        verify(swipeLayout, times(0)).forgetQuickScaleEvents()
        assertEquals(firstDownEvent.actionMasked, swipeLayout.quickScaleEvents.firstDownEvent!!.actionMasked)
        assertEquals(firstDownEvent.eventTime, swipeLayout.quickScaleEvents.firstDownEvent!!.eventTime)
        assertEquals(upEvent.actionMasked, swipeLayout.quickScaleEvents.upEvent!!.actionMasked)
        assertEquals(upEvent.eventTime, swipeLayout.quickScaleEvents.upEvent!!.eventTime)
        assertEquals(newDownEvent.actionMasked, swipeLayout.quickScaleEvents.secondDownEvent!!.actionMasked)
        assertEquals(newDownEvent.eventTime, swipeLayout.quickScaleEvents.secondDownEvent!!.eventTime)
    }

    @Test
    fun `forgetQuickScaleEvents should recycle all events and reset the quickScaleStatus`() {
        val firstEvent = spy(TestUtils.getMotionEvent(ACTION_DOWN))
        val secondEvent = spy(TestUtils.getMotionEvent(ACTION_UP, previousEvent = firstEvent))
        val thirdEvent = spy(TestUtils.getMotionEvent(ACTION_DOWN))
        swipeLayout.quickScaleEvents = QuickScaleEvents(firstEvent, secondEvent, thirdEvent)
        swipeLayout.isQuickScaleInProgress = true

        swipeLayout.forgetQuickScaleEvents()

        verify(firstEvent).recycle()
        verify(secondEvent).recycle()
        verify(thirdEvent).recycle()
        assertEquals(QuickScaleEvents(), swipeLayout.quickScaleEvents)
        assertFalse(swipeLayout.isQuickScaleInProgress)
    }

    @Test
    fun `isQuickScaleInProgress should return false if timeout was reached`() {
        // default DOUBLE_TAP_TIMEOUT is 300ms

        val firstEvent = TestUtils.getMotionEvent(ACTION_DOWN)
        val secondEvent = TestUtils.getMotionEvent(ACTION_UP, 0f, 0f, 0, firstEvent)
        val thirdEvent = TestUtils.getMotionEvent(ACTION_DOWN, 0f, 0f, 301L, secondEvent)

        assertFalse(swipeLayout.isQuickScaleInProgress(firstEvent, secondEvent, thirdEvent))
    }

    @Test
    fun `isQuickScaleInProgress should return false if taps were too apart`() {
        val firstEvent = TestUtils.getMotionEvent(ACTION_DOWN)
        val secondEvent = TestUtils.getMotionEvent(ACTION_UP, 0f, 0f, 0, firstEvent)
        val thirdEvent = TestUtils.getMotionEvent(ACTION_DOWN, 200f, 20f, 200L, secondEvent)

        assertFalse(swipeLayout.isQuickScaleInProgress(firstEvent, secondEvent, thirdEvent))
    }

    @Test
    fun `isQuickScaleInProgress should return true if taps were close`() {
        val firstEvent = TestUtils.getMotionEvent(ACTION_DOWN)
        val secondEvent = TestUtils.getMotionEvent(ACTION_UP, 0f, 0f, 0, firstEvent)
        val thirdEvent = TestUtils.getMotionEvent(ACTION_DOWN, 20f, 20f, 200L, secondEvent)

        assertTrue(swipeLayout.isQuickScaleInProgress(firstEvent, secondEvent, thirdEvent))
    }

    @Test
    fun `isQuickScaleInProgress should return false if any event is null`() {
        // Using the same values as in the above test that asserts true
        val firstEvent = TestUtils.getMotionEvent(ACTION_DOWN)
        val secondEvent = TestUtils.getMotionEvent(ACTION_UP, 0f, 0f, 0, firstEvent)
        val thirdEvent = TestUtils.getMotionEvent(ACTION_DOWN, 20f, 20f, 200L, secondEvent)
        val oneNullEvent = QuickScaleEvents(firstEvent, secondEvent, null)
        val twoNullEvents = QuickScaleEvents(null, null, thirdEvent)
        val allNullEvents = QuickScaleEvents(null, null, null)

        assertFalse(swipeLayout.isQuickScaleInProgress(oneNullEvent))
        assertFalse(swipeLayout.isQuickScaleInProgress(twoNullEvents))
        assertFalse(swipeLayout.isQuickScaleInProgress(allNullEvents))
    }

    @Test
    fun `isQuickScaleInProgress should return true for valid sequence of non null events`() {
        // Using the same values as in the above test that asserts true
        swipeLayout = spy(swipeLayout)
        val firstEvent = TestUtils.getMotionEvent(ACTION_DOWN)
        val secondEvent = TestUtils.getMotionEvent(ACTION_UP, 0f, 0f, 0, firstEvent)
        val thirdEvent = TestUtils.getMotionEvent(ACTION_DOWN, 20f, 20f, 200L, secondEvent)
        val quickScaleEvents = QuickScaleEvents(firstEvent, secondEvent, thirdEvent)

        assertTrue(swipeLayout.isQuickScaleInProgress(quickScaleEvents))
        verify(swipeLayout).isQuickScaleInProgress(firstEvent, secondEvent, thirdEvent)
    }
}
