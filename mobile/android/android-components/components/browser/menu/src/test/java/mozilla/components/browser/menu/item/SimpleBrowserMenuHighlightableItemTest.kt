/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

package mozilla.components.browser.menu.item

import android.graphics.Color
import android.graphics.drawable.ColorDrawable
import android.view.LayoutInflater
import android.view.View
import android.widget.TextView
import androidx.core.content.ContextCompat
import androidx.test.ext.junit.runners.AndroidJUnit4
import mozilla.components.browser.menu.BrowserMenu
import mozilla.components.browser.menu.R
import mozilla.components.concept.menu.candidate.ContainerStyle
import mozilla.components.concept.menu.candidate.TextMenuCandidate
import mozilla.components.concept.menu.candidate.TextStyle
import mozilla.components.support.test.robolectric.testContext
import org.junit.Assert.assertEquals
import org.junit.Assert.assertTrue
import org.junit.Test
import org.junit.runner.RunWith
import org.mockito.Mockito
import org.mockito.Mockito.mock
import org.mockito.Mockito.verify

@RunWith(AndroidJUnit4::class)
class SimpleBrowserMenuHighlightableItemTest {

    @Test
    fun `GIVEN a simple item, WHEN we try to inflate it in the menu, THEN the item should be inflated`() {
        var onClickWasPress = false
        val item = SimpleBrowserMenuHighlightableItem(
            "label",
            textColorResource = android.R.color.black,
            backgroundTint = Color.RED,
        ) {
            onClickWasPress = true
        }

        val view = inflate(item)
        view.performClick()

        assertTrue(onClickWasPress)
    }

    @Test
    fun `GIVEN a simple item, WHEN we inflate it, THEN it should be visible by default`() {
        val listener = {}
        val item = SimpleBrowserMenuHighlightableItem(
            label = "label",
            textColorResource = android.R.color.black,
            backgroundTint = Color.RED,
            listener = listener,
        )

        assertTrue(item.visible())
    }

    @Test
    fun `GIVEN a simple item, WHEN clicking bound view, THEN callback is invoked and the menu dismissed`() {
        var callbackInvoked = false

        val item = SimpleBrowserMenuHighlightableItem(
            label = "label",
            textColorResource = android.R.color.black,
            backgroundTint = Color.RED,
        ) {
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
    fun `GIVEN a simple item, WHEN we inflate it, THEN it should have the right properties`() {
        val listener = {}
        val item = SimpleBrowserMenuHighlightableItem(
            label = "label",
            textSize = 10f,
            textColorResource = android.R.color.black,
            backgroundTint = Color.RED,
            isHighlighted = { false },
            listener = listener,
        )

        var view = inflate(item)
        var textView = view.findViewById<TextView>(R.id.simple_text)

        assertEquals(textView.text, "label")
        assertEquals(textView.textSize, 10f)
        assertEquals(textView.currentTextColor, testContext.getColor(android.R.color.black))

        val highlightedItem = SimpleBrowserMenuHighlightableItem(
            label = "label",
            textSize = 10f,
            textColorResource = android.R.color.black,
            backgroundTint = Color.RED,
            isHighlighted = { true },
            listener = listener,
        )

        view = inflate(highlightedItem)
        textView = view.findViewById(R.id.simple_text)

        assertEquals(textView.text, "label")
        assertEquals(textView.textSize, 10f)
        assertEquals(textView.currentTextColor, testContext.getColor(android.R.color.black))
        assertEquals((textView.background as ColorDrawable).color, Color.RED)
    }

    @Test
    fun `GIVEN a simple item, WHEN it converts to candidate, THEN it should have the correct properties`() {
        val listener = {}
        val shouldHighlight = false
        val item = SimpleBrowserMenuHighlightableItem(
            label = "label",
            textColorResource = android.R.color.black,
            backgroundTint = Color.RED,
            isHighlighted = { shouldHighlight },
            listener = listener,
        )

        assertEquals(
            TextMenuCandidate(
                "label",
                textStyle = TextStyle(
                    color = ContextCompat.getColor(testContext, android.R.color.black),
                ),
                containerStyle = ContainerStyle(true),
                onClick = listener,
            ),
            item.asCandidate(testContext),
        )
    }

    private fun inflate(item: SimpleBrowserMenuHighlightableItem): View {
        val view = LayoutInflater.from(testContext).inflate(item.getLayoutResource(), null)
        val mockMenu = Mockito.mock(BrowserMenu::class.java)
        item.bind(mockMenu, view)
        return view
    }
}
