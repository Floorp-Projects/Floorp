/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

package mozilla.components.browser.menu.item

import android.content.Context
import android.view.LayoutInflater
import android.view.View
import android.widget.TextView
import androidx.test.core.app.ApplicationProvider
import androidx.test.ext.junit.runners.AndroidJUnit4
import mozilla.components.browser.menu.BrowserMenu
import mozilla.components.browser.menu.R
import org.junit.Assert.assertEquals
import org.junit.Assert.assertTrue
import org.junit.Before
import org.junit.Test
import org.junit.runner.RunWith
import org.mockito.Mockito.mock

@RunWith(AndroidJUnit4::class)
class TwoStateBrowserMenuImageTextTest {

    private val context: Context get() = ApplicationProvider.getApplicationContext()
    private lateinit var menuItemPrimary: TwoStateBrowserMenuImageText
    private lateinit var menuItemSecondary: TwoStateBrowserMenuImageText

    private var primaryPressed = false
    private var secondaryPressed = false

    private val primaryLabel: String = "primaryLabel"
    private val secondaryLabel: String = "secondaryLabel"

    @Before
    fun setup() {
        menuItemPrimary = TwoStateBrowserMenuImageText(
            primaryLabel = primaryLabel,
            secondaryLabel = secondaryLabel,
            primaryStateIconResource = android.R.drawable.ic_delete,
            secondaryStateIconResource = android.R.drawable.ic_input_add,
            isInPrimaryState = { true },
            primaryStateAction = { primaryPressed = true },
        )

        menuItemSecondary = TwoStateBrowserMenuImageText(
            primaryLabel = primaryLabel,
            secondaryLabel = secondaryLabel,
            primaryStateIconResource = android.R.drawable.ic_delete,
            secondaryStateIconResource = android.R.drawable.ic_input_add,
            isInPrimaryState = { false },
            isInSecondaryState = { true },
            secondaryStateAction = { secondaryPressed = true },
        )
    }

    @Test
    fun `browser menu should be inflated`() {
        val view = inflate(menuItemPrimary)
        view.performClick()
        assertTrue(primaryPressed)

        val secondView = inflate(menuItemSecondary)
        secondView.performClick()
        assertTrue(secondaryPressed)
    }

    @Test
    fun `browser menu should have the right text`() {
        val view = inflate(menuItemPrimary)
        val textView = view.findViewById<TextView>(R.id.text)

        assertEquals(textView.text, primaryLabel)

        val secondView = inflate(menuItemSecondary)
        val secondTextView = secondView.findViewById<TextView>(R.id.text)

        assertEquals(secondTextView.text, secondaryLabel)
    }

    private fun inflate(item: BrowserMenuImageText): View {
        val view = LayoutInflater.from(context).inflate(item.getLayoutResource(), null)
        val mockMenu = mock(BrowserMenu::class.java)
        item.bind(mockMenu, view)
        return view
    }
}
