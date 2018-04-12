/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

package mozilla.components.session

import mozilla.components.session.helper.mock
import org.junit.Assert.assertEquals
import org.junit.Test
import org.mockito.Mockito.never
import org.mockito.Mockito.verify
import org.mockito.Mockito.verifyNoMoreInteractions

class SessionManagerTest {
    @Test
    fun `SessionProvider will be used to build initial list of sessions and selection`() {
        val sessions = listOf(
                Session("http://www.mozilla.org"),
                Session("http://www.firefox.com"))

        val provider = object : SessionProvider {
            override fun getInitialSessions() = Pair(sessions, 0)
        }

        val manager = SessionManager(provider)

        assertEquals(2, manager.size)
        assertEquals("http://www.mozilla.org", manager.selectedSession.url)
    }

    @Test
    fun `session can be selected`() {
        val sessions = listOf(
                Session("http://www.mozilla.org"),
                Session("http://www.firefox.com"))

        val provider = object : SessionProvider {
            override fun getInitialSessions() = Pair(sessions, 0)
        }

        val manager = SessionManager(provider)

        assertEquals("http://www.mozilla.org", manager.selectedSession.url)

        manager.select(sessions[1])

        assertEquals("http://www.firefox.com", manager.selectedSession.url)
    }

    @Test
    fun `observer gets notified when session gets selected`() {
        val sessions = listOf(
                Session("http://www.mozilla.org"),
                Session("http://www.firefox.com"))

        val provider = object : SessionProvider {
            override fun getInitialSessions() = Pair(sessions, 0)
        }

        val manager = SessionManager(provider)

        val observer: SessionManager.Observer = mock()
        manager.register(observer)

        manager.select(sessions[1])

        verify(observer).onSessionSelected(sessions[1])
    }

    @Test(expected = IllegalArgumentException::class)
    fun `manager throws exception if unknown session is selected`() {
        val sessions = listOf(
                Session("http://www.mozilla.org"),
                Session("http://www.firefox.com"))

        val provider = object : SessionProvider {
            override fun getInitialSessions() = Pair(sessions, 0)
        }

        val manager = SessionManager(provider)

        manager.select(Session("https://getpocket.com"))
    }

    @Test
    fun `observer does not get notified after unregistering`() {
        val sessions = listOf(
                Session("http://www.mozilla.org"),
                Session("http://www.firefox.com"))

        val provider = object : SessionProvider {
            override fun getInitialSessions() = Pair(sessions, 0)
        }

        val manager = SessionManager(provider)

        val observer: SessionManager.Observer = mock()
        manager.register(observer)

        manager.select(sessions[1])

        verify(observer).onSessionSelected(sessions[1])
        verifyNoMoreInteractions(observer)

        manager.unregister(observer)

        manager.select(sessions[0])

        verify(observer, never()).onSessionSelected(sessions[0])
        verifyNoMoreInteractions(observer)
    }
}
