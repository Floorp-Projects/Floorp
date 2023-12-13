/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

package mozilla.components.browser.engine.gecko

import android.app.Activity
import android.content.Context
import android.os.Looper.getMainLooper
import android.view.MotionEvent
import android.view.MotionEvent.ACTION_CANCEL
import android.view.MotionEvent.ACTION_DOWN
import android.view.MotionEvent.ACTION_MOVE
import android.view.MotionEvent.ACTION_UP
import android.widget.FrameLayout
import androidx.core.view.NestedScrollingChildHelper
import androidx.core.view.ViewCompat.SCROLL_AXIS_VERTICAL
import androidx.test.ext.junit.runners.AndroidJUnit4
import mozilla.components.concept.engine.INPUT_HANDLING_UNKNOWN
import mozilla.components.support.test.any
import mozilla.components.support.test.argumentCaptor
import mozilla.components.support.test.mock
import mozilla.components.support.test.mockMotionEvent
import mozilla.components.support.test.whenever
import org.junit.Assert.assertEquals
import org.junit.Assert.assertNotEquals
import org.junit.Assert.assertTrue
import org.junit.Test
import org.junit.runner.RunWith
import org.mockito.Mockito.anyInt
import org.mockito.Mockito.doReturn
import org.mockito.Mockito.spy
import org.mockito.Mockito.times
import org.mockito.Mockito.verify
import org.mozilla.geckoview.GeckoResult
import org.mozilla.geckoview.PanZoomController
import org.mozilla.geckoview.PanZoomController.INPUT_RESULT_HANDLED
import org.mozilla.geckoview.PanZoomController.INPUT_RESULT_UNHANDLED
import org.mozilla.geckoview.PanZoomController.SCROLLABLE_FLAG_BOTTOM
import org.mozilla.geckoview.PanZoomController.OVERSCROLL_FLAG_VERTICAL
import org.mozilla.geckoview.PanZoomController.OVERSCROLL_FLAG_HORIZONTAL
import org.robolectric.Robolectric.buildActivity
import org.robolectric.Shadows.shadowOf

@RunWith(AndroidJUnit4::class)
class NestedGeckoViewTest {

    private val context: Context
        get() = buildActivity(Activity::class.java).get()

    @Test
    fun `NestedGeckoView must delegate NestedScrollingChild implementation to childHelper`() {
        val nestedWebView = NestedGeckoView(context)
        val mockChildHelper: NestedScrollingChildHelper = mock()
        nestedWebView.childHelper = mockChildHelper

        doReturn(true).`when`(mockChildHelper).isNestedScrollingEnabled
        doReturn(true).`when`(mockChildHelper).hasNestedScrollingParent()

        nestedWebView.isNestedScrollingEnabled = true
        verify(mockChildHelper).isNestedScrollingEnabled = true

        assertTrue(nestedWebView.isNestedScrollingEnabled)
        verify(mockChildHelper).isNestedScrollingEnabled

        nestedWebView.startNestedScroll(1)
        verify(mockChildHelper).startNestedScroll(1)

        nestedWebView.stopNestedScroll()
        verify(mockChildHelper).stopNestedScroll()

        assertTrue(nestedWebView.hasNestedScrollingParent())
        verify(mockChildHelper).hasNestedScrollingParent()

        nestedWebView.dispatchNestedScroll(0, 0, 0, 0, null)
        verify(mockChildHelper).dispatchNestedScroll(0, 0, 0, 0, null)

        nestedWebView.dispatchNestedPreScroll(0, 0, null, null)
        verify(mockChildHelper).dispatchNestedPreScroll(0, 0, null, null)

        nestedWebView.dispatchNestedFling(0f, 0f, true)
        verify(mockChildHelper).dispatchNestedFling(0f, 0f, true)

        nestedWebView.dispatchNestedPreFling(0f, 0f)
        verify(mockChildHelper).dispatchNestedPreFling(0f, 0f)
    }

    @Test
    fun `verify onTouchEvent when ACTION_DOWN`() {
        val nestedWebView = spy(NestedGeckoView(context))
        val mockChildHelper: NestedScrollingChildHelper = mock()
        val downEvent = mockMotionEvent(ACTION_DOWN)
        val eventCaptor = argumentCaptor<MotionEvent>()
        nestedWebView.childHelper = mockChildHelper

        nestedWebView.onTouchEvent(downEvent)
        shadowOf(getMainLooper()).idle()

        // We pass a deep copy to `updateInputResult`.
        // Can't easily check for equality, `eventTime` should be good enough.
        verify(nestedWebView).updateInputResult(eventCaptor.capture())
        assertEquals(downEvent.eventTime, eventCaptor.value.eventTime)
        verify(mockChildHelper).startNestedScroll(SCROLL_AXIS_VERTICAL)
        verify(nestedWebView, times(0)).callSuperOnTouchEvent(any())
    }

    @Test
    fun `verify onTouchEvent when ACTION_MOVE`() {
        val nestedWebView = spy(NestedGeckoView(context))
        val mockChildHelper: NestedScrollingChildHelper = mock()
        nestedWebView.childHelper = mockChildHelper
        nestedWebView.inputResultDetail = nestedWebView.inputResultDetail.copy(INPUT_RESULT_HANDLED)
        doReturn(true).`when`(nestedWebView).callSuperOnTouchEvent(any())

        doReturn(true).`when`(mockChildHelper).dispatchNestedPreScroll(
            anyInt(),
            anyInt(),
            any(),
            any(),
        )

        nestedWebView.scrollOffset[0] = 1
        nestedWebView.scrollOffset[1] = 2

        nestedWebView.onTouchEvent(mockMotionEvent(ACTION_MOVE, y = 10f))
        assertEquals(nestedWebView.nestedOffsetY, 2)
        assertEquals(nestedWebView.lastY, 8)

        doReturn(true).`when`(mockChildHelper).dispatchNestedScroll(
            anyInt(),
            anyInt(),
            anyInt(),
            anyInt(),
            any(),
        )

        nestedWebView.onTouchEvent(mockMotionEvent(ACTION_MOVE, y = 10f))
        assertEquals(nestedWebView.nestedOffsetY, 6)
        assertEquals(nestedWebView.lastY, 6)

        // onTouchEventForResult should be also called for ACTION_MOVE
        verify(nestedWebView, times(1)).updateInputResult(any())
    }

    @Test
    fun `verify onTouchEvent when ACTION_UP or ACTION_CANCEL`() {
        val nestedWebView = spy(NestedGeckoView(context))
        val initialInputResultDetail = nestedWebView.inputResultDetail.copy(INPUT_RESULT_HANDLED)
        nestedWebView.inputResultDetail = initialInputResultDetail
        val mockChildHelper: NestedScrollingChildHelper = mock()
        nestedWebView.childHelper = mockChildHelper
        doReturn(true).`when`(nestedWebView).callSuperOnTouchEvent(any())

        assertEquals(INPUT_RESULT_HANDLED, nestedWebView.inputResultDetail.inputResult)
        nestedWebView.onTouchEvent(mockMotionEvent(ACTION_UP))
        verify(mockChildHelper).stopNestedScroll()
        // ACTION_UP should reset nestedWebView.inputResultDetail.
        assertNotEquals(initialInputResultDetail, nestedWebView.inputResultDetail)
        assertEquals(INPUT_HANDLING_UNKNOWN, nestedWebView.inputResultDetail.inputResult)

        nestedWebView.inputResultDetail = initialInputResultDetail
        assertEquals(INPUT_RESULT_HANDLED, nestedWebView.inputResultDetail.inputResult)
        nestedWebView.onTouchEvent(mockMotionEvent(ACTION_CANCEL))
        verify(mockChildHelper, times(2)).stopNestedScroll()
        // ACTION_CANCEL should reset nestedWebView.inputResultDetail.
        assertNotEquals(initialInputResultDetail, nestedWebView.inputResultDetail)
        assertEquals(INPUT_HANDLING_UNKNOWN, nestedWebView.inputResultDetail.inputResult)

        // onTouchEventForResult should never be called for ACTION_UP or ACTION_CANCEL
        verify(nestedWebView, times(0)).updateInputResult(any())
    }

    @Test
    fun `requestDisallowInterceptTouchEvent doesn't pass touch events to parents until engineView responds`() {
        var viewParentInterceptCounter = 0
        val result: GeckoResult<PanZoomController.InputResultDetail> = GeckoResult()
        val nestedWebView = object : NestedGeckoView(context) {
            init {
                // We need to make the view a non-zero size so that the touch events hit it.
                left = 0
                top = 0
                right = 5
                bottom = 5
            }

            override fun onTouchEventForDetailResult(event: MotionEvent) = result
        }
        val viewParent = object : FrameLayout(context) {
            override fun onInterceptTouchEvent(ev: MotionEvent?): Boolean {
                viewParentInterceptCounter++
                return super.onInterceptTouchEvent(ev)
            }
        }.apply {
            addView(nestedWebView)
        }

        // Down action enables requestDisallowInterceptTouchEvent (and starts a gesture).
        viewParent.dispatchTouchEvent(mockMotionEvent(ACTION_DOWN))

        // `onInterceptTouchEvent` will be triggered the first time because it's the first pass.
        assertEquals(1, viewParentInterceptCounter)

        // Move action assert that onInterceptTouchEvent calls continue to be ignored.
        viewParent.dispatchTouchEvent(mockMotionEvent(ACTION_MOVE))

        assertEquals(1, viewParentInterceptCounter)

        // Simulate a response from the APZ GeckoEngineView API.
        result.complete(mock())
        shadowOf(getMainLooper()).idle()

        // Move action no longer ignores onInterceptTouchEvent calls.
        viewParent.dispatchTouchEvent(mockMotionEvent(ACTION_MOVE))

        assertEquals(2, viewParentInterceptCounter)

        // Complete the gesture by finishing with an up action.
        viewParent.dispatchTouchEvent(mockMotionEvent(ACTION_UP))

        assertEquals(3, viewParentInterceptCounter)
    }

    @Test
    fun `touch events are never intercepted once after scrolled down`() {
        var viewParentInterceptCounter = 0
        val result: GeckoResult<PanZoomController.InputResultDetail> = GeckoResult()
        val nestedWebView = object : NestedGeckoView(context) {
            init {
                // We need to make the view a non-zero size so that the touch events hit it.
                left = 0
                top = 0
                right = 5
                bottom = 5
            }
        }
        val spy = spy(nestedWebView)
        doReturn(result).`when`(spy).onTouchEventForDetailResult(any())

        val viewParent = object : FrameLayout(context) {
            override fun onInterceptTouchEvent(ev: MotionEvent?): Boolean {
                viewParentInterceptCounter++
                return super.onInterceptTouchEvent(ev)
            }
        }.apply {
            addView(spy)
        }

        // Down action enables requestDisallowInterceptTouchEvent (and starts a gesture).
        viewParent.dispatchTouchEvent(mockMotionEvent(ACTION_DOWN, y = 4f))

        // `onInterceptTouchEvent` will be triggered the first time because it's the first pass.
        assertEquals(1, viewParentInterceptCounter)

        // Move action to scroll down assert that onInterceptTouchEvent calls continue to be ignored.
        viewParent.dispatchTouchEvent(mockMotionEvent(ACTION_MOVE, y = 3f))

        assertEquals(1, viewParentInterceptCounter)

        // Simulate a `handled` response from the APZ GeckoEngineView API.
        val inputResultMock = mock<PanZoomController.InputResultDetail>().apply {
            whenever(handledResult()).thenReturn(INPUT_RESULT_HANDLED)
            whenever(scrollableDirections()).thenReturn(SCROLLABLE_FLAG_BOTTOM)
            whenever(overscrollDirections()).thenReturn(OVERSCROLL_FLAG_VERTICAL or OVERSCROLL_FLAG_HORIZONTAL)
        }
        result.complete(inputResultMock)
        shadowOf(getMainLooper()).idle()

        // Move action to scroll down further that onInterceptTouchEvent calls continue to be ignored.
        viewParent.dispatchTouchEvent(mockMotionEvent(ACTION_MOVE, y = 2f))

        assertEquals(1, viewParentInterceptCounter)

        // Complete the gesture by finishing with an up action.
        viewParent.dispatchTouchEvent(mockMotionEvent(ACTION_UP))

        assertEquals(1, viewParentInterceptCounter)
    }

    @Test
    fun `touch events are intercepted once after initially scrolled up`() {
        var viewParentInterceptCounter = 0
        val result: GeckoResult<PanZoomController.InputResultDetail> = GeckoResult()
        val nestedWebView = object : NestedGeckoView(context) {
            init {
                // We need to make the view a non-zero size so that the touch events hit it.
                left = 0
                top = 0
                right = 5
                bottom = 5
            }
        }
        val spy = spy(nestedWebView)
        doReturn(result).`when`(spy).onTouchEventForDetailResult(any())

        val viewParent = object : FrameLayout(context) {
            override fun onInterceptTouchEvent(ev: MotionEvent?): Boolean {
                viewParentInterceptCounter++
                return super.onInterceptTouchEvent(ev)
            }
        }.apply {
            addView(spy)
        }

        // Down action enables requestDisallowInterceptTouchEvent (and starts a gesture).
        viewParent.dispatchTouchEvent(mockMotionEvent(ACTION_DOWN, y = 1f))

        // `onInterceptTouchEvent` will be triggered the first time because it's the first pass.
        assertEquals(1, viewParentInterceptCounter)

        // Move action to scroll up assert that onInterceptTouchEvent calls are no longer ignored.
        viewParent.dispatchTouchEvent(mockMotionEvent(ACTION_MOVE, y = 2f))

        assertEquals(1, viewParentInterceptCounter)

        // Simulate a `handled` response from the APZ GeckoEngineView API.
        val inputResultMock = mock<PanZoomController.InputResultDetail>().apply {
            whenever(handledResult()).thenReturn(INPUT_RESULT_HANDLED)
            whenever(scrollableDirections()).thenReturn(SCROLLABLE_FLAG_BOTTOM)
            whenever(overscrollDirections()).thenReturn(OVERSCROLL_FLAG_VERTICAL or OVERSCROLL_FLAG_HORIZONTAL)
        }
        result.complete(inputResultMock)
        shadowOf(getMainLooper()).idle()

        // Move action to scroll up further assert that onInterceptTouchEvent calls are not ignored.
        viewParent.dispatchTouchEvent(mockMotionEvent(ACTION_MOVE, y = 3f))

        assertEquals(2, viewParentInterceptCounter)

        // Complete the gesture by finishing with an up action.
        viewParent.dispatchTouchEvent(mockMotionEvent(ACTION_UP))

        assertEquals(3, viewParentInterceptCounter)
    }
}
