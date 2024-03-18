/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

package mozilla.components.browser.menu.item

import androidx.test.ext.junit.runners.AndroidJUnit4
import mozilla.components.browser.menu.R
import mozilla.components.concept.menu.candidate.CompoundMenuCandidate
import mozilla.components.concept.menu.candidate.DrawableMenuIcon
import mozilla.components.support.test.robolectric.testContext
import org.junit.Assert.assertEquals
import org.junit.Before
import org.junit.Test
import org.junit.runner.RunWith

@RunWith(AndroidJUnit4::class)
class BrowserMenuImageSwitchTest {
    private lateinit var menuItem: BrowserMenuImageSwitch
    private val label = "test"
    private val icon = android.R.drawable.ic_menu_call

    @Before
    fun setup() {
        menuItem = BrowserMenuImageSwitch(icon, label) {}
    }

    @Test
    fun `menu item uses correct layout`() {
        assertEquals(R.layout.mozac_browser_menu_item_image_switch, menuItem.getLayoutResource())
    }

    @Test
    fun `menu item  has correct label`() {
        assertEquals(label, menuItem.label)
    }

    @Test
    fun `menu item  has correct icon`() {
        assertEquals(icon, menuItem.imageResource)
    }

    @Test
    fun `menu switch can be converted to candidate`() {
        val listener = { _: Boolean -> }

        assertEquals(
            CompoundMenuCandidate(
                label,
                isChecked = false,
                start = DrawableMenuIcon(null),
                end = CompoundMenuCandidate.ButtonType.SWITCH,
                onCheckedChange = listener,
            ),
            BrowserMenuImageSwitch(icon, label, listener = listener).asCandidate(testContext).run {
                copy(start = (start as DrawableMenuIcon?)?.copy(drawable = null))
            },
        )
    }
}
