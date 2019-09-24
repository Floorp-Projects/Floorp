/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

package mozilla.components.browser.menu.item

import mozilla.components.browser.menu.R
import org.junit.Assert.assertEquals
import org.junit.Test

class BrowserMenuSwitchTest {

    @Test
    fun `browser checkbox uses correct layout`() {
        val item = BrowserMenuSwitch("Hello") {}
        assertEquals(R.layout.mozac_browser_menu_item_switch, item.getLayoutResource())
    }
}
