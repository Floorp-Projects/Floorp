/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

package mozilla.components.browser.menu.item

import android.view.View
import mozilla.components.browser.menu.BrowserMenu
import mozilla.components.browser.menu.R
import org.junit.Assert.assertEquals
import org.junit.Assert.assertTrue
import org.junit.Test
import org.mockito.Mockito.mock

class BrowserMenuDividerTest {

    @Test
    fun `browser divider uses correct layout`() {
        val item = BrowserMenuDivider()
        assertEquals(R.layout.mozac_browser_menu_item_divider, item.getLayoutResource())
    }

    @Test
    fun `hitting default methods`() {
        val item = BrowserMenuDivider()
        assertTrue(item.visible())
        item.bind(mock(BrowserMenu::class.java), mock(View::class.java))
        item.invalidate(mock(View::class.java))
    }
}
