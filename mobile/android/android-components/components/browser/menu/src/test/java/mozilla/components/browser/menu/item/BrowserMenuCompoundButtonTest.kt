/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

package mozilla.components.browser.menu.item

import android.view.LayoutInflater
import android.view.View
import android.widget.CheckBox
import androidx.test.ext.junit.runners.AndroidJUnit4
import mozilla.components.browser.menu.BrowserMenu
import mozilla.components.browser.menu.R
import mozilla.components.support.test.robolectric.testContext
import org.junit.Assert.assertNotNull
import org.junit.Assert.assertTrue
import org.junit.Test
import org.junit.runner.RunWith
import org.mockito.Mockito.mock
import org.mockito.Mockito.spy
import org.mockito.Mockito.verify

@RunWith(AndroidJUnit4::class)
class BrowserMenuCompoundButtonTest {

    @Test
    fun `simple menu items are always visible by default`() {
        val item = SimpleTestBrowserCompoundButton("Hello") {
            // do nothing
        }

        assertTrue(item.visible())
    }

    @Test
    fun `layout resource can be inflated`() {
        val item = SimpleTestBrowserCompoundButton("Hello") {
            // do nothing
        }

        val view = LayoutInflater.from(testContext)
            .inflate(item.getLayoutResource(), null)

        assertNotNull(view)
    }

    @Test
    fun `clicking bound view will invoke callback and dismiss menu`() {
        var callbackInvoked = false

        val item = SimpleTestBrowserCompoundButton("Hello") { checked ->
            callbackInvoked = checked
        }

        val menu = mock(BrowserMenu::class.java)
        val view = CheckBox(testContext)

        item.bind(menu, view)

        view.isChecked = true

        assertTrue(callbackInvoked)
        verify(menu).dismiss()
    }

    @Test
    fun `initialState is invoked on bind`() {
        val initialState: () -> Boolean = { true }
        val item = SimpleTestBrowserCompoundButton("Hello", initialState) {}

        val menu = mock(BrowserMenu::class.java)
        val view = spy(CheckBox(testContext))
        item.bind(menu, view)

        verify(view).isChecked = true
    }

    @Test
    fun `hitting default methods`() {
        val item = SimpleTestBrowserCompoundButton("") {}
        item.invalidate(mock(View::class.java))
    }

    class SimpleTestBrowserCompoundButton(
        label: String,
        initialState: () -> Boolean = { false },
        listener: (Boolean) -> Unit
    ) : BrowserMenuCompoundButton(label, initialState, listener) {
        override fun getLayoutResource(): Int = R.layout.mozac_browser_menu_item_simple
    }
}
