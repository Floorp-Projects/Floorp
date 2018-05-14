/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

package mozilla.components.browser.session

import mozilla.components.browser.session.helper.mock
import org.junit.Assert.assertEquals
import org.junit.Test
import org.mockito.Mockito.never
import org.mockito.Mockito.verify
import org.mockito.Mockito.verifyNoMoreInteractions

class SessionManagerTest {
    @Test
    fun `session can be added`() {
        val manager = SessionManager(Session("http://www.mozilla.org"))
        manager.add(Session("http://getpocket.com"))
        manager.add(Session("http://www.firefox.com"), true)

        assertEquals(3, manager.size)
        assertEquals("http://www.firefox.com", manager.selectedSession.url)
    }

    @Test
    fun `session can be selected`() {
        val session1 = Session("http://www.mozilla.org")
        val session2 = Session("http://www.firefox.com")

        val manager = SessionManager(session1)
        manager.add(session2)

        assertEquals("http://www.mozilla.org", manager.selectedSession.url)
        manager.select(session2)
        assertEquals("http://www.firefox.com", manager.selectedSession.url)
    }

    @Test
    fun `observer gets notified when session gets selected`() {
        val session1 = Session("http://www.mozilla.org")
        val session2 = Session("http://www.firefox.com")

        val manager = SessionManager(session1)
        manager.add(session2)

        val observer: SessionManager.Observer = mock()
        manager.register(observer)

        manager.select(session2)

        verify(observer).onSessionSelected(session2)
    }

    @Test(expected = IllegalArgumentException::class)
    fun `manager throws exception if unknown session is selected`() {
        val manager = SessionManager(Session("http://www.mozilla.org"))

        manager.select(Session("https://getpocket.com"))
    }

    @Test
    fun `observer does not get notified after unregistering`() {
        val session1 = Session("http://www.mozilla.org")
        val session2 = Session("http://www.firefox.com")

        val manager = SessionManager(session1)
        manager.add(session2)

        val observer: SessionManager.Observer = mock()
        manager.register(observer)

        manager.select(session2)

        verify(observer).onSessionSelected(session2)
        verifyNoMoreInteractions(observer)

        manager.unregister(observer)

        manager.select(session1)

        verify(observer, never()).onSessionSelected(session1)
        verifyNoMoreInteractions(observer)
    }
}
