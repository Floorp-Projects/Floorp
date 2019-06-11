/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

package mozilla.components.browser.engine.system

import android.view.MotionEvent.ACTION_CANCEL
import android.view.MotionEvent.ACTION_DOWN
import android.view.MotionEvent.ACTION_MOVE
import android.view.MotionEvent.ACTION_UP
import androidx.core.view.NestedScrollingChildHelper
import androidx.core.view.ViewCompat.SCROLL_AXIS_VERTICAL
import androidx.test.ext.junit.runners.AndroidJUnit4
import mozilla.components.support.test.mock
import mozilla.components.support.test.mockMotionEvent
import mozilla.components.support.test.robolectric.testContext
import org.junit.Assert.assertEquals
import org.junit.Assert.assertTrue
import org.junit.Test
import org.junit.runner.RunWith
import org.mockito.Mockito.any
import org.mockito.Mockito.anyInt
import org.mockito.Mockito.doReturn
import org.mockito.Mockito.times
import org.mockito.Mockito.verify

@RunWith(AndroidJUnit4::class)
class NestedWebViewTest {

    @Test
    fun `NestedWebView must delegate NestedScrollingChild implementation to childHelper`() {
        val nestedWebView = NestedWebView(testContext)
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
        val nestedWebView = NestedWebView(testContext)
        val mockChildHelper: NestedScrollingChildHelper = mock()
        nestedWebView.childHelper = mockChildHelper

        nestedWebView.onTouchEvent(mockMotionEvent(ACTION_DOWN))
        verify(mockChildHelper).startNestedScroll(SCROLL_AXIS_VERTICAL)
    }

    @Test
    fun `verify onTouchEvent when ACTION_MOVE`() {
        val nestedWebView = NestedWebView(testContext)
        val mockChildHelper: NestedScrollingChildHelper = mock()
        nestedWebView.childHelper = mockChildHelper

        doReturn(true).`when`(mockChildHelper).dispatchNestedPreScroll(
            anyInt(), anyInt(), any(),
            any()
        )

        nestedWebView.scrollOffset[0] = 1
        nestedWebView.scrollOffset[1] = 2

        nestedWebView.onTouchEvent(mockMotionEvent(ACTION_MOVE, y = 10f))
        assertEquals(nestedWebView.nestedOffsetY, 2)
        assertEquals(nestedWebView.lastY, 8)

        doReturn(true).`when`(mockChildHelper).dispatchNestedScroll(
            anyInt(), anyInt(), anyInt(), anyInt(), any()
        )

        nestedWebView.onTouchEvent(mockMotionEvent(ACTION_MOVE, y = 10f))
        assertEquals(nestedWebView.nestedOffsetY, 6)
        assertEquals(nestedWebView.lastY, 6)
    }

    @Test
    fun `verify onTouchEvent when ACTION_UP or ACTION_CANCEL`() {
        val nestedWebView = NestedWebView(testContext)
        val mockChildHelper: NestedScrollingChildHelper = mock()
        nestedWebView.childHelper = mockChildHelper

        nestedWebView.onTouchEvent(mockMotionEvent(ACTION_UP))
        verify(mockChildHelper).stopNestedScroll()

        nestedWebView.onTouchEvent(mockMotionEvent(ACTION_CANCEL))
        verify(mockChildHelper, times(2)).stopNestedScroll()
    }
}