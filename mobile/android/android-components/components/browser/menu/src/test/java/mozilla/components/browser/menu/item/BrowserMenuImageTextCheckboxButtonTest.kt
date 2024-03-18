/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

package mozilla.components.browser.menu.item

import android.view.LayoutInflater
import android.view.View
import android.widget.CheckBox
import android.widget.ImageView
import android.widget.TextView
import androidx.core.content.ContextCompat
import androidx.core.widget.ImageViewCompat.getImageTintList
import androidx.test.ext.junit.runners.AndroidJUnit4
import mozilla.components.browser.menu.BrowserMenu
import mozilla.components.browser.menu.R
import mozilla.components.support.test.robolectric.testContext
import org.junit.Assert.assertEquals
import org.junit.Assert.assertNotNull
import org.junit.Assert.assertTrue
import org.junit.Before
import org.junit.Test
import org.junit.runner.RunWith
import org.mockito.Mockito.mock
import org.mockito.Mockito.spy
import org.mockito.Mockito.verify

@RunWith(AndroidJUnit4::class)
class BrowserMenuImageTextCheckboxButtonTest {

    private lateinit var item: BrowserMenuImageTextCheckboxButton
    private lateinit var secondaryItem: BrowserMenuImageTextCheckboxButton

    private val label = "Bookmarks"
    private val imageResource = android.R.drawable.ic_menu_report_image
    private val iconTintColorResource = android.R.color.holo_red_dark

    private val tintColorResource = android.R.color.holo_purple
    private val labelListener = { }
    private val primaryLabel = "Add"
    private val secondaryLabel = "Edit"
    private val primaryStateIconResource = android.R.drawable.star_big_off
    private val secondaryStateIconResource = android.R.drawable.star_big_on
    private val isInPrimaryState: () -> Boolean = { true }
    private val onCheckedChangedListener: (Boolean) -> Unit = { }

    @Before
    fun setUp() {
        item = spy(
            BrowserMenuImageTextCheckboxButton(
                imageResource = imageResource,
                label = label,
                iconTintColorResource = iconTintColorResource,
                textColorResource = tintColorResource,
                labelListener = labelListener,
                primaryLabel = primaryLabel,
                secondaryLabel = secondaryLabel,
                primaryStateIconResource = primaryStateIconResource,
                secondaryStateIconResource = secondaryStateIconResource,
                tintColorResource = tintColorResource,
                isInPrimaryState = isInPrimaryState,
                onCheckedChangedListener = onCheckedChangedListener,
            ),
        )

        secondaryItem = spy(
            BrowserMenuImageTextCheckboxButton(
                imageResource = imageResource,
                label = label,
                iconTintColorResource = iconTintColorResource,
                textColorResource = tintColorResource,
                labelListener = labelListener,
                primaryLabel = primaryLabel,
                secondaryLabel = secondaryLabel,
                primaryStateIconResource = primaryStateIconResource,
                secondaryStateIconResource = secondaryStateIconResource,
                tintColorResource = tintColorResource,
                isInPrimaryState = { false },
                onCheckedChangedListener = onCheckedChangedListener,
            ),
        )
    }

    @Test
    fun `layout resource can be inflated`() {
        val view = LayoutInflater.from(testContext)
            .inflate(item.getLayoutResource(), null)

        assertNotNull(view)
    }

    @Test
    fun `item uses correct layout`() {
        assertEquals(R.layout.mozac_browser_menu_item_image_text_checkbox_button, item.getLayoutResource())
    }

    @Test
    fun `item is visible by default`() {
        assertTrue(item.visible())
    }

    @Test
    fun `initialState is invoked on bind and properly sets label`() {
        val menu = mock(BrowserMenu::class.java)
        val view = LayoutInflater.from(testContext)
            .inflate(item.getLayoutResource(), null)

        item.bind(menu, view)
        val checkBox = view.findViewById<CheckBox>(R.id.checkbox)
        var expectedLabel = if (item.isInPrimaryState()) primaryLabel else secondaryLabel

        assertEquals(expectedLabel, primaryLabel)
        assertEquals(expectedLabel, checkBox.text)

        secondaryItem.bind(menu, view)
        val secondaryCheckBox = view.findViewById<CheckBox>(R.id.checkbox)
        expectedLabel = if (secondaryItem.isInPrimaryState()) primaryLabel else secondaryLabel

        assertEquals(expectedLabel, secondaryLabel)
        assertEquals(expectedLabel, secondaryCheckBox.text)
    }

    @Test
    fun `item has the correct text color`() {
        val view = inflate(item)

        val textView = view.findViewById<TextView>(R.id.text)
        val expectedColour = ContextCompat.getColor(view.context, item.textColorResource)

        assertEquals(textView.text, label)
        assertEquals(expectedColour, textView.currentTextColor)
    }

    @Test
    fun `item has icon with correct resource and tint`() {
        val view = inflate(item)

        val icon = view.findViewById<ImageView>(R.id.image)
        val expectedColour = ContextCompat.getColor(view.context, item.iconTintColorResource)

        assertNotNull(icon.drawable)
        assertEquals(getImageTintList(icon)?.defaultColor, expectedColour)
    }

    @Test
    fun `item accessibilityRegion has label text as content description`() {
        val view = inflate(item)

        val accessibilityRegion = view.findViewById<View>(R.id.accessibilityRegion)

        assertEquals(label, accessibilityRegion.contentDescription)
    }

    @Test
    fun `item accessibilityRegion is clickable`() {
        val view = inflate(item)

        val accessibilityRegion = view.findViewById<View>(R.id.accessibilityRegion)

        assertTrue(accessibilityRegion.isClickable)
        assertTrue(accessibilityRegion.callOnClick())
    }

    @Test
    fun `clicking item dismisses menu`() {
        val view = inflate(item)
        val menu = mock(BrowserMenu::class.java)

        item.bind(menu, view)
        view.callOnClick()

        verify(menu).dismiss()
    }

    @Test
    fun `clicking checkbox dismisses menu`() {
        val view = inflate(item)
        val menu = mock(BrowserMenu::class.java)

        item.bind(menu, view)
        val checkBox = view.findViewById<CheckBox>(R.id.checkbox)

        checkBox.performClick()

        verify(menu).dismiss()
    }

    @Test
    fun `item checkbox has text with correct tint`() {
        val view = inflate(item)

        val checkbox = view.findViewById<CheckBox>(R.id.checkbox)
        val expectedColour = ContextCompat.getColor(view.context, tintColorResource)

        assertEquals(expectedColour, checkbox.currentTextColor)
    }

    private fun inflate(item: BrowserMenuImageText): View {
        val view = LayoutInflater.from(testContext).inflate(item.getLayoutResource(), null)
        val mockMenu = mock(BrowserMenu::class.java)
        item.bind(mockMenu, view)
        return view
    }
}
