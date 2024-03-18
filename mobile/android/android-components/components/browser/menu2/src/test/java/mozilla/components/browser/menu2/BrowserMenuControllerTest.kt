/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

package mozilla.components.browser.menu2

import android.view.Gravity
import android.widget.Button
import android.widget.PopupWindow
import androidx.test.ext.junit.runners.AndroidJUnit4
import mozilla.components.browser.menu2.ext.MenuPositioningData
import mozilla.components.browser.menu2.ext.createAnchor
import mozilla.components.browser.menu2.ext.createContainerView
import mozilla.components.browser.menu2.ext.getTargetCoordinates
import mozilla.components.concept.menu.MenuController
import mozilla.components.concept.menu.candidate.DecorativeTextMenuCandidate
import mozilla.components.concept.menu.candidate.MenuCandidate
import mozilla.components.support.test.robolectric.testContext
import org.junit.Assert.assertEquals
import org.junit.Assert.assertFalse
import org.junit.Assert.assertNull
import org.junit.Assert.assertTrue
import org.junit.Test
import org.junit.runner.RunWith
import org.mockito.Mockito

@RunWith(AndroidJUnit4::class)
class BrowserMenuControllerTest {

    @Test
    fun `created popup window is displayed automatically`() {
        val menu: MenuController = BrowserMenuController()
        menu.submitList(listOf(DecorativeTextMenuCandidate("Hello")))

        val anchor = Button(testContext)
        val popup = menu.show(anchor)

        assertTrue(popup.isShowing)
    }

    @Test
    fun `WHEN an empty list is submitted to the menu THEN the menu isn't shown`() {
        val menu: MenuController = BrowserMenuController()
        menu.submitList(emptyList())

        val anchor = Button(testContext)
        val popup = menu.show(anchor)

        assertFalse(popup.isShowing)
    }

    @Test
    fun `observer is notified when submitList is called`() {
        var submitted: List<MenuCandidate>? = null
        val menu: MenuController = BrowserMenuController()
        menu.register(
            object : MenuController.Observer {
                override fun onMenuListSubmit(list: List<MenuCandidate>) {
                    submitted = list
                }

                override fun onDismiss() = Unit
            },
        )

        assertNull(submitted)

        menu.submitList(listOf(DecorativeTextMenuCandidate("Hello")))
        assertEquals(listOf(DecorativeTextMenuCandidate("Hello")), submitted)
    }

    @Test
    fun `dismissing the browser menu will dismiss the popup`() {
        var dismissed = false
        val menu: MenuController = BrowserMenuController()
        menu.submitList(listOf(DecorativeTextMenuCandidate("Hello")))

        menu.register(
            object : MenuController.Observer {
                override fun onDismiss() {
                    dismissed = true
                }

                override fun onMenuListSubmit(list: List<MenuCandidate>) = Unit
            },
        )

        menu.dismiss()
        assertFalse(dismissed)

        val anchor = Button(testContext)
        val popup = menu.show(anchor)

        assertTrue(popup.isShowing)
        assertFalse(dismissed)

        menu.dismiss()

        assertFalse(popup.isShowing)
        assertTrue(dismissed)
    }

    @Test
    fun `WHEN displayPopup is called with provided positioning data THEN showAtLocation is called with provided positioning values & menu height and animation are set`() {
        val containerView = createContainerView()
        val popupWindow = Mockito.spy(PopupWindow())

        val (x, y) = 20 to 25
        val anchor = createAnchor(x, y)

        val (targetX, targetY) = getTargetCoordinates(x, y, containerView, anchor)
        val positioningData = MenuPositioningData(
            anchor = anchor,
            x = targetX,
            y = targetY,
            containerHeight = containerView.measuredHeight,
            animation = R.style.Mozac_Browser_Menu2_Animation_OverflowMenuLeftTop,
        )

        popupWindow.displayPopup(positioningData)

        assertEquals(containerView.measuredHeight, popupWindow.height)
        assertEquals(positioningData.animation, popupWindow.animationStyle)

        Mockito.verify(popupWindow).showAtLocation(
            positioningData.anchor,
            Gravity.NO_GRAVITY,
            positioningData.x,
            positioningData.y,
        )
    }
}
