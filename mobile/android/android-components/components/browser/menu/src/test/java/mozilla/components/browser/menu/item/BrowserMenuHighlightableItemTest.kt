/*
 * This Source Code Form is subject to the terms of the Mozilla Public
 *  License, v. 2.0. If a copy of the MPL was not distributed with this
 *  file, You can obtain one at http://mozilla.org/MPL/2.0/.
 */

package mozilla.components.browser.menu.item

import android.content.Context
import android.view.LayoutInflater
import android.view.View
import android.widget.TextView
import androidx.appcompat.widget.AppCompatImageView
import androidx.test.core.app.ApplicationProvider
import androidx.test.ext.junit.runners.AndroidJUnit4
import mozilla.components.browser.menu.BrowserMenu
import mozilla.components.browser.menu.R
import org.junit.Assert.assertEquals
import org.junit.Assert.assertNotNull
import org.junit.Assert.assertNull
import org.junit.Assert.assertTrue
import org.junit.Test
import org.junit.runner.RunWith
import org.mockito.Mockito.mock

@RunWith(AndroidJUnit4::class)
class BrowserMenuHighlightableItemTest {

    private val context: Context get() = ApplicationProvider.getApplicationContext()

    @Test
    fun `browser menu highlightable item should be inflated`() {
        var onClickWasPress = false
        val item = BrowserMenuHighlightableItem(
            "label",
            android.R.drawable.ic_menu_report_image,
            android.R.color.black,
            android.R.color.black,
            BrowserMenuHighlightableItem.Highlight(android.R.drawable.ic_menu_report_image, R.color.photonRed50, R.color.photonRed50)
        ) {
            onClickWasPress = true
        }

        val view = inflate(item)

        view.performClick()
        assertTrue(onClickWasPress)
    }

    @Test
    fun `browser menu highlightable item  should have the right text, image, and iconTintColorResource`() {
        val item = BrowserMenuHighlightableItem(
            "label",
            android.R.drawable.ic_menu_report_image,
            android.R.color.black,
            android.R.color.black,
            BrowserMenuHighlightableItem.Highlight(android.R.drawable.ic_menu_report_image, R.color.photonRed50, R.color.photonRed50)
        )

        val view = inflate(item)

        val textView = view.findViewById<TextView>(R.id.text)
        assertEquals(textView.text, "label")

        val imageView = view.findViewById<AppCompatImageView>(R.id.image)
        val highlightImageView = view.findViewById<AppCompatImageView>(R.id.highlight_image)

        assertNotNull(imageView.drawable)
        assertNotNull(imageView.imageTintList)
        assertEquals(highlightImageView.visibility, View.VISIBLE)
        assertNotNull(highlightImageView.drawable)
        assertNotNull(highlightImageView.imageTintList)
        assertNotNull(view.background)
    }

    @Test
    fun `browser menu highlightable item with with no iconTintColorResource must not have an imageTintList`() {
        val item = BrowserMenuHighlightableItem(
            "label",
            android.R.drawable.ic_menu_report_image,
            highlight = BrowserMenuHighlightableItem.Highlight(android.R.drawable.ic_menu_report_image, R.color.photonRed50, R.color.photonRed50)
        )

        val view = inflate(item)

        val imageView = view.findViewById<AppCompatImageView>(R.id.image)
        val highlightImageView = view.findViewById<AppCompatImageView>(R.id.highlight_image)

        assertNull(imageView.imageTintList)
        assertNull(highlightImageView.imageTintList)
    }

    @Test
    fun `browser menu highlightable item with with no highlight must not have highlightImageView visible`() {
        val item = BrowserMenuHighlightableItem(
            "label",
            android.R.drawable.ic_menu_report_image,
            highlight = null
        )

        val view = inflate(item)
        val highlightImageView = view.findViewById<AppCompatImageView>(R.id.highlight_image)
        assertEquals(highlightImageView.visibility, View.GONE)
    }

    private fun inflate(item: BrowserMenuHighlightableItem): View {
        val view = LayoutInflater.from(context).inflate(item.getLayoutResource(), null)
        val mockMenu = mock(BrowserMenu::class.java)
        item.bind(mockMenu, view)
        return view
    }
}
