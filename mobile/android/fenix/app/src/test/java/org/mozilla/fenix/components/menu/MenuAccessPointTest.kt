/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

package org.mozilla.fenix.components.menu

import org.junit.Assert.assertEquals
import org.junit.Test
import org.mozilla.fenix.components.accounts.FenixFxAEntryPoint

class MenuAccessPointTest {

    @Test
    fun `WHEN menu access points are converted to Fenix FxA entry points THEN return the correct entry points`() {
        assertEquals(
            FenixFxAEntryPoint.BrowserToolbar,
            MenuAccessPoint.Browser.toFenixFxAEntryPoint(),
        )
        assertEquals(FenixFxAEntryPoint.Unknown, MenuAccessPoint.External.toFenixFxAEntryPoint())
        assertEquals(FenixFxAEntryPoint.HomeMenu, MenuAccessPoint.Home.toFenixFxAEntryPoint())
    }
}
