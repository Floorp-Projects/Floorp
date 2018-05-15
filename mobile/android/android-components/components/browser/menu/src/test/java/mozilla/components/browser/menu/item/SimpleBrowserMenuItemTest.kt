/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

package mozilla.components.browser.menu.item

import android.view.LayoutInflater
import android.widget.TextView
import mozilla.components.browser.menu.BrowserMenu
import org.junit.Assert.assertNotNull
import org.junit.Assert.assertTrue
import org.junit.Test
import org.junit.runner.RunWith
import org.mockito.Mockito.mock
import org.mockito.Mockito.verify
import org.robolectric.RobolectricTestRunner
import org.robolectric.RuntimeEnvironment

@RunWith(RobolectricTestRunner::class)
class SimpleBrowserMenuItemTest {
    @Test
    fun `simple menu items are always visible by default`() {
        val item = SimpleBrowserMenuItem("Hello") {
            // do nothing
        }

        assertTrue(item.visible())
    }

    @Test
    fun `layout resource can be inflated`() {
        val item = SimpleBrowserMenuItem("Hello") {
            // do nothing
        }

        val view = LayoutInflater.from(
            RuntimeEnvironment.application
        ).inflate(item.getLayoutResource(), null)

        assertNotNull(view)
    }

    @Test
    fun `clicking bound view will invoke callback and dismiss menu`() {
        var callbackInvoked = false

        val item = SimpleBrowserMenuItem("Hello") {
            callbackInvoked = true
        }

        val menu = mock(BrowserMenu::class.java)
        val view = TextView(RuntimeEnvironment.application)

        item.bind(menu, view)

        view.performClick()

        assertTrue(callbackInvoked)
        verify(menu).dismiss()
    }
}