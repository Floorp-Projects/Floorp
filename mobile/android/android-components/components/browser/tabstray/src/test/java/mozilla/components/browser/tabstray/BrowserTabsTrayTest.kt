/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

package mozilla.components.browser.tabstray

import mozilla.components.browser.session.Session
import mozilla.components.support.test.mock
import org.junit.Assert.assertEquals
import org.junit.Test
import org.junit.runner.RunWith
import org.mockito.Mockito.verify
import org.robolectric.RobolectricTestRunner
import org.robolectric.RuntimeEnvironment
import org.robolectric.Shadows

@RunWith(RobolectricTestRunner::class)
class BrowserTabsTrayTest {
    @Test
    fun `holders will unsubscribe if view gets detached`() {
        val adapter: TabsAdapter = mock()
        val tabsTray = BrowserTabsTray(RuntimeEnvironment.application, tabsAdapter = adapter)

        val shadow = Shadows.shadowOf(tabsTray)
        shadow.callOnDetachedFromWindow()

        verify(adapter).unsubscribeHolders()
    }

    @Test
    fun `TabsTray concept methods are forwarded to adapter`() {
        val adapter: TabsAdapter = mock()
        val tabsTray = BrowserTabsTray(RuntimeEnvironment.application, tabsAdapter = adapter)

        val sessions = listOf<Session>()

        tabsTray.displaySessions(sessions, -1)
        verify(adapter).displaySessions(sessions, -1)

        tabsTray.updateSessions(sessions, -2)
        verify(adapter).updateSessions(sessions, -2)

        tabsTray.onSessionsInserted(2, 5)
        verify(adapter).onSessionsInserted(2, 5)

        tabsTray.onSessionsRemoved(4, 1)
        verify(adapter).onSessionsRemoved(4, 1)

        tabsTray.onSessionMoved(7, 1)
        verify(adapter).onSessionMoved(7, 1)

        tabsTray.onSessionsChanged(0, 1)
        verify(adapter).onSessionsChanged(0, 1)
    }

    @Test
    fun `TabsTray is set on adapter`() {
        val adapter: TabsAdapter = TabsAdapter()
        val tabsTray = BrowserTabsTray(RuntimeEnvironment.application, tabsAdapter = adapter)

        assertEquals(tabsTray, adapter.tabsTray)
    }
}
