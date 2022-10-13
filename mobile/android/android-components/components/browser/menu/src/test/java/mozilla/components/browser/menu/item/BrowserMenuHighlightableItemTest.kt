/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

package mozilla.components.browser.menu.item

import android.graphics.Color
import android.graphics.drawable.ColorDrawable
import android.view.LayoutInflater
import android.view.View
import android.widget.ImageView
import android.widget.TextView
import androidx.appcompat.widget.AppCompatImageView
import androidx.constraintlayout.widget.ConstraintLayout
import androidx.core.content.ContextCompat.getColor
import androidx.core.view.isVisible
import androidx.test.ext.junit.runners.AndroidJUnit4
import mozilla.components.browser.menu.BrowserMenu
import mozilla.components.browser.menu.BrowserMenuHighlight
import mozilla.components.browser.menu.R
import mozilla.components.concept.menu.candidate.DrawableMenuIcon
import mozilla.components.concept.menu.candidate.HighPriorityHighlightEffect
import mozilla.components.concept.menu.candidate.LowPriorityHighlightEffect
import mozilla.components.concept.menu.candidate.TextMenuCandidate
import mozilla.components.concept.menu.candidate.TextStyle
import mozilla.components.support.test.robolectric.testContext
import org.junit.Assert.assertEquals
import org.junit.Assert.assertFalse
import org.junit.Assert.assertNotEquals
import org.junit.Assert.assertNotNull
import org.junit.Assert.assertNull
import org.junit.Assert.assertTrue
import org.junit.Test
import org.junit.runner.RunWith
import org.mockito.Mockito.mock
import org.robolectric.Shadows

@RunWith(AndroidJUnit4::class)
class BrowserMenuHighlightableItemTest {

    @Suppress("Deprecation")
    @Test
    fun `browser menu highlightable item should be inflated`() {
        var onClickWasPress = false
        val item = BrowserMenuHighlightableItem(
            "label",
            imageResource = android.R.drawable.ic_menu_report_image,
            iconTintColorResource = android.R.color.black,
            textColorResource = android.R.color.black,
            highlight = BrowserMenuHighlightableItem.Highlight(
                endImageResource = android.R.drawable.ic_menu_report_image,
                backgroundResource = R.color.photonRed50,
                colorResource = R.color.photonRed50,
            ),
        ) {
            onClickWasPress = true
        }

        val view = inflate(item)

        view.performClick()
        assertTrue(onClickWasPress)
    }

    @Suppress("Deprecation")
    @Test
    fun `browser menu highlightable item should properly handle classic highlighting`() {
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
                colorResource = R.color.photonRed50,
            ),
            isHighlighted = { shouldHighlight },
        )

        val view = inflate(item)

        assertEquals("label", view.textView.text)

        // Highlight should not exist before set
        val oldDrawable = view.startImageView.drawable
        assertEquals(android.R.drawable.ic_menu_report_image, Shadows.shadowOf(oldDrawable).createdFromResId)
        assertFalse(view.endImageView.isVisible)

        shouldHighlight = true
        item.invalidate(view)

        // Highlight should now exist
        assertTrue(view.endImageView.isVisible)
        assertNotEquals(oldDrawable, view.startImageView.drawable)
        assertEquals(android.R.drawable.ic_menu_camera, Shadows.shadowOf(view.startImageView.drawable).createdFromResId)
        assertEquals(android.R.drawable.ic_menu_add, Shadows.shadowOf(view.endImageView.drawable).createdFromResId)
        assertNotNull(view.endImageView.imageTintList)
        assertEquals(R.color.photonRed50, Shadows.shadowOf(view.background).createdFromResId)
    }

    @Test
    fun `browser menu highlightable item should properly handle high priority highlighting`() {
        var shouldHighlight = false
        val item = BrowserMenuHighlightableItem(
            label = "label",
            startImageResource = android.R.drawable.ic_menu_report_image,
            iconTintColorResource = android.R.color.black,
            textColorResource = android.R.color.black,
            highlight = BrowserMenuHighlight.HighPriority(
                endImageResource = android.R.drawable.ic_menu_add,
                backgroundTint = Color.RED,
                label = "highlight",
            ),
            isHighlighted = { shouldHighlight },
        )

        val view = inflate(item)

        assertEquals("label", view.textView.text)
        assertEquals("highlight", view.highlightedTextView.text)

        // Highlight should not exist before set
        assertEquals(android.R.drawable.ic_menu_report_image, Shadows.shadowOf(view.startImageView.drawable).createdFromResId)
        assertFalse(view.highlightedTextView.isVisible)
        assertFalse(view.endImageView.isVisible)

        shouldHighlight = true
        item.invalidate(view)

        // Highlight should now exist
        assertTrue(view.highlightedTextView.isVisible)
        assertEquals(android.R.drawable.ic_menu_report_image, Shadows.shadowOf(view.startImageView.drawable).createdFromResId)
        assertEquals(android.R.drawable.ic_menu_add, Shadows.shadowOf(view.endImageView.drawable).createdFromResId)
        assertNotNull(view.endImageView.imageTintList)
        assertTrue(view.endImageView.isVisible)
    }

    @Test
    fun `browser menu highlightable item should properly handle low priority highlighting`() {
        var shouldHighlight = false
        val item = BrowserMenuHighlightableItem(
            label = "label",
            startImageResource = android.R.drawable.ic_menu_report_image,
            iconTintColorResource = android.R.color.black,
            textColorResource = android.R.color.black,
            highlight = BrowserMenuHighlight.LowPriority(
                notificationTint = Color.RED,
                label = "highlight",
            ),
            isHighlighted = { shouldHighlight },
        )

        val view = inflate(item)

        assertEquals("label", view.textView.text)
        assertEquals("highlight", view.highlightedTextView.text)

        val startImageView = view.findViewById<AppCompatImageView>(R.id.image)
        val highlightImageView = view.findViewById<AppCompatImageView>(R.id.end_image)

        // Highlight should not exist before set
        assertEquals(android.R.drawable.ic_menu_report_image, Shadows.shadowOf(startImageView.drawable).createdFromResId)
        assertFalse(view.highlightedTextView.isVisible)
        assertFalse(highlightImageView.isVisible)

        shouldHighlight = true
        item.invalidate(view)

        // Highlight should now exist
        assertTrue(view.findViewById<TextView>(R.id.highlight_text).isVisible)
        assertFalse(view.findViewById<AppCompatImageView>(R.id.end_image).isVisible)
        assertNull(view.background)
    }

    @Test
    fun `browser menu highlightable item with with no iconTintColorResource must not have a tinted icon`() {
        val item = BrowserMenuHighlightableItem(
            "label",
            startImageResource = android.R.drawable.ic_menu_report_image,
            highlight = BrowserMenuHighlight.HighPriority(
                endImageResource = android.R.drawable.ic_menu_report_image,
                backgroundTint = Color.RED,
            ),
        )

        val view = inflate(item)

        assertEquals("label", view.textView.text)
        assertNull(view.startImageView.drawable!!.colorFilter)
        assertNull(view.endImageView.imageTintList)
    }

    @Test
    fun `bind highlightable item with with default high priority`() {
        val item = BrowserMenuHighlightableItem(
            "label",
            startImageResource = android.R.drawable.ic_menu_report_image,
            highlight = BrowserMenuHighlight.HighPriority(
                backgroundTint = Color.RED,
            ),
        )

        val view = inflate(item)

        assertEquals("label", view.textView.text)
        assertEquals("label", view.highlightedTextView.text)
        assertTrue(view.highlightedTextView.isVisible)
        assertTrue(view.background is ColorDrawable)
        assertNull(view.endImageView.drawable)
    }

    @Suppress("Deprecation")
    @Test
    fun `browser menu highlightable item with with no highlight must not have highlightImageView visible`() {
        val item = BrowserMenuHighlightableItem(
            "label",
            android.R.drawable.ic_menu_report_image,
            highlight = null,
        )

        val view = inflate(item)
        val endImageView = view.findViewById<AppCompatImageView>(R.id.end_image)
        val textView = view.findViewById<TextView>(R.id.text)
        assertEquals("label", textView.text)
        assertEquals(endImageView.visibility, View.GONE)
    }

    @Test
    fun `menu item can be converted to candidate`() {
        val listener = {}

        var shouldHighlight = false
        val highPriorityItem = BrowserMenuHighlightableItem(
            label = "label",
            startImageResource = android.R.drawable.ic_menu_report_image,
            iconTintColorResource = android.R.color.black,
            textColorResource = android.R.color.black,
            highlight = BrowserMenuHighlight.HighPriority(
                endImageResource = android.R.drawable.ic_menu_add,
                backgroundTint = Color.RED,
                label = "highlight",
            ),
            isHighlighted = { shouldHighlight },
            listener = listener,
        )

        assertEquals(
            TextMenuCandidate(
                "label",
                start = DrawableMenuIcon(
                    null,
                    tint = getColor(testContext, android.R.color.black),
                ),
                textStyle = TextStyle(
                    color = getColor(testContext, android.R.color.black),
                ),
                onClick = listener,
            ),
            highPriorityItem.asCandidate(testContext).removeDrawables(),
        )

        shouldHighlight = true
        assertEquals(
            TextMenuCandidate(
                "highlight",
                start = DrawableMenuIcon(
                    null,
                    tint = getColor(testContext, android.R.color.black),
                ),
                end = DrawableMenuIcon(null),
                textStyle = TextStyle(
                    color = getColor(testContext, android.R.color.black),
                ),
                effect = HighPriorityHighlightEffect(
                    backgroundTint = Color.RED,
                ),
                onClick = listener,
            ),
            highPriorityItem.asCandidate(testContext).removeDrawables(),
        )

        assertEquals(
            TextMenuCandidate(
                "highlight",
                start = DrawableMenuIcon(
                    null,
                    tint = getColor(testContext, android.R.color.black),
                    effect = LowPriorityHighlightEffect(
                        notificationTint = Color.RED,
                    ),
                ),
                textStyle = TextStyle(
                    color = getColor(testContext, android.R.color.black),
                ),
                onClick = listener,
            ),
            BrowserMenuHighlightableItem(
                label = "label",
                startImageResource = android.R.drawable.ic_menu_report_image,
                iconTintColorResource = android.R.color.black,
                textColorResource = android.R.color.black,
                highlight = BrowserMenuHighlight.LowPriority(
                    notificationTint = Color.RED,
                    label = "highlight",
                ),
                isHighlighted = { true },
                listener = listener,
            ).asCandidate(testContext).removeDrawables(),
        )
    }

    private fun inflate(item: BrowserMenuHighlightableItem): ConstraintLayout {
        val view = LayoutInflater.from(testContext).inflate(item.getLayoutResource(), null)
        val mockMenu = mock(BrowserMenu::class.java)
        item.bind(mockMenu, view)
        return view as ConstraintLayout
    }

    private val ConstraintLayout.startImageView: ImageView get() = findViewById(R.id.image)
    private val ConstraintLayout.endImageView: ImageView get() = findViewById(R.id.end_image)
    private val ConstraintLayout.textView: TextView get() = findViewById(R.id.text)
    private val ConstraintLayout.highlightedTextView: TextView get() = findViewById(R.id.highlight_text)

    private fun TextMenuCandidate.removeDrawables() = copy(
        start = (start as? DrawableMenuIcon)?.copy(drawable = null),
        end = (end as? DrawableMenuIcon)?.copy(drawable = null),
    )
}
