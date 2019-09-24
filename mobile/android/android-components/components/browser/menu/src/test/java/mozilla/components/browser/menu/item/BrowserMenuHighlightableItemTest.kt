/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

package mozilla.components.browser.menu.item

import android.content.Context
import android.view.LayoutInflater
import android.view.View
import android.widget.TextView
import androidx.appcompat.widget.AppCompatImageView
import androidx.core.view.isVisible
import androidx.test.core.app.ApplicationProvider
import androidx.test.ext.junit.runners.AndroidJUnit4
import mozilla.components.browser.menu.BrowserMenu
import mozilla.components.browser.menu.R
import org.junit.Assert.assertEquals
import org.junit.Assert.assertFalse
import org.junit.Assert.assertNotNull
import org.junit.Assert.assertNull
import org.junit.Assert.assertTrue
import org.junit.Test
import org.junit.runner.RunWith
import org.mockito.Mockito.mock
import org.robolectric.Shadows

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
            BrowserMenuHighlightableItem.Highlight(
                    endImageResource = android.R.drawable.ic_menu_report_image,
                    backgroundResource = R.color.photonRed50,
                    colorResource = R.color.photonRed50
            )
        ) {
            onClickWasPress = true
        }

        val view = inflate(item)

        view.performClick()
        assertTrue(onClickWasPress)
    }

    @Test
    fun `browser menu highlightable item should properly handle highlighting`() {
        var shouldHighlight = false
        val item = BrowserMenuHighlightableItem(
                label = "label",
                startImageResource = android.R.drawable.ic_menu_report_image,
                iconTintColorResource = android.R.color.black,
                textColorResource = android.R.color.black,
                highlight = BrowserMenuHighlightableItem.Highlight(
                        startImageResource = android.R.drawable.ic_menu_camera,
                        endImageResource = android.R.drawable.ic_menu_add,
                        backgroundResource = R.color.photonRed50,
                        colorResource = R.color.photonRed50
                ),
                isHighlighted = { shouldHighlight }
        )

        val view = inflate(item)

        val textView = view.findViewById<TextView>(R.id.text)
        assertEquals(textView.text, "label")

        val startImageView = view.findViewById<AppCompatImageView>(R.id.image)
        val highlightImageView = view.findViewById<AppCompatImageView>(R.id.end_image)

        // Highlight should not exist before set
        assertEquals(android.R.drawable.ic_menu_report_image, Shadows.shadowOf(startImageView.drawable).createdFromResId)
        assertFalse(highlightImageView.isVisible)

        shouldHighlight = true
        item.invalidate(view)

        // Highlight should now exist
        assertTrue(highlightImageView.isVisible)
        assertNotNull(startImageView.imageTintList)
        assertEquals(android.R.drawable.ic_menu_camera, Shadows.shadowOf(startImageView.drawable).createdFromResId)
        assertEquals(android.R.drawable.ic_menu_add, Shadows.shadowOf(highlightImageView.drawable).createdFromResId)
        assertNotNull(highlightImageView.imageTintList)
        assertEquals(R.color.photonRed50, Shadows.shadowOf(view.background).createdFromResId)
    }

    @Test
    fun `browser menu highlightable item with with no iconTintColorResource must not have an imageTintList`() {
        val item = BrowserMenuHighlightableItem(
            "label",
            android.R.drawable.ic_menu_report_image,
            highlight = BrowserMenuHighlightableItem.Highlight(
                    endImageResource = android.R.drawable.ic_menu_report_image,
                    backgroundResource = R.color.photonRed50,
                    colorResource = R.color.photonRed50
            )
        )

        val view = inflate(item)

        val startImageView = view.findViewById<AppCompatImageView>(R.id.image)
        val endImageView = view.findViewById<AppCompatImageView>(R.id.end_image)

        assertNull(startImageView.imageTintList)
        assertNull(endImageView.imageTintList)
    }

    @Test
    fun `browser menu highlightable item with with no highlight must not have highlightImageView visible`() {
        val item = BrowserMenuHighlightableItem(
            "label",
            android.R.drawable.ic_menu_report_image,
            highlight = null
        )

        val view = inflate(item)
        val endImageView = view.findViewById<AppCompatImageView>(R.id.end_image)
        assertEquals(endImageView.visibility, View.GONE)
    }

    private fun inflate(item: BrowserMenuHighlightableItem): View {
        val view = LayoutInflater.from(context).inflate(item.getLayoutResource(), null)
        val mockMenu = mock(BrowserMenu::class.java)
        item.bind(mockMenu, view)
        return view
    }
}
