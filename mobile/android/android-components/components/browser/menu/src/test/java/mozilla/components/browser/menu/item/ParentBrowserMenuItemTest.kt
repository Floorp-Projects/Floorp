/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

package mozilla.components.browser.menu.item

import android.view.LayoutInflater
import android.widget.TextView
import androidx.appcompat.widget.AppCompatImageView
import androidx.core.content.ContextCompat
import androidx.test.ext.junit.runners.AndroidJUnit4
import mozilla.components.browser.menu.BrowserMenu
import mozilla.components.browser.menu.BrowserMenuAdapter
import mozilla.components.browser.menu.R
import mozilla.components.concept.menu.candidate.DrawableMenuIcon
import mozilla.components.concept.menu.candidate.TextMenuCandidate
import mozilla.components.support.test.mock
import mozilla.components.support.test.robolectric.testContext
import org.junit.Assert.assertEquals
import org.junit.Assert.assertFalse
import org.junit.Assert.assertNotNull
import org.junit.Assert.assertNull
import org.junit.Assert.assertTrue
import org.junit.Test
import org.junit.runner.RunWith

@RunWith(AndroidJUnit4::class)
class ParentBrowserMenuItemTest {

    @Test
    fun `menu item ImageText should have the right text, image, and iconTintColorResource`() {
        val subMenuItem = SimpleBrowserMenuItem("test")
        val subMenuAdapter = BrowserMenuAdapter(testContext, listOf(subMenuItem))
        val subMenu = BrowserMenu(subMenuAdapter)
        val parentMenuItem = ParentBrowserMenuItem(
            label = "label",
            imageResource = android.R.drawable.ic_menu_report_image,
            iconTintColorResource = android.R.color.black,
            textColorResource = android.R.color.black,
            subMenu = subMenu
        )
        val view = LayoutInflater.from(testContext).inflate(parentMenuItem.getLayoutResource(), null)
        val nestedMenuAdapter = BrowserMenuAdapter(testContext, listOf(parentMenuItem))
        val nestedMenu = BrowserMenu(nestedMenuAdapter)

        parentMenuItem.bind(nestedMenu, view)
        val textView = view.findViewById<TextView>(R.id.text)
        val imageView = view.findViewById<AppCompatImageView>(R.id.image)
        val overflowView = view.findViewById<AppCompatImageView>(R.id.overflowImage)

        assertEquals("label", textView.text)
        assertNotNull(imageView.drawable)
        assertNotNull(imageView.imageTintList)
        assertNotNull(overflowView.imageTintList)
    }

    @Test
    fun `menu item ImageText with no iconTintColorResource must not have an imageTintList`() {
        val subMenuItem = SimpleBrowserMenuItem("test")
        val subMenuAdapter = BrowserMenuAdapter(testContext, listOf(subMenuItem))
        val subMenu = BrowserMenu(subMenuAdapter)
        val parentMenuItem = ParentBrowserMenuItem(
            label = "label",
            imageResource = android.R.drawable.ic_menu_report_image,
            subMenu = subMenu
        )
        val view = LayoutInflater.from(testContext).inflate(parentMenuItem.getLayoutResource(), null)
        val nestedMenuAdapter = BrowserMenuAdapter(testContext, listOf(parentMenuItem))
        val nestedMenu = BrowserMenu(nestedMenuAdapter)

        parentMenuItem.bind(nestedMenu, view)
        val imageView = view.findViewById<AppCompatImageView>(R.id.image)

        assertNull(imageView.imageTintList)
    }

    @Test
    fun `onBackPressed after sub menu is shown will dismiss the sub menu`() {
        val backPressMenuItem = BackPressMenuItem(
            label = "back",
            imageResource = R.drawable.mozac_ic_back
        )
        val backPressView = LayoutInflater.from(testContext).inflate(backPressMenuItem.getLayoutResource(), null)
        val subMenuItem = SimpleBrowserMenuItem("test")
        val subMenuAdapter = BrowserMenuAdapter(testContext, listOf(backPressMenuItem, subMenuItem))
        val subMenu = BrowserMenu(subMenuAdapter)
        backPressMenuItem.bind(subMenu, backPressView)

        val parentMenuItem = ParentBrowserMenuItem(
            label = "label",
            imageResource = android.R.drawable.ic_menu_report_image,
            subMenu = subMenu
        )
        val view = LayoutInflater.from(testContext).inflate(parentMenuItem.getLayoutResource(), null)
        val nestedMenuAdapter = BrowserMenuAdapter(testContext, listOf(parentMenuItem))
        val nestedMenu = BrowserMenu(nestedMenuAdapter)
        parentMenuItem.bind(nestedMenu, view)

        nestedMenu.show(view)
        view.performClick()
        assertTrue(subMenu.isShown)
        assertFalse(nestedMenu.isShown)

        backPressView.performClick()
        assertFalse(subMenu.isShown)
        assertTrue(nestedMenu.isShown)
    }

    @Test
    fun `menu item image text item can be converted to candidate`() {
        assertEquals(
            TextMenuCandidate(
                "label",
                start = DrawableMenuIcon(null)
            ),
            ParentBrowserMenuItem(
                "label",
                android.R.drawable.ic_menu_report_image,
                subMenu = mock()
            ).asCandidate(testContext).run {
                copy(start = (start as? DrawableMenuIcon)?.copy(drawable = null))
            }
        )

        assertEquals(
            TextMenuCandidate(
                text = "label",
                start = DrawableMenuIcon(
                    drawable = null,
                    tint = ContextCompat.getColor(testContext, android.R.color.black)
                )
            ),
                ParentBrowserMenuItem(
                "label",
                android.R.drawable.ic_menu_report_image,
                android.R.color.black,
                subMenu = mock()
            ).asCandidate(testContext).run {
                copy(start = (start as? DrawableMenuIcon)?.copy(drawable = null))
            }
        )
    }
}
