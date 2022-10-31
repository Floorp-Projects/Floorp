/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

package mozilla.components.ui.widgets

import android.graphics.drawable.Drawable
import android.widget.ImageButton
import android.widget.TextView
import androidx.appcompat.view.ContextThemeWrapper
import androidx.core.view.isGone
import androidx.core.view.isVisible
import androidx.test.ext.junit.runners.AndroidJUnit4
import mozilla.components.support.test.mock
import mozilla.components.support.test.robolectric.testContext
import org.junit.Assert.assertEquals
import org.junit.Assert.assertNotNull
import org.junit.Assert.assertTrue
import org.junit.Before
import org.junit.Test
import org.junit.runner.RunWith

@RunWith(AndroidJUnit4::class)
class WidgetSiteItemViewTest {

    private lateinit var view: WidgetSiteItemView

    @Before
    fun setup() {
        val context = ContextThemeWrapper(testContext, R.style.Mozac_Widgets_TestTheme)
        view = WidgetSiteItemView(context)
    }

    @Test
    fun `setText hides the caption`() {
        val labelView = view.findViewById<TextView>(R.id.label)
        val captionView = view.findViewById<TextView>(R.id.caption)

        view.setText(label = "label", caption = null)
        assertEquals("label", labelView.text)
        assertTrue(captionView.isGone)

        view.setText(label = "Label", caption = "")
        assertEquals("Label", labelView.text)
        assertEquals("", captionView.text)
        assertTrue(captionView.isVisible)
    }

    @Test
    fun `setSecondaryButton shows the button`() {
        val secondaryButton = view.findViewById<ImageButton>(R.id.secondary_button)
        val drawable = mock<Drawable>()
        var clicked = false
        view.setSecondaryButton(
            icon = drawable,
            contentDescription = "Menu",
            onClickListener = { clicked = true },
        )
        assertTrue(secondaryButton.isVisible)
        assertEquals(drawable, secondaryButton.drawable)
        assertEquals("Menu", secondaryButton.contentDescription)

        secondaryButton.performClick()
        assertTrue(clicked)
    }

    @Test
    fun `setSecondaryButton with resource IDs shows the button`() {
        val secondaryButton = view.findViewById<ImageButton>(R.id.secondary_button)
        var clicked = false
        view.setSecondaryButton(
            icon = R.drawable.mozac_ic_lock,
            contentDescription = R.string.mozac_error_lock,
            onClickListener = { clicked = true },
        )
        assertTrue(secondaryButton.isVisible)
        assertNotNull(secondaryButton.drawable)
        assertEquals("mozac_error_lock", secondaryButton.contentDescription)

        secondaryButton.performClick()
        assertTrue(clicked)
    }

    @Test
    fun `removeSecondaryButton does nothing if set was not called`() {
        val secondaryButton = view.findViewById<ImageButton>(R.id.secondary_button)
        assertTrue(secondaryButton.isGone)

        view.removeSecondaryButton()
        assertTrue(secondaryButton.isGone)
    }
}
