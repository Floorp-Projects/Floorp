/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

package mozilla.components.browser.menu

import android.os.Build
import android.view.Gravity
import android.view.View
import android.widget.Button
import android.widget.PopupWindow
import androidx.coordinatorlayout.widget.CoordinatorLayout
import androidx.recyclerview.widget.LinearLayoutManager
import androidx.recyclerview.widget.RecyclerView
import androidx.test.ext.junit.runners.AndroidJUnit4
import mozilla.components.browser.menu.item.SimpleBrowserMenuItem
import mozilla.components.support.test.any
import mozilla.components.support.test.mock
import mozilla.components.support.test.robolectric.testContext
import org.junit.Assert.assertEquals
import org.junit.Assert.assertFalse
import org.junit.Assert.assertNotNull
import org.junit.Assert.assertTrue
import org.junit.Test
import org.junit.runner.RunWith
import org.mockito.Mockito
import org.mockito.Mockito.doAnswer
import org.mockito.Mockito.doNothing
import org.mockito.Mockito.doReturn
import org.mockito.Mockito.spy
import org.mockito.Mockito.verify
import org.robolectric.Shadows
import org.robolectric.annotation.Config
import org.robolectric.shadows.ShadowDisplay

@RunWith(AndroidJUnit4::class)
class BrowserMenuTest {

    @Test
    fun `show returns non-null popup window`() {
        val items = listOf(
            SimpleBrowserMenuItem("Hello") {},
            SimpleBrowserMenuItem("World") {})

        val adapter = BrowserMenuAdapter(testContext, items)

        val menu = BrowserMenu(adapter)

        val anchor = Button(testContext)
        val popup = menu.show(anchor)

        assertNotNull(popup)
    }

    @Test
    fun `recyclerview adapter will have items for every menu item`() {
        val items = listOf(
            SimpleBrowserMenuItem("Hello") {},
            SimpleBrowserMenuItem("World") {})

        val adapter = BrowserMenuAdapter(testContext, items)

        val menu = BrowserMenu(adapter)

        val anchor = Button(testContext)
        val popup = menu.show(anchor)

        val recyclerView: RecyclerView = popup.contentView.findViewById(R.id.mozac_browser_menu_recyclerView)
        assertNotNull(recyclerView)

        val recyclerAdapter = recyclerView.adapter!!
        assertNotNull(recyclerAdapter)
        assertEquals(2, recyclerAdapter.itemCount)
    }

    @Test
    fun `endOfMenuAlwaysVisible will be forwarded to recyclerview layoutManager`() {
        val items = listOf(
            SimpleBrowserMenuItem("Hello") {},
            SimpleBrowserMenuItem("World") {})

        val adapter = spy(BrowserMenuAdapter(testContext, items))
        val menu = BrowserMenu(adapter)

        val anchor = Button(testContext)
        val popup = menu.show(anchor, endOfMenuAlwaysVisible = true)

        val recyclerView: RecyclerView = popup.contentView.findViewById(R.id.mozac_browser_menu_recyclerView)
        assertNotNull(recyclerView)

        val layoutManager = recyclerView.layoutManager as LinearLayoutManager
        assertTrue(layoutManager.stackFromEnd)
    }

    @Test
    @Config(sdk = [Build.VERSION_CODES.M])
    fun `endOfMenuAlwaysVisible will be forwarded to scrollOnceToTheBottom on devices with Android M and below`() {
        val items = listOf(
            SimpleBrowserMenuItem("Hello") {},
            SimpleBrowserMenuItem("World") {})

        val adapter = BrowserMenuAdapter(testContext, items)
        val menu = spy(BrowserMenu(adapter))
        doNothing().`when`(menu).scrollOnceToTheBottom(any())

        val anchor = Button(testContext)
        val popup = menu.show(anchor, endOfMenuAlwaysVisible = true)

        val recyclerView: RecyclerView = popup.contentView.findViewById(R.id.mozac_browser_menu_recyclerView)
        val layoutManager = recyclerView.layoutManager as LinearLayoutManager

        assertFalse(layoutManager.stackFromEnd)
        verify(menu).scrollOnceToTheBottom(any())
    }

    @Test
    fun `invalidate will be forwarded to recyclerview adapter`() {
        val items = listOf(
            SimpleBrowserMenuItem("Hello") {},
            SimpleBrowserMenuItem("World") {})

        val adapter = spy(BrowserMenuAdapter(testContext, items))

        val menu = BrowserMenu(adapter)

        val anchor = Button(testContext)
        val popup = menu.show(anchor)

        val recyclerView: RecyclerView = popup.contentView.findViewById(R.id.mozac_browser_menu_recyclerView)
        assertNotNull(recyclerView)
        assertNotNull(recyclerView.adapter)

        menu.invalidate()
        Mockito.verify(adapter).invalidate(recyclerView)
    }

    @Test
    fun `invalidate is a no-op if the menu is closed`() {
        val items = listOf(SimpleBrowserMenuItem("Hello") {})
        val menu = BrowserMenu(BrowserMenuAdapter(testContext, items))

        menu.invalidate()
    }

    @Test
    fun `created popup window is displayed automatically`() {
        val items = listOf(
            SimpleBrowserMenuItem("Hello") {},
            SimpleBrowserMenuItem("World") {})

        val adapter = BrowserMenuAdapter(testContext, items)

        val menu = BrowserMenu(adapter)

        val anchor = Button(testContext)
        val popup = menu.show(anchor)

        assertTrue(popup.isShowing)
    }

    @Test
    fun `dismissing the browser menu will dismiss the popup`() {
        val items = listOf(
            SimpleBrowserMenuItem("Hello") {},
            SimpleBrowserMenuItem("World") {})

        val adapter = BrowserMenuAdapter(testContext, items)

        val menu = BrowserMenu(adapter)

        val anchor = Button(testContext)
        val popup = menu.show(anchor)

        assertTrue(popup.isShowing)

        menu.dismiss()

        assertFalse(popup.isShowing)
    }

    @Test
    fun `determineMenuOrientation returns Orientation-DOWN by default`() {
        assertEquals(
            BrowserMenu.Orientation.DOWN,
            BrowserMenu.determineMenuOrientation(mock())
        )
    }

    @Test
    fun `determineMenuOrientation returns Orientation-UP for views with bottom gravity in CoordinatorLayout`() {
        val params = CoordinatorLayout.LayoutParams(100, 100)
        params.gravity = Gravity.BOTTOM

        val view = View(testContext)
        view.layoutParams = params

        assertEquals(
            BrowserMenu.Orientation.UP,
            BrowserMenu.determineMenuOrientation(view)
        )
    }

    @Test
    fun `determineMenuOrientation returns Orientation-DOWN for views with top gravity in CoordinatorLayout`() {
        val params = CoordinatorLayout.LayoutParams(100, 100)
        params.gravity = Gravity.TOP

        val view = View(testContext)
        view.layoutParams = params

        assertEquals(
            BrowserMenu.Orientation.DOWN,
            BrowserMenu.determineMenuOrientation(view)
        )
    }

    @Test
    fun `displayPopup that fitsDown with preferredOrientation DOWN`() {
        val menuContentView = createMockViewWith(y = 0)
        val anchor = createMockViewWith(y = 10)
        val popupWindow = spy(PopupWindow())

        // Makes the availableHeightToBottom bigger than the menuContentView
        setScreenHeight(200)
        doReturn(11).`when`(menuContentView).measuredHeight
        doReturn(-10).`when`(anchor).height

        popupWindow.displayPopup(menuContentView, anchor, BrowserMenu.Orientation.DOWN)

        assertEquals(popupWindow.animationStyle, R.style.Mozac_Browser_Menu_Animation_OverflowMenuTop)
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

        popupWindow.displayPopup(menuContentView, anchor, BrowserMenu.Orientation.UP)

        assertEquals(popupWindow.animationStyle, R.style.Mozac_Browser_Menu_Animation_OverflowMenuTop)
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

        popupWindow.displayPopup(containerView, anchor, BrowserMenu.Orientation.UP)

        assertEquals(popupWindow.animationStyle, R.style.Mozac_Browser_Menu_Animation_OverflowMenuBottom)
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

        popupWindow.displayPopup(containerView, anchor, BrowserMenu.Orientation.DOWN)

        assertEquals(popupWindow.animationStyle, R.style.Mozac_Browser_Menu_Animation_OverflowMenuBottom)
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

        popupWindow.displayPopup(containerView, anchor, BrowserMenu.Orientation.UP)

        assertEquals(popupWindow.animationStyle, R.style.Mozac_Browser_Menu_Animation_OverflowMenuBottom)
        verify(popupWindow).showAsDropDown(anchor, 0, screenHeight - contentHeight)
    }

    @Test
    fun `displayPopup that don't fitUp neither fitDown`() {
        val containerView = createMockViewWith(y = 0)
        val anchor = createMockViewWith(y = 0)
        val popupWindow = spy(PopupWindow())
        doReturn(Int.MAX_VALUE).`when`(containerView).measuredHeight

        popupWindow.displayPopup(containerView, anchor, BrowserMenu.Orientation.DOWN)
        assertEquals(popupWindow.animationStyle, -1)
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
