/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

package mozilla.components.browser.menu2.ext

import android.view.Gravity
import android.view.View
import android.widget.PopupWindow
import androidx.core.view.ViewCompat
import androidx.test.ext.junit.runners.AndroidJUnit4
import mozilla.components.browser.menu2.R
import mozilla.components.concept.menu.Orientation
import mozilla.components.support.test.any
import mozilla.components.support.test.robolectric.testContext
import org.junit.Assert.assertEquals
import org.junit.Before
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

    private lateinit var menuContentView: View
    private lateinit var anchor: View
    private lateinit var popupWindow: PopupWindow

    @Before
    fun setUp() {
        menuContentView = createMockViewWith(x = 0, y = 0, false)
        doReturn(90).`when`(menuContentView).measuredHeight
        doReturn(30).`when`(menuContentView).measuredWidth

        popupWindow = spy(PopupWindow())

        // Makes the availableHeightToBottom bigger than the menuContentView
        setScreenHeightAndWidth()
    }

    @Test
    fun `WHEN displaying prefer down popup from top left THEN show popup down and to the right`() {
        anchor = createMockViewWith(x = 0, y = 0, false)
        popupWindow.displayPopup(menuContentView, anchor, Orientation.DOWN)

        assertEquals(R.style.Mozac_Browser_Menu2_Animation_OverflowMenuLeftTop, popupWindow.animationStyle)
        verify(popupWindow).showAsDropDown(anchor, 0, 0)
    }

    @Test
    fun `WHEN displaying prefer down popup from top right THEN show popup down and to the left`() {
        anchor = createMockViewWith(x = 90, y = 0, false)
        popupWindow.displayPopup(menuContentView, anchor, Orientation.DOWN)

        assertEquals(R.style.Mozac_Browser_Menu2_Animation_OverflowMenuRightTop, popupWindow.animationStyle)
        verify(popupWindow).showAsDropDown(anchor, 0, 0)
    }

    @Test
    fun `WHEN display down popup on right to left device THEN show on the correct side of the anchor`() {
        anchor = createMockViewWith(x = 90, y = 0, true)
        popupWindow.displayPopup(menuContentView, anchor, Orientation.DOWN)

        assertEquals(R.style.Mozac_Browser_Menu2_Animation_OverflowMenuRightTop, popupWindow.animationStyle)
        verify(popupWindow).showAsDropDown(anchor, 0, 0)
    }

    @Test
    fun `WHEN displaying prefer up popup from top right THEN show popup down and to the left`() {
        anchor = createMockViewWith(x = 90, y = 0, false)
        popupWindow.displayPopup(menuContentView, anchor, Orientation.UP)

        assertEquals(R.style.Mozac_Browser_Menu2_Animation_OverflowMenuRightTop, popupWindow.animationStyle)
        verify(popupWindow).showAsDropDown(anchor, 0, 0)
    }

    @Test
    fun `WHEN displaying prefer up popup from bottom right THEN show popup up and to the left`() {
        anchor = createMockViewWith(x = 90, y = 190, false)
        popupWindow.displayPopup(menuContentView, anchor, Orientation.UP)

        assertEquals(R.style.Mozac_Browser_Menu2_Animation_OverflowMenuRightBottom, popupWindow.animationStyle)
        verify(popupWindow).showAsDropDown(anchor, 0, -90)
    }

    @Test
    fun `WHEN displaying prefer up popup from bottom left THEN show popup up and to the left`() {
        anchor = createMockViewWith(x = 0, y = 190, false)
        popupWindow.displayPopup(menuContentView, anchor, Orientation.UP)

        assertEquals(R.style.Mozac_Browser_Menu2_Animation_OverflowMenuLeftBottom, popupWindow.animationStyle)
        verify(popupWindow).showAsDropDown(anchor, 0, -90)
    }

    @Test
    fun `WHEN display up popup on right to left device THEN show on the correct side of the anchor`() {
        anchor = createMockViewWith(x = 0, y = 190, true)
        popupWindow.displayPopup(menuContentView, anchor, Orientation.UP)

        assertEquals(R.style.Mozac_Browser_Menu2_Animation_OverflowMenuLeftBottom, popupWindow.animationStyle)
        verify(popupWindow).showAsDropDown(anchor, 0, -90)
    }

    @Test
    fun `WHEN displaying prefer down popup from bottom left THEN show popup up and to the left`() {
        anchor = createMockViewWith(x = 0, y = 190, false)
        popupWindow.displayPopup(menuContentView, anchor, Orientation.DOWN)

        assertEquals(R.style.Mozac_Browser_Menu2_Animation_OverflowMenuLeftBottom, popupWindow.animationStyle)
        verify(popupWindow).showAsDropDown(anchor, 0, -90)
    }

    @Test
    fun `WHEN displaying popup from below screen bottom right THEN show popup up`() {
        anchor = createMockViewWith(x = 110, y = 210, false)
        popupWindow.displayPopup(menuContentView, anchor, Orientation.UP)

        assertEquals(R.style.Mozac_Browser_Menu2_Animation_OverflowMenuRightBottom, popupWindow.animationStyle)
        verify(popupWindow).showAsDropDown(anchor, 0, -90)
    }

    @Test
    fun `WHEN displaying popup that doesn't fit up or down THEN show popup up from anchor`() {
        anchor = createMockViewWith(x = 0, y = 10, false)
        doReturn(Int.MAX_VALUE).`when`(menuContentView).measuredHeight

        popupWindow.displayPopup(menuContentView, anchor, Orientation.UP)
        assertEquals(R.style.Mozac_Browser_Menu2_Animation_OverflowMenuLeft, popupWindow.animationStyle)
        verify(popupWindow).showAtLocation(anchor, Gravity.START or Gravity.TOP, 0, 10)
    }

    @Test
    fun `WHEN displaying force up popup from bottom left THEN show popup up and to the left`() {
        anchor = createMockViewWith(x = 0, y = 190, false)
        doReturn(300).`when`(menuContentView).measuredHeight

        popupWindow.displayPopup(menuContentView, anchor, Orientation.UP, true)
        assertEquals(R.style.Mozac_Browser_Menu2_Animation_OverflowMenuLeftBottom, popupWindow.animationStyle)
        verify(popupWindow).showAsDropDown(anchor, 0, -300)
    }

    @Test
    fun `WHEN displaying force down popup from top left THEN show popup down and to the right`() {
        anchor = createMockViewWith(x = 0, y = 0, false)
        doReturn(300).`when`(menuContentView).measuredHeight

        popupWindow.displayPopup(menuContentView, anchor, Orientation.DOWN, true)
        assertEquals(R.style.Mozac_Browser_Menu2_Animation_OverflowMenuLeftTop, popupWindow.animationStyle)
        verify(popupWindow).showAsDropDown(anchor, 0, 0)
    }

    private fun createMockViewWith(x: Int, y: Int, isRTL: Boolean): View {
        val view = spy(View(testContext))
        doAnswer { invocation ->
            val locationInWindow = (invocation.getArgument(0) as IntArray)
            locationInWindow[0] = x
            locationInWindow[1] = y
            locationInWindow
        }.`when`(view).getLocationOnScreen(any())

        doReturn(10).`when`(view).height
        doReturn(10).`when`(view).width
        if (isRTL) {
            doReturn(ViewCompat.LAYOUT_DIRECTION_RTL).`when`(view).layoutDirection
        } else {
            doReturn(ViewCompat.LAYOUT_DIRECTION_LTR).`when`(view).layoutDirection
        }

        return view
    }

    private fun setScreenHeightAndWidth() {
        val display = ShadowDisplay.getDefaultDisplay()
        val shadow = Shadows.shadowOf(display)
        shadow.setHeight(200)
        shadow.setWidth(100)
    }
}
