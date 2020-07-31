/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

package mozilla.components.browser.menu2.ext

import android.view.Gravity
import android.view.View
import android.widget.PopupWindow
import androidx.test.ext.junit.runners.AndroidJUnit4
import mozilla.components.browser.menu2.R
import mozilla.components.concept.menu.Orientation
import mozilla.components.support.test.any
import mozilla.components.support.test.robolectric.testContext
import org.junit.Assert.assertEquals
import org.junit.Test
import org.junit.runner.RunWith
import org.mockito.Mockito.doAnswer
import org.mockito.Mockito.doReturn
import org.mockito.Mockito.spy
import org.mockito.Mockito.verify
import org.robolectric.Shadows
import org.robolectric.shadows.ShadowDisplay

@RunWith(AndroidJUnit4::class)
class PopupWindowTest {

    @Test
    fun `displayPopup that fitsDown with preferredOrientation DOWN`() {
        val menuContentView = createMockViewWith(y = 0)
        val anchor = createMockViewWith(y = 10)
        val popupWindow = spy(PopupWindow())

        // Makes the availableHeightToBottom bigger than the menuContentView
        setScreenHeight(200)
        doReturn(11).`when`(menuContentView).measuredHeight
        doReturn(-10).`when`(anchor).height

        popupWindow.displayPopup(menuContentView, anchor, Orientation.DOWN)

        assertEquals(R.style.Mozac_Browser_Menu2_Animation_OverflowMenuTop, popupWindow.animationStyle)
        verify(popupWindow).showAsDropDown(anchor, 0, 10)
    }

    @Test
    fun `displayPopup that fitsDown with preferredOrientation UP`() {
        val menuContentView = createMockViewWith(y = 0)
        val anchor = createMockViewWith(y = 10)
        val popupWindow = spy(PopupWindow())

        // Makes the availableHeightToBottom bigger than the menuContentView
        setScreenHeight(200)
        doReturn(11).`when`(menuContentView).measuredHeight
        doReturn(-10).`when`(anchor).height

        popupWindow.displayPopup(menuContentView, anchor, Orientation.UP)

        assertEquals(R.style.Mozac_Browser_Menu2_Animation_OverflowMenuTop, popupWindow.animationStyle)
        verify(popupWindow).showAsDropDown(anchor, 0, 10)
    }

    @Test
    fun `displayPopup that fitsUp with preferredOrientation UP`() {
        val containerView = createMockViewWith(y = 0)
        // Makes the availableHeightToTop 10
        val anchor = createMockViewWith(y = 10)
        val popupWindow = spy(PopupWindow())

        // Makes the availableHeightToBottom smaller than the availableHeightToTop
        setScreenHeight(0)
        doReturn(-10).`when`(anchor).height

        // Makes the content of the menu smaller than the availableHeightToTop
        doReturn(9).`when`(containerView).measuredHeight

        popupWindow.displayPopup(containerView, anchor, Orientation.UP)

        assertEquals(R.style.Mozac_Browser_Menu2_Animation_OverflowMenuBottom, popupWindow.animationStyle)
        verify(popupWindow).showAsDropDown(anchor, 0, -9)
    }

    @Test
    fun `displayPopup that fitsUp with preferredOrientation DOWN`() {
        val containerView = createMockViewWith(y = 0)
        // Makes the availableHeightToTop 10
        val anchor = createMockViewWith(y = 10)
        val popupWindow = spy(PopupWindow())
        val contentHeight = 9

        // Makes the availableHeightToBottom smaller than the availableHeightToTop
        setScreenHeight(0)
        doReturn(-10).`when`(anchor).height

        // Makes the content of the menu smaller than the availableHeightToTop
        doReturn(contentHeight).`when`(containerView).measuredHeight

        popupWindow.displayPopup(containerView, anchor, Orientation.DOWN)

        assertEquals(R.style.Mozac_Browser_Menu2_Animation_OverflowMenuBottom, popupWindow.animationStyle)
        verify(popupWindow).showAsDropDown(anchor, 0, -contentHeight)
    }

    @Test
    fun `displayPopup that fitsUp when anchor is partially below of the bottom of the screen`() {
        val containerView = createMockViewWith(y = 0)
        // Makes the availableHeightToTop 10
        val anchor = createMockViewWith(y = 10)
        val popupWindow = spy(PopupWindow())
        val screenHeight = -1
        val contentHeight = -9

        // Makes the availableHeightToBottom smaller than the availableHeightToTop
        setScreenHeight(screenHeight)
        doReturn(-10).`when`(anchor).height

        // Makes the content of the menu smaller than the availableHeightToTop
        doReturn(contentHeight).`when`(containerView).measuredHeight

        popupWindow.displayPopup(containerView, anchor, Orientation.UP)

        assertEquals(R.style.Mozac_Browser_Menu2_Animation_OverflowMenuBottom, popupWindow.animationStyle)
        verify(popupWindow).showAsDropDown(anchor, 0, screenHeight - contentHeight)
    }

    @Test
    fun `displayPopup that don't fitUp neither fitDown`() {
        val containerView = createMockViewWith(y = 0)
        val anchor = createMockViewWith(y = 0)
        val popupWindow = spy(PopupWindow())
        doReturn(Int.MAX_VALUE).`when`(containerView).measuredHeight

        popupWindow.displayPopup(containerView, anchor, Orientation.DOWN)
        assertEquals(R.style.Mozac_Browser_Menu2_Animation_OverflowMenuTop, popupWindow.animationStyle)
        verify(popupWindow).showAtLocation(anchor, Gravity.START or Gravity.TOP, 0, 0)
    }

    private fun createMockViewWith(y: Int): View {
        val view = spy(View(testContext))
        doAnswer { invocation ->
            val locationInWindow = (invocation.getArgument(0) as IntArray)
            locationInWindow[0] = 0
            locationInWindow[1] = y
            locationInWindow
        }.`when`(view).getLocationInWindow(any())
        return view
    }

    private fun setScreenHeight(value: Int) {
        val display = ShadowDisplay.getDefaultDisplay()
        val shadow = Shadows.shadowOf(display)
        shadow.setHeight(value)
    }
}
