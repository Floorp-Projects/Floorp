/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

package mozilla.components.browser.menu2

import org.junit.Test

class MenuControllerTest {

    @Test
    fun `can construct without errors`() {
        MenuController(Side.END)
    }
}
