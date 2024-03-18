/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

package mozilla.components.browser.menu.item

import mozilla.components.browser.menu.R
import mozilla.components.concept.menu.candidate.ContainerStyle
import mozilla.components.concept.menu.candidate.DividerMenuCandidate
import mozilla.components.support.test.mock
import org.junit.Assert.assertEquals
import org.junit.Assert.assertTrue
import org.junit.Test

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
        item.bind(mock(), mock())
        item.invalidate(mock())
    }

    @Test
    fun `menu divider can be converted to candidate`() {
        assertEquals(
            DividerMenuCandidate(),
            BrowserMenuDivider().asCandidate(mock()),
        )

        assertEquals(
            DividerMenuCandidate(
                containerStyle = ContainerStyle(isVisible = true),
            ),
            BrowserMenuDivider().apply {
                visible = { true }
            }.asCandidate(mock()),
        )
    }
}
