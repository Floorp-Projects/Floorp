/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

package mozilla.components.browser.menu.item

import android.view.LayoutInflater
import android.view.View
import android.widget.TextView
import androidx.core.content.ContextCompat.getColor
import androidx.test.ext.junit.runners.AndroidJUnit4
import mozilla.components.browser.menu.BrowserMenu
import mozilla.components.browser.menu.R
import mozilla.components.concept.menu.candidate.DecorativeTextMenuCandidate
import mozilla.components.concept.menu.candidate.TextMenuCandidate
import mozilla.components.concept.menu.candidate.TextStyle
import mozilla.components.support.test.robolectric.testContext
import org.junit.Assert.assertEquals
import org.junit.Assert.assertNotNull
import org.junit.Assert.assertTrue
import org.junit.Test
import org.junit.runner.RunWith
import org.mockito.Mockito.mock
import org.mockito.Mockito.verify

@RunWith(AndroidJUnit4::class)
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

        val view = LayoutInflater.from(testContext)
            .inflate(item.getLayoutResource(), null)

        assertNotNull(view)
    }

    @Test
    fun `clicking bound view will invoke callback and dismiss menu`() {
        var callbackInvoked = false

        val item = SimpleBrowserMenuItem("Hello") {
            callbackInvoked = true
        }

        val menu = mock(BrowserMenu::class.java)
        val view = TextView(testContext)

        item.bind(menu, view)

        view.performClick()

        assertTrue(callbackInvoked)
        verify(menu).dismiss()
    }

    @Test
    fun `simple browser menu item should have the right text, textSize, and textColorResource`() {
        val item = SimpleBrowserMenuItem(
            "Powered by Mozilla",
            10f,
            android.R.color.holo_green_dark,
        )

        val view = inflate(item)

        val textView = view.findViewById<TextView>(R.id.simple_text)
        assertEquals(textView.text, "Powered by Mozilla")
        assertEquals(textView.textSize, 10f)
        assertEquals(textView.currentTextColor, testContext.getColor(android.R.color.holo_green_dark))
    }

    @Test
    fun `simple menu item can be converted to candidate`() {
        val listener = {}

        assertEquals(
            TextMenuCandidate(
                "Hello",
                onClick = listener,
            ),
            SimpleBrowserMenuItem(
                "Hello",
                listener = listener,
            ).asCandidate(testContext),
        )

        assertEquals(
            DecorativeTextMenuCandidate(
                "Powered by Mozilla",
                textStyle = TextStyle(
                    size = 10f,
                    color = getColor(testContext, android.R.color.holo_green_dark),
                ),
            ),
            SimpleBrowserMenuItem(
                "Powered by Mozilla",
                10f,
                android.R.color.holo_green_dark,
            ).asCandidate(testContext),
        )
    }

    private fun inflate(item: SimpleBrowserMenuItem): View {
        val view = LayoutInflater.from(testContext).inflate(item.getLayoutResource(), null)
        val mockMenu = mock(BrowserMenu::class.java)
        item.bind(mockMenu, view)
        return view
    }
}
