/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

package mozilla.components.browser.session

import mozilla.components.concept.engine.Engine
import mozilla.components.concept.engine.EngineSession
import mozilla.components.support.test.mock
import org.junit.Assert.assertEquals
import org.junit.Assert.assertNotNull
import org.junit.Assert.assertNull
import org.junit.Assert.assertTrue
import org.junit.Test
import org.mockito.Mockito.doReturn
import org.mockito.Mockito.never
import org.mockito.Mockito.verify
import org.mockito.Mockito.verifyNoMoreInteractions

class SessionManagerTest {
    @Test
    fun `session can be added`() {
        val manager = SessionManager(mock())
        manager.add(Session("http://getpocket.com"))
        manager.add(Session("http://www.firefox.com"), true)

        assertEquals(2, manager.size)
        assertEquals("http://www.firefox.com", manager.selectedSession.url)
    }

    @Test
    fun `session can be selected`() {
        val session1 = Session("http://www.mozilla.org")
        val session2 = Session("http://www.firefox.com")

        val manager = SessionManager(mock())
        manager.add(session1)
        manager.add(session2)

        assertEquals("http://www.mozilla.org", manager.selectedSession.url)
        manager.select(session2)
        assertEquals("http://www.firefox.com", manager.selectedSession.url)
    }

    @Test
    fun `observer gets notified when session gets selected`() {
        val session1 = Session("http://www.mozilla.org")
        val session2 = Session("http://www.firefox.com")

        val manager = SessionManager(mock())
        manager.add(session1)
        manager.add(session2)

        val observer: SessionManager.Observer = mock()
        manager.register(observer)

        manager.select(session2)

        verify(observer).onSessionSelected(session2)
    }

    @Test(expected = IllegalArgumentException::class)
    fun `manager throws exception if unknown session is selected`() {
        val manager = SessionManager(mock())
        manager.add(Session("http://www.mozilla.org"))

        manager.select(Session("https://getpocket.com"))
    }

    @Test
    fun `observer does not get notified after unregistering`() {
        val session1 = Session("http://www.mozilla.org")
        val session2 = Session("http://www.firefox.com")

        val manager = SessionManager(mock())
        manager.add(session1)
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

    @Test
    fun `observer is called when session is added`() {
        val manager = SessionManager(mock())
        val session = Session("https://www.mozilla.org")

        val observer: SessionManager.Observer = mock()
        manager.register(observer)

        manager.add(session)

        verify(observer).onSessionAdded(session)
        verify(observer).onSessionSelected(session) // First session is selected automatically
        verifyNoMoreInteractions(observer)
    }

    @Test
    fun `observer is called when session is removed`() {
        val manager = SessionManager(mock())
        val session1 = Session("https://www.mozilla.org")
        val session2 = Session("https://www.firefox.com")

        manager.add(session1)
        manager.add(session2)

        val observer: SessionManager.Observer = mock()
        manager.register(observer)

        manager.remove(session1)

        verify(observer).onSessionRemoved(session1)
        verifyNoMoreInteractions(observer)
    }

    @Test
    fun `observer is not called when session to remove is not in list`() {
        val manager = SessionManager(mock())
        val session1 = Session("https://www.mozilla.org")
        val session2 = Session("https://www.firefox.com")

        manager.add(session1)

        val observer: SessionManager.Observer = mock()
        manager.register(observer)

        manager.remove(session2)

        verifyNoMoreInteractions(observer)
    }

    @Test
    fun `initial session is selected`() {
        val session = Session("https://www.mozilla.org")

        val manager = SessionManager(mock())
        manager.add(session)

        assertEquals(1, manager.size)
        assertEquals(session, manager.selectedSession)
    }

    @Test
    fun `manager can have no session`() {
        val manager = SessionManager(mock())

        assertEquals(0, manager.size)
    }

    @Test(expected = IllegalStateException::class)
    fun `exception is thrown if selected session is selected with no selection`() {
        val manager = SessionManager(mock())
        manager.selectedSession
    }

    @Test
    fun `selected session will be recalculated when selected session gets removed`() {
        val manager = SessionManager(mock())

        val session1 = Session("https://www.mozilla.org")
        val session2 = Session("https://www.firefox.com")
        val session3 = Session("https://wiki.mozilla.org")
        val session4 = Session("https://github.com/mozilla-mobile/android-components")

        manager.add(session1)
        manager.add(session2)
        manager.add(session3)
        manager.add(session4)

        // (1), 2, 3, 4
        assertEquals(session1, manager.selectedSession)

        // 1, 2, 3, (4)
        manager.select(session4)
        assertEquals(session4, manager.selectedSession)

        // 1, 2, (3)
        manager.remove(session4)
        assertEquals(session3, manager.selectedSession)

        // 2, (3)
        manager.remove(session1)
        assertEquals(session3, manager.selectedSession)

        // (2), 3
        manager.select(session2)
        assertEquals(session2, manager.selectedSession)

        // (2)
        manager.remove(session3)
        assertEquals(session2, manager.selectedSession)

        // -
        manager.remove(session2)
        assertEquals(0, manager.size)
    }

    @Test
    fun `sessions property removes immutable copy`() {
        val manager = SessionManager(mock())

        val session1 = Session("https://www.mozilla.org")
        val session2 = Session("https://www.firefox.com")
        val session3 = Session("https://wiki.mozilla.org")
        val session4 = Session("https://github.com/mozilla-mobile/android-components")

        manager.add(session1)
        manager.add(session2)
        manager.add(session3)
        manager.add(session4)

        val sessions = manager.sessions

        assertEquals(4, sessions.size)
        assertTrue(sessions.contains(session1))
        assertTrue(sessions.contains(session2))
        assertTrue(sessions.contains(session3))
        assertTrue(sessions.contains(session4))

        manager.remove(session1)

        assertEquals(3, manager.size)
        assertEquals(4, sessions.size)
    }

    @Test
    fun `removeAll removes all sessions and notifies observer`() {
        val manager = SessionManager(mock())

        val session1 = Session("https://www.mozilla.org")
        val session2 = Session("https://www.firefox.com")
        val session3 = Session("https://wiki.mozilla.org")
        val session4 = Session("https://github.com/mozilla-mobile/android-components")

        manager.add(session1)
        manager.add(session2)
        manager.add(session3)
        manager.add(session4)

        val observer: SessionManager.Observer = mock()
        manager.register(observer)

        assertEquals(4, manager.size)

        manager.removeAll()

        assertEquals(0, manager.size)

        verify(observer).onAllSessionsRemoved()
        verifyNoMoreInteractions(observer)
    }

    @Test
    fun `findSessionById returns session with same id`() {
        val manager = SessionManager(mock())

        val session1 = Session("https://www.mozilla.org")
        val session2 = Session("https://www.firefox.com")
        val session3 = Session("https://wiki.mozilla.org")
        val session4 = Session("https://github.com/mozilla-mobile/android-components")

        manager.add(session1)
        manager.add(session2)
        manager.add(session3)
        manager.add(session4)

        assertEquals(session1, manager.findSessionById(session1.id))
        assertEquals(session2, manager.findSessionById(session2.id))
        assertEquals(session3, manager.findSessionById(session3.id))
        assertEquals(session4, manager.findSessionById(session4.id))

        assertNull(manager.findSessionById("banana"))
    }

    @Test
    fun `session manager creates and links engine session`() {
        val engine: Engine = mock()

        val actualEngineSession: EngineSession = mock()
        doReturn(actualEngineSession).`when`(engine).createSession()

        val sessionManager = SessionManager(engine)

        val session = Session("https://www.mozilla.org")
        sessionManager.add(session)

        assertNull(sessionManager.getEngineSession(session))

        assertEquals(actualEngineSession, sessionManager.getOrCreateEngineSession(session))
        assertEquals(actualEngineSession, sessionManager.getEngineSession(session))
        assertEquals(actualEngineSession, sessionManager.getOrCreateEngineSession(session))
    }

    @Test
    fun `removing a session unlinks the engine session`() {
        val engine: Engine = mock()

        val actualEngineSession: EngineSession = mock()
        doReturn(actualEngineSession).`when`(engine).createSession()

        val sessionManager = SessionManager(engine)

        val session = Session("https://www.mozilla.org")
        sessionManager.add(session)

        assertNotNull(sessionManager.getOrCreateEngineSession(session))
        assertNotNull(session.engineSessionHolder.engineSession)
        assertNotNull(session.engineSessionHolder.engineObserver)

        sessionManager.remove(session)

        assertNull(session.engineSessionHolder.engineSession)
        assertNull(session.engineSessionHolder.engineObserver)
    }

    @Test
    fun `add will link an engine session if provided`() {
        val engine: Engine = mock()

        val actualEngineSession: EngineSession = mock()
        val sessionManager = SessionManager(engine)

        val session = Session("https://www.mozilla.org")
        assertNull(session.engineSessionHolder.engineSession)
        assertNull(session.engineSessionHolder.engineObserver)

        sessionManager.add(session, engineSession = actualEngineSession)

        assertNotNull(session.engineSessionHolder.engineSession)
        assertNotNull(session.engineSessionHolder.engineObserver)

        assertEquals(actualEngineSession, sessionManager.getOrCreateEngineSession(session))
        assertEquals(actualEngineSession, sessionManager.getEngineSession(session))
        assertEquals(actualEngineSession, sessionManager.getOrCreateEngineSession(session))
    }
}
