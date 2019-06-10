/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

package mozilla.components.browser.menu.item

import android.view.LayoutInflater
import android.widget.ImageButton
import android.widget.LinearLayout
import android.widget.TextView
import androidx.appcompat.widget.AppCompatImageView
import androidx.test.ext.junit.runners.AndroidJUnit4
import mozilla.components.browser.menu.BrowserMenu
import mozilla.components.browser.menu.R
import mozilla.components.support.test.robolectric.testContext
import org.junit.Assert.assertEquals
import org.junit.Assert.assertFalse
import org.junit.Assert.assertNotNull
import org.junit.Assert.assertTrue
import org.junit.Test
import org.junit.runner.RunWith
import org.mockito.Mockito.mock
import org.mockito.Mockito.never
import org.mockito.Mockito.reset
import org.mockito.Mockito.verify

@RunWith(AndroidJUnit4::class)
class BrowserMenuItemToolbarTest {

    @Test
    fun `toolbar is visible by default`() {
        val toolbar = BrowserMenuItemToolbar(emptyList())

        assertTrue(toolbar.visible())
    }

    @Test
    fun `layout resource can be inflated`() {
        val toolbar = BrowserMenuItemToolbar(emptyList())

        val view = LayoutInflater.from(testContext)
            .inflate(toolbar.getLayoutResource(), null)

        assertNotNull(view)
    }

    @Test
    fun `empty toolbar does not add anything to view group`() {
        val layout = LinearLayout(testContext)

        val menu = mock(BrowserMenu::class.java)

        val toolbar = BrowserMenuItemToolbar(emptyList())
        toolbar.bind(menu, layout)

        assertEquals(0, layout.childCount)
    }

    @Test
    fun `toolbar removes previously existing child views from view group`() {
        val layout = LinearLayout(testContext)
        layout.addView(TextView(testContext))
        layout.addView(TextView(testContext))

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
                "Button01"
            ) {},
            BrowserMenuItemToolbar.Button(
                R.drawable.abc_ic_ab_back_material,
                "Button02"
            ) {},
            BrowserMenuItemToolbar.TwoStateButton(
                primaryImageResource = R.drawable.abc_ic_go_search_api_material,
                primaryContentDescription = "TwoStatePrimary",
                secondaryImageResource = R.drawable.abc_ic_clear_material,
                secondaryContentDescription = "TwoStateSecondary"
            ) {}
        )

        val menu = mock(BrowserMenu::class.java)
        val layout = LinearLayout(testContext)

        val toolbar = BrowserMenuItemToolbar(buttons)
        toolbar.bind(menu, layout)

        assertEquals(3, layout.childCount)

        val child1 = layout.getChildAt(0)
        val child2 = layout.getChildAt(1)
        val child3 = layout.getChildAt(2)

        assertTrue(child1 is ImageButton)
        assertTrue(child2 is ImageButton)
        assertTrue(child3 is ImageButton)

        assertEquals("Button01", child1.contentDescription)
        assertEquals("Button02", child2.contentDescription)
        assertEquals("TwoStatePrimary", child3.contentDescription)
    }

    @Test
    fun `Disabled Button is not enabled`() {
        val buttons = listOf(
            BrowserMenuItemToolbar.Button(
                imageResource = R.drawable.abc_ic_go_search_api_material,
                contentDescription = "Button01",
                iconTintColorResource = R.color.accent_material_light,
                isEnabled = { false }
            ) {}
        )

        val menu = mock(BrowserMenu::class.java)
        val layout = LinearLayout(testContext)

        val toolbar = BrowserMenuItemToolbar(buttons)
        toolbar.bind(menu, layout)

        val child1 = layout.getChildAt(0)
        assertEquals("Button01", child1.contentDescription)
        assertFalse(child1.isEnabled)
    }

    @Test
    fun `Button redraws when invalidate is triggered`() {
        var isEnabled = false
        val buttons = listOf(
            BrowserMenuItemToolbar.Button(
                imageResource = R.drawable.abc_ic_go_search_api_material,
                contentDescription = "Button01",
                isEnabled = { isEnabled }
            ) {}
        )

        val menu = mock(BrowserMenu::class.java)
        val layout = LinearLayout(testContext)

        val toolbar = BrowserMenuItemToolbar(buttons)
        toolbar.bind(menu, layout)

        val child1 = layout.getChildAt(0)
        assertEquals("Button01", child1.contentDescription)
        assertFalse(child1.isEnabled)

        isEnabled = true
        toolbar.invalidate(layout)
        assertTrue(child1.isEnabled)
    }

    @Test
    fun `Disabled TwoState Button in secondary state is disabled`() {
        val buttons = listOf(
            BrowserMenuItemToolbar.TwoStateButton(
                primaryImageResource = R.drawable.abc_ic_go_search_api_material,
                primaryContentDescription = "TwoStateEnabled",
                secondaryImageResource = R.drawable.abc_ic_clear_material,
                secondaryContentDescription = "TwoStateDisabled",
                isInPrimaryState = { false },
                disableInSecondaryState = true
            ) {}
        )

        val menu = mock(BrowserMenu::class.java)
        val layout = LinearLayout(testContext)

        val toolbar = BrowserMenuItemToolbar(buttons)
        toolbar.bind(menu, layout)

        val child1 = layout.getChildAt(0)
        assertEquals("TwoStateDisabled", child1.contentDescription)
        assertFalse(child1.isEnabled)
    }

    @Test
    fun `TwoStateButton has primary and secondary state invoked`() {
        val primaryResource = R.drawable.abc_ic_go_search_api_material
        val secondaryResource = R.drawable.abc_ic_clear_material

        var reloadPageAction = BrowserMenuItemToolbar.TwoStateButton(
            primaryImageResource = primaryResource,
            primaryContentDescription = "primary",
            primaryImageTintResource = R.color.accent_material_dark,
            secondaryImageResource = secondaryResource,
            secondaryContentDescription = "secondary",
            secondaryImageTintResource = R.color.accent_material_light
        ) {}
        assertTrue(reloadPageAction.isInPrimaryState.invoke())

        reloadPageAction = BrowserMenuItemToolbar.TwoStateButton(
            primaryImageResource = primaryResource,
            primaryContentDescription = "primary",
            primaryImageTintResource = R.color.accent_material_dark,
            secondaryImageResource = secondaryResource,
            secondaryContentDescription = "secondary",
            secondaryImageTintResource = R.color.accent_material_light,
            isInPrimaryState = { false }
        ) {}
        assertFalse(reloadPageAction.isInPrimaryState.invoke())
    }

    @Test
    fun `TwoStateButton redraws when invalidate is triggered`() {
        var isInPrimaryState = true
        val buttons = listOf(
            BrowserMenuItemToolbar.TwoStateButton(
                primaryImageResource = R.drawable.abc_ic_go_search_api_material,
                primaryContentDescription = "TwoStateEnabled",
                secondaryImageResource = R.drawable.abc_ic_clear_material,
                secondaryContentDescription = "TwoStateDisabled",
                isInPrimaryState = { isInPrimaryState }
            ) {}
        )

        val menu = mock(BrowserMenu::class.java)
        val layout = LinearLayout(testContext)

        val toolbar = BrowserMenuItemToolbar(buttons)
        toolbar.bind(menu, layout)

        val child1 = layout.getChildAt(0)
        assertEquals("TwoStateEnabled", child1.contentDescription)

        isInPrimaryState = false
        toolbar.invalidate(layout)
        assertEquals("TwoStateDisabled", child1.contentDescription)
    }

    @Test
    fun `TwoStateButton doesn't redraw if state hasn't changed`() {
        val isInPrimaryState = true
        val button = BrowserMenuItemToolbar.TwoStateButton(
            primaryImageResource = R.drawable.abc_ic_go_search_api_material,
            primaryContentDescription = "TwoStateEnabled",
            secondaryImageResource = R.drawable.abc_ic_clear_material,
            secondaryContentDescription = "TwoStateDisabled",
            isInPrimaryState = { isInPrimaryState },
            disableInSecondaryState = true
        ) {}

        val view = mock(AppCompatImageView::class.java)

        button.bind(view)
        verify(view).contentDescription = "TwoStateEnabled"

        reset(view)

        button.invalidate(view)
        verify(view, never()).contentDescription = "TwoStateEnabled"
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
        val layout = LinearLayout(testContext)

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
