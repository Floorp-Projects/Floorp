/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

package org.mozilla.fenix.home

import io.mockk.every
import io.mockk.mockk
import org.junit.Assert.assertEquals
import org.junit.Before
import org.junit.Test
import org.mozilla.fenix.browser.browsingmode.BrowsingMode
import org.mozilla.fenix.browser.browsingmode.BrowsingModeManager

class ModeTest {

    private lateinit var browsingModeManager: BrowsingModeManager

    @Before
    fun setup() {
        browsingModeManager = mockk(relaxed = true)
    }

    @Test
    fun `WHEN browsing mode is normal THEN return normal mode`() {
        every { browsingModeManager.mode } returns BrowsingMode.Normal

        assertEquals(Mode.Normal, Mode.fromBrowsingMode(browsingModeManager.mode))
    }

    @Test
    fun `WHEN browsing mode is private THEN return private mode`() {
        every { browsingModeManager.mode } returns BrowsingMode.Private

        assertEquals(Mode.Private, Mode.fromBrowsingMode(browsingModeManager.mode))
    }
}
