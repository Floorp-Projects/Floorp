/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

package mozilla.components.browser.engine.gecko

import android.support.v4.view.NestedScrollingChildHelper
import android.support.v4.view.ViewCompat.SCROLL_AXIS_VERTICAL
import android.view.MotionEvent
import android.view.MotionEvent.ACTION_CANCEL
import android.view.MotionEvent.ACTION_DOWN
import android.view.MotionEvent.ACTION_UP
import mozilla.components.support.test.mock
import org.junit.Assert.assertEquals
import org.junit.Assert.assertTrue
import org.junit.Test
import org.junit.runner.RunWith
import org.mockito.Mockito.any
import org.mockito.Mockito.anyInt
import org.mockito.Mockito.doReturn
import org.mockito.Mockito.times
import org.mockito.Mockito.verify
import org.robolectric.RobolectricTestRunner
import org.robolectric.RuntimeEnvironment

@RunWith(RobolectricTestRunner::class)
class NestedGeckoViewTest {

    @Test
    fun `NestedGeckoView must delegate NestedScrollingChild implementation to childHelper`() {
        val nestedWebView = NestedGeckoView(RuntimeEnvironment.application)
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
        val nestedWebView = NestedGeckoView(RuntimeEnvironment.application)
        val mockMotionEvent: MotionEvent = mock()
        val mockChildHelper: NestedScrollingChildHelper = mock()
        nestedWebView.childHelper = mockChildHelper

        doReturn(ACTION_DOWN).`when`(mockMotionEvent).actionMasked

        nestedWebView.onTouchEvent(mockMotionEvent)
        verify(mockChildHelper).startNestedScroll(SCROLL_AXIS_VERTICAL)
    }

    @Test
    fun `verify onTouchEvent when ACTION_MOVE`() {
        val nestedWebView = NestedGeckoView(RuntimeEnvironment.application)
        val mockMotionEvent: MotionEvent = mock()
        val mockChildHelper: NestedScrollingChildHelper = mock()
        nestedWebView.childHelper = mockChildHelper

        doReturn(MotionEvent.ACTION_MOVE).`when`(mockMotionEvent).actionMasked

        doReturn(10f).`when`(mockMotionEvent).y

        doReturn(true).`when`(mockChildHelper).dispatchNestedPreScroll(
            anyInt(), anyInt(), any(),
            any()
        )

        nestedWebView.scrollOffset[0] = 1
        nestedWebView.scrollOffset[1] = 2

        nestedWebView.onTouchEvent(mockMotionEvent)
        assertEquals(nestedWebView.nestedOffsetY, 2)
        assertEquals(nestedWebView.lastY, 8)

        doReturn(true).`when`(mockChildHelper).dispatchNestedScroll(
            anyInt(), anyInt(), anyInt(), anyInt(), any()
        )

        nestedWebView.onTouchEvent(mockMotionEvent)
        assertEquals(nestedWebView.nestedOffsetY, 6)
        assertEquals(nestedWebView.lastY, 6)
    }

    @Test
    fun `verify onTouchEvent when ACTION_UP or ACTION_CANCEL`() {
        val nestedWebView = NestedGeckoView(RuntimeEnvironment.application)
        val mockMotionEvent: MotionEvent = mock()
        val mockChildHelper: NestedScrollingChildHelper = mock()
        nestedWebView.childHelper = mockChildHelper

        doReturn(ACTION_UP).`when`(mockMotionEvent).actionMasked

        nestedWebView.onTouchEvent(mockMotionEvent)
        verify(mockChildHelper).stopNestedScroll()

        doReturn(ACTION_CANCEL).`when`(mockMotionEvent).actionMasked

        nestedWebView.onTouchEvent(mockMotionEvent)
        verify(mockChildHelper, times(2)).stopNestedScroll()
    }
}