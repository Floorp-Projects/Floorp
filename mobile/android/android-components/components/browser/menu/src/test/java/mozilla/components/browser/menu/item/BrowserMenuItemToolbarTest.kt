/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

package mozilla.components.browser.menu.item

import android.view.LayoutInflater
import android.widget.ImageButton
import android.widget.LinearLayout
import android.widget.TextView
import mozilla.components.browser.menu.BrowserMenu
import mozilla.components.browser.menu.R
import org.junit.Assert.assertEquals
import org.junit.Assert.assertFalse
import org.junit.Assert.assertNotNull
import org.junit.Assert.assertTrue
import org.junit.Test
import org.junit.runner.RunWith
import org.mockito.Mockito.mock
import org.mockito.Mockito.never
import org.mockito.Mockito.verify
import org.robolectric.RobolectricTestRunner
import org.robolectric.RuntimeEnvironment

@RunWith(RobolectricTestRunner::class)
class BrowserMenuItemToolbarTest {
    @Test
    fun `toolbar is visible by default`() {
        val toolbar = BrowserMenuItemToolbar(emptyList())

        assertTrue(toolbar.visible())
    }

    @Test
    fun `layout resource can be inflated`() {
        val toolbar = BrowserMenuItemToolbar(emptyList())

        val view = LayoutInflater.from(
                RuntimeEnvironment.application
        ).inflate(toolbar.getLayoutResource(), null)

        assertNotNull(view)
    }

    @Test
    fun `empty toolbar does not add anything to view group`() {
        val layout = LinearLayout(RuntimeEnvironment.application)

        val menu = mock(BrowserMenu::class.java)

        val toolbar = BrowserMenuItemToolbar(emptyList())
        toolbar.bind(menu, layout)

        assertEquals(0, layout.childCount)
    }

    @Test
    fun `toolbar removes previously existing child views from view group`() {
        val layout = LinearLayout(RuntimeEnvironment.application)
        layout.addView(TextView(RuntimeEnvironment.application))
        layout.addView(TextView(RuntimeEnvironment.application))

        assertEquals(2, layout.childCount)

        val menu = mock(BrowserMenu::class.java)

        val toolbar = BrowserMenuItemToolbar(emptyList())
        toolbar.bind(menu, layout)

        assertEquals(0, layout.childCount)
    }

    @Test
    fun `items are added as ImageButton to view group`() {
        val buttons = listOf(
                BrowserMenuItemToolbar.Button(
                        R.drawable.abc_ic_ab_back_material,
                        "Button01") {},
                BrowserMenuItemToolbar.Button(
                        R.drawable.abc_ic_ab_back_material,
                        "Button02") {})

        val menu = mock(BrowserMenu::class.java)
        val layout = LinearLayout(RuntimeEnvironment.application)

        val toolbar = BrowserMenuItemToolbar(buttons)
        toolbar.bind(menu, layout)

        assertEquals(2, layout.childCount)

        val child1 = layout.getChildAt(0)
        val child2 = layout.getChildAt(1)

        assertTrue(child1 is ImageButton)
        assertTrue(child2 is ImageButton)

        assertEquals("Button01", child1.contentDescription)
        assertEquals("Button02", child2.contentDescription)
    }

    @Test
    fun `clicking item view invokes callback and dismisses menu`() {
        var callbackInvoked = false

        val button = BrowserMenuItemToolbar.Button(
            R.drawable.abc_ic_ab_back_material,
            "Test") {
            callbackInvoked = true
        }

        assertFalse(callbackInvoked)

        val menu = mock(BrowserMenu::class.java)
        val layout = LinearLayout(RuntimeEnvironment.application)

        val toolbar = BrowserMenuItemToolbar(listOf(button))
        toolbar.bind(menu, layout)

        assertEquals(1, layout.childCount)

        val view = layout.getChildAt(0)

        assertFalse(callbackInvoked)
        verify(menu, never()).dismiss()

        view.performClick()

        assertTrue(callbackInvoked)
        verify(menu).dismiss()
    }
}
