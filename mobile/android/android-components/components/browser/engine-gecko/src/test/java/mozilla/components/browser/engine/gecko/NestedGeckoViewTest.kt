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
import androidx.core.view.NestedScrollingChildHelper
import androidx.core.view.ViewCompat.SCROLL_AXIS_VERTICAL
import androidx.test.ext.junit.runners.AndroidJUnit4
import mozilla.components.support.test.any
import mozilla.components.support.test.argumentCaptor
import mozilla.components.support.test.mock
import mozilla.components.support.test.mockMotionEvent
import org.junit.Assert.assertEquals
import org.junit.Assert.assertFalse
import org.junit.Assert.assertTrue
import org.junit.Test
import org.junit.runner.RunWith
import org.mockito.Mockito.anyInt
import org.mockito.Mockito.doReturn
import org.mockito.Mockito.spy
import org.mockito.Mockito.times
import org.mockito.Mockito.verify
import org.mozilla.geckoview.PanZoomController.INPUT_RESULT_HANDLED
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

        // onTouchEventForResult should be called only for ACTION_DOWN
        verify(nestedWebView, times(0)).updateInputResult(any())
    }

    @Test
    fun `verify onTouchEvent when ACTION_UP or ACTION_CANCEL`() {
        val nestedWebView = spy(NestedGeckoView(context))
        nestedWebView.inputResultDetail = nestedWebView.inputResultDetail.copy(INPUT_RESULT_HANDLED)
        val mockChildHelper: NestedScrollingChildHelper = mock()
        nestedWebView.childHelper = mockChildHelper
        doReturn(true).`when`(nestedWebView).callSuperOnTouchEvent(any())

        nestedWebView.onTouchEvent(mockMotionEvent(ACTION_UP))
        verify(mockChildHelper).stopNestedScroll()
        // ACTION_UP should call "inputResultDetail.reset()". Test that call's effect.
        assertTrue(nestedWebView.inputResultDetail.isTouchHandlingUnknown())
        assertFalse(nestedWebView.inputResultDetail.isTouchHandledByBrowser())

        nestedWebView.inputResultDetail = nestedWebView.inputResultDetail.copy(INPUT_RESULT_HANDLED)
        nestedWebView.onTouchEvent(mockMotionEvent(ACTION_CANCEL))
        verify(mockChildHelper, times(2)).stopNestedScroll()
        // ACTION_CANCEL should call "inputResultDetail.reset()". Test that call's effect.
        assertTrue(nestedWebView.inputResultDetail.isTouchHandlingUnknown())
        assertFalse(nestedWebView.inputResultDetail.isTouchHandledByBrowser())

        // onTouchEventForResult should be called only for ACTION_DOWN
        verify(nestedWebView, times(0)).updateInputResult(any())
    }
}
