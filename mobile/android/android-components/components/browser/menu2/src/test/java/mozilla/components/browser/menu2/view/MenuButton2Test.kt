/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

package mozilla.components.browser.menu2.view

import android.content.res.ColorStateList
import android.graphics.Color
import android.graphics.PorterDuff
import android.graphics.PorterDuffColorFilter
import android.widget.ImageView
import androidx.appcompat.widget.AppCompatImageView
import androidx.core.view.children
import androidx.core.view.isVisible
import androidx.test.ext.junit.runners.AndroidJUnit4
import mozilla.components.concept.menu.MenuController
import mozilla.components.concept.menu.candidate.HighPriorityHighlightEffect
import mozilla.components.concept.menu.candidate.LowPriorityHighlightEffect
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
import org.mockito.Mockito.verify

@RunWith(AndroidJUnit4::class)
class MenuButton2Test {
    private lateinit var menuController: MenuController
    private lateinit var menuButton: MenuButton2
    private lateinit var menuIcon: ImageView
    private lateinit var highlightView: ImageView
    private lateinit var notificationIconView: ImageView

    @Before
    fun setup() {
        menuController = mock()
        menuButton = MenuButton2(testContext)

        val images = menuButton.children.mapNotNull { it as? AppCompatImageView }.toList()
        highlightView = images[0]
        menuIcon = images[1]
        notificationIconView = images[2]
    }

    @Test
    fun `changing menu controller dismisses old menu`() {
        menuButton.menuController = menuController
        menuButton.performClick()

        verify(menuController).register(any(), eq(menuButton))
        verify(menuController).show(menuButton)

        menuButton.menuController = mock()
        verify(menuController).dismiss()
        verify(menuController).unregister(any())
    }

    @Test
    fun `changing menu controller to null dismisses old menu`() {
        menuButton.menuController = menuController
        menuButton.performClick()

        verify(menuController).register(any(), eq(menuButton))

        menuButton.menuController = null
        verify(menuController).dismiss()
        verify(menuController).unregister(any())
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
    fun `icon displays high priority highlight`() {
        assertFalse(highlightView.isVisible)
        assertFalse(notificationIconView.isVisible)

        menuButton.setEffect(
            HighPriorityHighlightEffect(Color.RED),
        )

        assertTrue(highlightView.isVisible)
        assertFalse(notificationIconView.isVisible)

        assertEquals(ColorStateList.valueOf(Color.RED), highlightView.imageTintList)
    }

    @Test
    fun `icon displays low priority highlight`() {
        assertFalse(highlightView.isVisible)
        assertFalse(notificationIconView.isVisible)

        menuButton.setEffect(
            LowPriorityHighlightEffect(Color.BLUE),
        )

        assertFalse(highlightView.isVisible)
        assertTrue(notificationIconView.isVisible)

        assertEquals(PorterDuffColorFilter(Color.BLUE, PorterDuff.Mode.SRC_ATOP), notificationIconView.colorFilter)
    }
}
