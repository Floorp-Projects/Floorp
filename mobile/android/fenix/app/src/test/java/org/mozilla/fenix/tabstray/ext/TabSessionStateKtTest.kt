/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

package org.mozilla.fenix.tabstray.ext

import io.mockk.every
import io.mockk.mockk
import mozilla.components.browser.state.state.ContentState
import mozilla.components.browser.state.state.TabSessionState
import org.junit.Assert.assertFalse
import org.junit.Assert.assertTrue
import org.junit.Test
import org.mozilla.fenix.tabstray.browser.BrowserTrayList.BrowserTabType.NORMAL
import org.mozilla.fenix.tabstray.browser.BrowserTrayList.BrowserTabType.PRIVATE

class TabSessionStateKtTest {

    @Test
    fun `WHEN configuration is private THEN return true`() {
        val contentState = mockk<ContentState>()
        val state = TabSessionState(content = contentState)
        val config = PRIVATE

        every { contentState.private } returns true

        assertTrue(state.filterFromConfig(config))
    }

    @Test
    fun `WHEN configuration is normal THEN return false`() {
        val contentState = mockk<ContentState>()
        val state = TabSessionState(content = contentState)
        val config = NORMAL

        every { contentState.private } returns false

        assertTrue(state.filterFromConfig(config))
    }

    @Test
    fun `WHEN configuration does not match THEN return false`() {
        val contentState = mockk<ContentState>()
        val state = TabSessionState(content = contentState)
        val config = NORMAL

        every { contentState.private } returns true

        assertFalse(state.filterFromConfig(config))

        val config2 = PRIVATE

        every { contentState.private } returns false

        assertFalse(state.filterFromConfig(config2))
    }
}
