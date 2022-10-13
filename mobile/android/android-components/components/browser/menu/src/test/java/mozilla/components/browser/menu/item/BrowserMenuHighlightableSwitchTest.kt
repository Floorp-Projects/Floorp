/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

package mozilla.components.browser.menu.item

import android.graphics.Color
import android.view.LayoutInflater
import android.widget.ImageView
import androidx.appcompat.widget.AppCompatImageView
import androidx.appcompat.widget.SwitchCompat
import androidx.constraintlayout.widget.ConstraintLayout
import androidx.core.view.isVisible
import androidx.test.ext.junit.runners.AndroidJUnit4
import mozilla.components.browser.menu.BrowserMenu
import mozilla.components.browser.menu.BrowserMenuHighlight
import mozilla.components.browser.menu.R
import mozilla.components.support.test.robolectric.testContext
import org.junit.Assert.assertEquals
import org.junit.Assert.assertFalse
import org.junit.Assert.assertNull
import org.junit.Assert.assertTrue
import org.junit.Test
import org.junit.runner.RunWith
import org.mockito.Mockito.mock
import org.robolectric.Shadows

@RunWith(AndroidJUnit4::class)
class BrowserMenuHighlightableSwitchTest {

    @Test
    fun `menu item uses correct layout`() {
        val item = BrowserMenuHighlightableSwitch(
            label = "label",
            startImageResource = android.R.drawable.ic_menu_report_image,
            highlight = BrowserMenuHighlight.LowPriority(
                notificationTint = Color.RED,
            ),
        ) {}

        assertEquals(R.layout.mozac_browser_menu_highlightable_switch, item.getLayoutResource())
    }

    @Suppress("Deprecation")
    @Test
    fun `browser menu highlightable item should be inflated`() {
        var onClickWasPress = false
        val item = BrowserMenuHighlightableSwitch(
            label = "label",
            startImageResource = android.R.drawable.ic_menu_report_image,
            highlight = BrowserMenuHighlight.LowPriority(
                notificationTint = Color.RED,
            ),
        ) {
            onClickWasPress = true
        }

        val view = inflate(item)

        view.switch.performClick()
        assertTrue(onClickWasPress)
    }

    @Test
    fun `browser menu highlightable item should properly handle low priority highlighting`() {
        var shouldHighlight = false
        val item = BrowserMenuHighlightableSwitch(
            label = "label",
            startImageResource = android.R.drawable.ic_menu_report_image,
            iconTintColorResource = android.R.color.black,
            textColorResource = android.R.color.black,
            highlight = BrowserMenuHighlight.LowPriority(
                notificationTint = R.color.photonRed50,
                label = "highlight",
            ),
            isHighlighted = { shouldHighlight },
        ) {}

        val view = inflate(item)

        assertEquals("label", view.switch.text)

        val startImageView = view.findViewById<AppCompatImageView>(R.id.image)

        // Highlight should not exist before set
        assertEquals(android.R.drawable.ic_menu_report_image, Shadows.shadowOf(startImageView.drawable).createdFromResId)
        assertFalse(view.notificationDot.isVisible)

        shouldHighlight = true
        item.invalidate(view)

        // Highlight should now exist
        assertEquals("highlight", view.switch.text)
        assertTrue(view.notificationDot.isVisible)
    }

    @Test
    fun `browser menu highlightable item with with no iconTintColorResource must not have a tinted icon`() {
        val item = BrowserMenuHighlightableSwitch(
            "label",
            startImageResource = android.R.drawable.ic_menu_report_image,
            highlight = BrowserMenuHighlight.LowPriority(
                notificationTint = Color.RED,
            ),
        ) {}

        val view = inflate(item)

        assertEquals("label", view.switch.text)
        assertNull(view.startImageView.drawable!!.colorFilter)
    }

    private fun inflate(item: BrowserMenuHighlightableSwitch): ConstraintLayout {
        val view = LayoutInflater.from(testContext).inflate(item.getLayoutResource(), null)
        val mockMenu = mock(BrowserMenu::class.java)
        item.bind(mockMenu, view)
        return view as ConstraintLayout
    }

    private val ConstraintLayout.startImageView: ImageView get() = findViewById(R.id.image)
    private val ConstraintLayout.notificationDot: ImageView get() = findViewById(R.id.notification_dot)
    private val ConstraintLayout.switch: SwitchCompat get() = findViewById(R.id.switch_widget)
}
