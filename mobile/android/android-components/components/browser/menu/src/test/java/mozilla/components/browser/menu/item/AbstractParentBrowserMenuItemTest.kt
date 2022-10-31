/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

package mozilla.components.browser.menu.item

import android.view.View
import androidx.test.ext.junit.runners.AndroidJUnit4
import mozilla.components.browser.menu.BrowserMenu
import mozilla.components.browser.menu.BrowserMenuAdapter
import mozilla.components.browser.menu.R
import mozilla.components.support.test.robolectric.testContext
import org.junit.Assert.assertFalse
import org.junit.Assert.assertTrue
import org.junit.Test
import org.junit.runner.RunWith

@RunWith(AndroidJUnit4::class)
class AbstractParentBrowserMenuItemTest {

    @Test
    fun bind() {
        val view = View(testContext)
        var subMenuShowCalled = false
        var subMenuDismissCalled = false

        val subMenuItem = SimpleBrowserMenuItem("test")
        val subMenuAdapter = BrowserMenuAdapter(testContext, listOf(subMenuItem))
        val subMenu = BrowserMenu(subMenuAdapter)
        val parentMenuItem = DummyParentBrowserMenuItem(subMenu)
        val nestedMenuAdapter = BrowserMenuAdapter(testContext, listOf(parentMenuItem))
        val nestedMenu = BrowserMenu(nestedMenuAdapter)

        parentMenuItem.onSubMenuShow = {
            subMenuShowCalled = true
        }
        parentMenuItem.onSubMenuDismiss = {
            subMenuDismissCalled = true
        }

        parentMenuItem.bind(nestedMenu, view)
        nestedMenu.show(view)
        assertTrue(nestedMenu.isShown)

        view.performClick()
        assertFalse(nestedMenu.isShown)
        assertTrue(subMenu.isShown)
        assertTrue(subMenuShowCalled)

        subMenu.dismiss()
        assertTrue(subMenuDismissCalled)
    }

    @Test
    fun onBackPressed() {
        val view = View(testContext)
        var subMenuDismissCalled = false

        val subMenuItem = SimpleBrowserMenuItem("test")
        val subMenuAdapter = BrowserMenuAdapter(testContext, listOf(subMenuItem))
        val subMenu = BrowserMenu(subMenuAdapter)
        val parentMenuItem = DummyParentBrowserMenuItem(subMenu)
        val nestedMenuAdapter = BrowserMenuAdapter(testContext, listOf(parentMenuItem))
        val nestedMenu = BrowserMenu(nestedMenuAdapter)

        parentMenuItem.onSubMenuDismiss = {
            subMenuDismissCalled = true
        }

        parentMenuItem.bind(nestedMenu, view)
        // verify onBackPressed while sub menu is not shown does nothing.
        parentMenuItem.onBackPressed(nestedMenu, view)
        assertFalse(subMenuDismissCalled)

        nestedMenu.show(view)
        view.performClick()
        parentMenuItem.onBackPressed(nestedMenu, view)
        assertTrue(nestedMenu.isShown)
        assertFalse(subMenu.isShown)
        assertTrue(subMenuDismissCalled)
    }
}

class DummyParentBrowserMenuItem(
    subMenu: BrowserMenu,
    endOfMenuAlwaysVisible: Boolean = false,
) : AbstractParentBrowserMenuItem(subMenu, endOfMenuAlwaysVisible) {
    override var visible: () -> Boolean = { true }
    override fun getLayoutResource(): Int = R.layout.mozac_browser_menu_item_simple
}
