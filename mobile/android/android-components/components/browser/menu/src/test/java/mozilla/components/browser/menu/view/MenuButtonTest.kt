/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

package mozilla.components.browser.menu.view

import android.content.res.ColorStateList
import android.graphics.Color
import android.graphics.PorterDuff
import android.graphics.PorterDuffColorFilter
import android.widget.ImageView
import androidx.appcompat.widget.AppCompatImageView
import androidx.core.view.children
import androidx.core.view.isVisible
import androidx.test.ext.junit.runners.AndroidJUnit4
import mozilla.components.browser.menu.BrowserMenu
import mozilla.components.browser.menu.BrowserMenuBuilder
import mozilla.components.browser.menu.BrowserMenuHighlight
import mozilla.components.concept.menu.MenuController
import mozilla.components.support.test.any
import mozilla.components.support.test.eq
import mozilla.components.support.test.mock
import mozilla.components.support.test.robolectric.testContext
import org.junit.Assert.assertEquals
import org.junit.Assert.assertFalse
import org.junit.Assert.assertNotNull
import org.junit.Assert.assertNull
import org.junit.Assert.assertTrue
import org.junit.Before
import org.junit.Test
import org.junit.runner.RunWith
import org.mockito.ArgumentMatchers.anyBoolean
import org.mockito.Mockito.doReturn
import org.mockito.Mockito.never
import org.mockito.Mockito.times
import org.mockito.Mockito.verify

@RunWith(AndroidJUnit4::class)
class MenuButtonTest {
    private lateinit var menuController: MenuController
    private lateinit var menuBuilder: BrowserMenuBuilder
    private lateinit var menu: BrowserMenu
    private lateinit var menuButton: MenuButton
    private lateinit var menuIcon: ImageView
    private lateinit var highlightView: ImageView
    private lateinit var notificationIconView: ImageView

    @Before
    fun setup() {
        menuController = mock()
        menu = mock()
        menuBuilder = mock()
        doReturn(menu).`when`(menuBuilder).build(testContext)

        menuButton = MenuButton(testContext)
        val images = menuButton.children.mapNotNull { it as? AppCompatImageView }.toList()
        highlightView = images[0]
        menuIcon = images[1]
        notificationIconView = images[2]
    }

    @Test
    fun `changing menu controller dismisses old menu`() {
        menuButton.menuController = menuController
        menuButton.performClick()

        verify(menuController).show(menuButton)

        menuButton.menuController = mock()
        verify(menuController).dismiss()
    }

    @Test
    fun `changing menu builder dismisses old menu`() {
        menuButton.menuBuilder = menuBuilder
        menuButton.performClick()

        verify(menu).show(eq(menuButton), any(), any(), anyBoolean(), any())

        menuButton.menuBuilder = mock()
        verify(menu).dismiss()
    }

    @Test
    fun `opening a new menu will prefer using the controller`() {
        menuButton.menuController = menuController
        menuButton.menuBuilder = menuBuilder

        menuButton.performClick()

        verify(menuController).show(menuButton)
        verify(menuBuilder, never()).build(testContext)
        verify(menu, never()).show(any(), any(), any(), anyBoolean(), any())
    }

    @Test
    fun `trying to open a new menu when we already have one will dismiss the current`() {
        menuButton.menuBuilder = menuBuilder

        menuButton.performClick()
        menuButton.performClick()

        verify(menu, times(1)).show(eq(menuButton), any(), any(), anyBoolean(), any())
        verify(menu, times(1)).dismiss()
    }

    @Test
    fun `icon has content description`() {
        assertEquals("Menu", menuIcon.contentDescription)
        assertNotNull(menuIcon.drawable)
    }

    @Test
    fun `icon color filter can be changed`() {
        assertNull(menuIcon.colorFilter)

        menuButton.setColorFilter(0xffffff)
        assertEquals(PorterDuffColorFilter(0xffffff, PorterDuff.Mode.SRC_ATOP), menuIcon.colorFilter)
    }

    @Test
    fun `icon can invalidate menu`() {
        menuButton.menuBuilder = menuBuilder
        menuButton.performClick()

        verify(menu).show(eq(menuButton), any(), any(), anyBoolean(), any())

        menuButton.invalidateBrowserMenu()
        verify(menu).invalidate()
    }

    @Test
    fun `icon displays high priority highlight`() {
        assertFalse(highlightView.isVisible)
        assertFalse(notificationIconView.isVisible)

        menuButton.setHighlight(
            BrowserMenuHighlight.HighPriority(Color.RED),
        )

        assertTrue(highlightView.isVisible)
        assertFalse(notificationIconView.isVisible)

        assertEquals(ColorStateList.valueOf(Color.RED), highlightView.imageTintList)
    }

    @Test
    fun `icon displays low priority highlight`() {
        assertFalse(highlightView.isVisible)
        assertFalse(notificationIconView.isVisible)

        menuButton.setHighlight(
            BrowserMenuHighlight.LowPriority(Color.BLUE),
        )

        assertFalse(highlightView.isVisible)
        assertTrue(notificationIconView.isVisible)

        assertEquals(PorterDuffColorFilter(Color.BLUE, PorterDuff.Mode.SRC_ATOP), notificationIconView.colorFilter)
    }

    @Test
    fun `menu can be dismissed`() {
        menuButton.menuController = menuController
        menuButton.menu = menu

        menuButton.dismissMenu()

        verify(menuButton.menuController)?.dismiss()
        verify(menuButton.menu)?.dismiss()
    }
}
