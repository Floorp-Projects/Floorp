/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

package mozilla.components.browser.session.ext

import androidx.test.ext.junit.runners.AndroidJUnit4
import mozilla.components.browser.session.Session
import mozilla.components.browser.state.state.CustomTabConfig
import mozilla.components.support.test.mock
import org.junit.Assert.assertEquals
import org.junit.Assert.assertSame
import org.junit.Test
import org.junit.runner.RunWith

@RunWith(AndroidJUnit4::class)
class SessionExtensionsTest {

    @Test
    fun `toTabSessionState - Can convert tab session with parent tab`() {
        val session = Session("https://mozilla.org")
        session.parentId = "session"

        val tabState = session.toTabSessionState()
        assertEquals(tabState.id, session.id)
        assertEquals(tabState.content.url, session.url)
        assertEquals(tabState.parentId, session.parentId)
    }

    @Test
    fun `toCustomTabSessionState - Can convert custom tab session`() {
        val session = Session("https://mozilla.org")
        session.customTabConfig = CustomTabConfig()

        val customTabState = session.toCustomTabSessionState()
        assertEquals(customTabState.id, session.id)
        assertEquals(customTabState.content.url, session.url)
        assertSame(customTabState.config, session.customTabConfig)
    }

    @Test(expected = IllegalStateException::class)
    fun `toCustomTabSessionState - Throws exception when converting a non-custom tab session`() {
        val session: Session = mock()
        session.toCustomTabSessionState()
    }
}
