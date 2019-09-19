/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

package mozilla.components.browser.session

import android.graphics.Bitmap
import mozilla.components.browser.state.state.CustomTabConfig
import mozilla.components.concept.engine.Engine
import mozilla.components.concept.engine.EngineSession
import mozilla.components.concept.engine.EngineSessionState
import mozilla.components.support.test.mock
import org.junit.Assert.assertEquals
import org.junit.Assert.assertFalse
import org.junit.Assert.assertNotNull
import org.junit.Assert.assertNull
import org.junit.Assert.assertTrue
import org.junit.Test
import org.mockito.ArgumentMatchers.anyString

import org.mockito.Mockito.`when`
import org.mockito.Mockito.doReturn
import org.mockito.Mockito.mock
import org.mockito.Mockito.never
import org.mockito.Mockito.reset
import org.mockito.Mockito.spy
import org.mockito.Mockito.times
import org.mockito.Mockito.verify
import org.mockito.Mockito.verifyNoMoreInteractions

class SessionManagerTest {
    @Test
    fun `session can be added`() {
        val manager = SessionManager(mock())
        manager.add(Session("http://getpocket.com"))
        manager.add(Session("http://www.firefox.com"), true)

        assertEquals(2, manager.size)
        assertEquals("http://www.firefox.com", manager.selectedSessionOrThrow.url)
    }

    @Test
    fun `session can be added by specifying parent`() {
        val manager = SessionManager(mock())
        val session1 = Session("https://www.mozilla.org")
        val session2 = Session("https://www.firefox.com")
        val session3 = Session("https://wiki.mozilla.org")
        val session4 = Session("https://github.com/mozilla-mobile/android-components")

        manager.add(session1)
        manager.add(session2)
        manager.add(session3, parent = session1)
        manager.add(session4, parent = session2)

        assertNull(manager.sessions[0].parentId)
        assertNull(manager.sessions[2].parentId)
        assertEquals(session1.id, manager.sessions[1].parentId)
        assertEquals(session2.id, manager.sessions[3].parentId)
    }

    @Test(expected = IllegalArgumentException::class)
    fun `session manager throws exception if parent is not in session manager`() {
        val parent = Session("https://www.mozilla.org")
        val session = Session("https://www.firefox.com")

        val manager = SessionManager(mock())
        manager.add(session, parent = parent)
    }

    @Test
    fun `session can be selected`() {
        val session1 = Session("http://www.mozilla.org")
        val session2 = Session("http://www.firefox.com")

        val manager = SessionManager(mock())
        manager.add(session1)
        manager.add(session2)

        assertEquals("http://www.mozilla.org", manager.selectedSessionOrThrow.url)
        manager.select(session2)
        assertEquals("http://www.firefox.com", manager.selectedSessionOrThrow.url)
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

    @Test
    fun `createSnapshot works when manager has no sessions`() {
        val manager = SessionManager(mock())
        assertTrue(manager.createSnapshot().isEmpty())
    }

    @Test
    fun `createSnapshot ignores private sessions`() {
        val manager = SessionManager(mock())
        val session = Session("http://mozilla.org", true)
        manager.add(session)

        assertTrue(manager.createSnapshot().isEmpty())
    }

    @Test
    fun `createSnapshot ignores CustomTab sessions`() {
        val manager = SessionManager(mock())
        val session = Session("http://mozilla.org")
        session.customTabConfig = mock(CustomTabConfig::class.java)
        manager.add(session)

        assertTrue(manager.createSnapshot().isEmpty())
    }

    @Test
    fun `createSnapshot ignores private CustomTab sessions`() {
        val manager = SessionManager(mock())
        val session = Session("http://mozilla.org", true)
        session.customTabConfig = mock(CustomTabConfig::class.java)
        manager.add(session)

        assertTrue(manager.createSnapshot().isEmpty())
    }

    @Test
    fun `restore checks validity of a snapshot - empty`() {
        val manager = SessionManager(mock())

        val observer: SessionManager.Observer = mock()
        manager.register(observer)

        manager.restore(SessionManager.Snapshot(listOf(), selectedSessionIndex = 0))

        verify(observer, never()).onSessionsRestored()
    }

    @Test
    fun `Restore single session snapshot without updating selection`() {
        val session: Session

        val manager = SessionManager(mock()).apply {
            session = Session("https://getpocket.com")

            add(Session("https://www.mozilla.org"))
            add(session)
            add(Session("https://www.firefox.com"))
        }

        val item = manager.createSessionSnapshot(session)

        manager.remove(session)

        val observer: SessionManager.Observer = mock()
        manager.register(observer)

        manager.restore(SessionManager.Snapshot.singleItem(item), updateSelection = false)

        assertEquals(3, manager.size)
        assertEquals("https://www.mozilla.org", manager.selectedSessionOrThrow.url)
        assertEquals("https://getpocket.com", manager.sessions[0].url)
        assertEquals("https://www.mozilla.org", manager.sessions[1].url)
        assertEquals("https://www.firefox.com", manager.sessions[2].url)

        verify(observer).onSessionsRestored()
    }

    @Test
    fun `Restore sessions with already existing session`() {
        val manager = SessionManager(engine = mock())
        manager.add(Session("https://www.mozilla.org"))

        assertEquals("https://www.mozilla.org", manager.selectedSessionOrThrow.url)

        val snapshot = SessionManager.Snapshot(
            listOf(
                SessionManager.Snapshot.Item(session = Session("https://www.firefox.com")),
                SessionManager.Snapshot.Item(session = Session("https://www.wikipedia.org")),
                SessionManager.Snapshot.Item(session = Session("https://getpocket.com"))
            ),
            selectedSessionIndex = 1
        )

        manager.restore(snapshot, updateSelection = false)

        assertEquals(4, manager.size)

        assertEquals("https://www.mozilla.org", manager.selectedSessionOrThrow.url)
        assertEquals("https://www.firefox.com", manager.sessions[0].url)
        assertEquals("https://www.wikipedia.org", manager.sessions[1].url)
        assertEquals("https://getpocket.com", manager.sessions[2].url)
        assertEquals("https://www.mozilla.org", manager.sessions[3].url)
    }

    @Test
    fun `restore may be used to bulk-add session from a SessionsSnapshot`() {
        val manager = SessionManager(mock())

        // Just one session in the snapshot.
        manager.restore(
            SessionManager.Snapshot(
                listOf(SessionManager.Snapshot.Item(session = Session("http://www.mozilla.org"))),
                selectedSessionIndex = 0
            )
        )
        assertEquals(1, manager.size)
        assertEquals("http://www.mozilla.org", manager.selectedSessionOrThrow.url)

        // Multiple sessions in the snapshot.
        val regularSession = Session("http://www.firefox.com")
        val engineSessionState: EngineSessionState = mock()
        val engineSession = mock(EngineSession::class.java)
        `when`(engineSession.saveState()).thenReturn(engineSessionState)

        val snapshot = SessionManager.Snapshot(
            listOf(
                SessionManager.Snapshot.Item(
                    session = regularSession,
                    engineSession = engineSession
                ),
                SessionManager.Snapshot.Item(session = Session("http://www.wikipedia.org"))
            ),
            selectedSessionIndex = 0
        )
        manager.restore(snapshot)
        assertEquals(3, manager.size)
        assertEquals("http://www.firefox.com", manager.selectedSessionOrThrow.url)
        assertEquals(engineSession, manager.selectedSessionOrThrow.engineSessionHolder.engineSession)
        assertNull(manager.selectedSessionOrThrow.engineSessionHolder.engineSessionState)
    }

    @Test
    fun `restore fires correct notifications`() {
        val manager = SessionManager(mock())

        val observer: SessionManager.Observer = mock()
        manager.register(observer)

        val session = Session("http://www.mozilla.org")
        // Snapshot with a single session.
        manager.restore(SessionManager.Snapshot(listOf(
            SessionManager.Snapshot.Item(
                session
            )
        ), 0))

        verify(observer, times(1)).onSessionsRestored()
        verify(observer, never()).onSessionAdded(session)
        verify(observer, times(1)).onSessionSelected(session)

        manager.removeAll()
        reset(observer)

        val session2 = Session("http://www.firefox.com")
        val session3 = Session("http://www.wikipedia.org")
        // Snapshot with multiple sessions.
        manager.restore(SessionManager.Snapshot(
            listOf(
                SessionManager.Snapshot.Item(session2),
                SessionManager.Snapshot.Item(session3),
                SessionManager.Snapshot.Item(session)
            ),
            1
        ))

        assertEquals(3, manager.size)
        verify(observer, times(1)).onSessionsRestored()
        verify(observer, never()).onSessionAdded(session)
        verify(observer, never()).onSessionAdded(session2)
        verify(observer, never()).onSessionAdded(session3)
        verify(observer, never()).onSessionSelected(session)
        verify(observer, never()).onSessionSelected(session2)
        verify(observer, times(1)).onSessionSelected(session3)
    }

    @Test
    fun `createSnapshot produces a correct snapshot of sessions`() {
        val manager = SessionManager(mock())
        val customTabSession = Session("http://mozilla.org")
        customTabSession.customTabConfig = mock(CustomTabConfig::class.java)
        val privateSession = Session("http://www.secret.com", true)
        val privateCustomTabSession = Session("http://very.secret.com", true)
        privateCustomTabSession.customTabConfig = mock(CustomTabConfig::class.java)

        val regularSession = Session("http://www.firefox.com")
        val engineSessionState: EngineSessionState = mock()
        val engineSession = mock(EngineSession::class.java)
        `when`(engineSession.saveState()).thenReturn(engineSessionState)

        val engine = mock(Engine::class.java)
        `when`(engine.name()).thenReturn("gecko")
        `when`(engine.createSession()).thenReturn(mock(EngineSession::class.java))

        manager.add(regularSession, false, engineSession)
        manager.add(Session("http://firefox.com"), true, engineSession)
        manager.add(Session("http://wikipedia.org"), false, engineSession)
        manager.add(privateSession)
        manager.add(customTabSession)
        manager.add(privateCustomTabSession)

        val snapshot = manager.createSnapshot()
        assertEquals(3, snapshot.sessions.size)
        assertEquals(1, snapshot.selectedSessionIndex)

        val snapshotSession = snapshot.sessions[0]
        assertEquals("http://www.firefox.com", snapshotSession.session.url)

        val snapshotState = snapshotSession.engineSession!!.saveState()
        assertEquals(engineSessionState, snapshotState)
    }

    @Test
    fun `createSnapshot resets selection index if selected session was private`() {
        val manager = SessionManager(mock())

        val privateSession = Session("http://www.secret.com", true)
        val regularSession1 = Session("http://www.firefox.com")
        val regularSession2 = Session("http://www.mozilla.org")

        val engine = mock(Engine::class.java)
        `when`(engine.name()).thenReturn("gecko")
        `when`(engine.createSession()).thenReturn(mock(EngineSession::class.java))

        manager.add(regularSession1, false)
        manager.add(regularSession2, false)
        manager.add(privateSession, true)

        val snapshot = manager.createSnapshot()
        assertEquals(2, snapshot.sessions.size)
        assertEquals(0, snapshot.selectedSessionIndex)
    }

    @Test
    fun `selectedSession is null with no selection`() {
        val manager = SessionManager(mock())
        assertNull(manager.selectedSession)
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
        session4.customTabConfig = mock(CustomTabConfig::class.java)

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
        doReturn(actualEngineSession).`when`(engine).createSession(false)
        val privateEngineSession: EngineSession = mock()
        doReturn(privateEngineSession).`when`(engine).createSession(true)

        val sessionManager = SessionManager(engine)

        val session = Session("https://www.mozilla.org")
        sessionManager.add(session)

        assertNull(sessionManager.getEngineSession(session))
        assertEquals(actualEngineSession, sessionManager.getOrCreateEngineSession(session))
        assertEquals(actualEngineSession, sessionManager.getEngineSession(session))
        assertEquals(actualEngineSession, sessionManager.getOrCreateEngineSession(session))

        val privateSession = Session("https://www.mozilla.org", true, Session.Source.NONE)
        sessionManager.add(privateSession)
        assertNull(sessionManager.getEngineSession(privateSession))
        assertEquals(privateEngineSession, sessionManager.getOrCreateEngineSession(privateSession))
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

    @Test
    fun `removeSessions retains customtab sessions`() {
        val manager = SessionManager(mock())

        val session1 = Session("https://www.mozilla.org")
        val session2 = Session("https://getPocket.com")
        val session3 = Session("https://www.firefox.com")
        session2.customTabConfig = mock(CustomTabConfig::class.java)

        manager.add(session1)
        manager.add(session2)
        manager.add(session3)

        val observer: SessionManager.Observer = mock()
        manager.register(observer)

        assertEquals(3, manager.size)

        manager.removeSessions()

        assertEquals(1, manager.size)
        assertEquals(session2, manager.all[0])

        verify(observer).onAllSessionsRemoved()
        verifyNoMoreInteractions(observer)
    }

    @Test(expected = IllegalStateException::class)
    fun `exception is thrown from selectedSessionOrThrow with no selection`() {
        val manager = SessionManager(mock())
        manager.selectedSessionOrThrow
    }

    @Test
    fun `thumbnails of all but selected session should be removed on low memory`() {
        val manager = SessionManager(mock())

        val emptyBitmap = spy(Bitmap::class.java)

        val session1 = Session("https://www.mozilla.org")
        session1.thumbnail = emptyBitmap

        val session2 = Session("https://getPocket.com")
        session2.thumbnail = emptyBitmap

        val session3 = Session("https://www.firefox.com")
        session3.thumbnail = emptyBitmap

        manager.add(session1, true)
        manager.add(session2, false)
        manager.add(session3, false)

        val allSessionsMustHaveAThumbnail = manager.all.all { it.thumbnail != null }

        assertTrue(allSessionsMustHaveAThumbnail)

        manager.onLowMemory()

        val onlySelectedSessionMustHaveAThumbnail =
                session1.thumbnail != null && session2.thumbnail == null && session3.thumbnail == null

        assertTrue(onlySelectedSessionMustHaveAThumbnail)
    }

    @Test
    fun `custom tab session will not be selected if it is the first session`() {
        val session = Session("about:blank")
        session.customTabConfig = mock(CustomTabConfig::class.java)

        val manager = SessionManager(mock())
        manager.add(session)

        assertNull(manager.selectedSession)
    }

    @Test
    fun `parent will be selected if child is removed and flag is set to true`() {
        val parent = Session("https://www.mozilla.org")

        val session1 = Session("https://www.firefox.com")
        val session2 = Session("https://getpocket.com")
        val child = Session("https://www.mozilla.org/en-US/internet-health/")

        val manager = SessionManager(mock())
        manager.add(parent)
        manager.add(session1)
        manager.add(session2)
        manager.add(child, parent = parent)

        manager.select(child)
        manager.remove(child, selectParentIfExists = true)

        assertEquals(parent, manager.selectedSession)
        assertEquals("https://www.mozilla.org", manager.selectedSessionOrThrow.url)
    }

    @Test
    fun `parent will not be selected if child is removed and flag is set to false`() {
        val parent = Session("https://www.mozilla.org")

        val session1 = Session("https://www.firefox.com")
        val session2 = Session("https://getpocket.com")
        val child1 = Session("https://www.mozilla.org/en-US/internet-health/")
        val child2 = Session("https://www.mozilla.org/en-US/technology/")

        val manager = SessionManager(mock())
        manager.add(parent)
        manager.add(session1)
        manager.add(session2)
        manager.add(child1, parent = parent)
        manager.add(child2, parent = parent)

        manager.select(child1)
        manager.remove(child1, selectParentIfExists = false)

        assertEquals(session1, manager.selectedSession)
        assertEquals("https://www.firefox.com", manager.selectedSessionOrThrow.url)
    }

    @Test
    fun `Setting selectParentIfExists when removing session without parent has no effect`() {
        val session1 = Session("https://www.firefox.com")
        val session2 = Session("https://getpocket.com")
        val session3 = Session("https://www.mozilla.org/en-US/internet-health/")

        val manager = SessionManager(mock())
        manager.add(session1)
        manager.add(session2)
        manager.add(session3)

        manager.select(session3)
        manager.remove(session3, selectParentIfExists = true)

        assertEquals(session2, manager.selectedSession)
        assertEquals("https://getpocket.com", manager.selectedSessionOrThrow.url)
    }

    @Test
    fun `Sessions with parent are added after parent`() {
        val parent01 = Session("https://www.mozilla.org")
        val parent02 = Session("https://getpocket.com")

        val session1 = Session("https://www.firefox.com")
        val session2 = Session("https://developer.mozilla.org/en-US/")
        val child001 = Session("https://www.mozilla.org/en-US/internet-health/")
        val child002 = Session("https://www.mozilla.org/en-US/technology/")
        val child003 = Session("https://getpocket.com/add/")

        val manager = SessionManager(mock())
        manager.add(parent01)
        manager.add(session1)
        manager.add(child001, parent = parent01)
        manager.add(session2)
        manager.add(parent02)
        manager.add(child002, parent = parent01)
        manager.add(child003, parent = parent02)

        assertEquals(parent01, manager.sessions[0]) // ├── parent 1
        assertEquals(child002, manager.sessions[1]) // │   ├── child 2
        assertEquals(child001, manager.sessions[2]) // │   └── child 1
        assertEquals(session1, manager.sessions[3]) // ├──session 1
        assertEquals(session2, manager.sessions[4]) // ├──session 2
        assertEquals(parent02, manager.sessions[5]) // └── parent 2
        assertEquals(child003, manager.sessions[6]) //     └── child 3
    }

    @Test
    fun `SessionManager updates parent id of children after updating parent`() {
        val session1 = Session("https://www.firefox.com")
        val session2 = Session("https://developer.mozilla.org/en-US/")
        val session3 = Session("https://www.mozilla.org/en-US/internet-health/")
        val session4 = Session("https://www.mozilla.org/en-US/technology/")

        val manager = SessionManager(mock())
        manager.add(session1)
        manager.add(session2, parent = session1)
        manager.add(session3, parent = session2)
        manager.add(session4, parent = session3)

        // session 1 <- session2 <- session3 <- session4
        assertNull(session1.parentId)
        assertEquals(session1.id, session2.parentId)
        assertEquals(session2.id, session3.parentId)
        assertEquals(session3.id, session4.parentId)

        manager.remove(session3)

        assertEquals(session1, manager.sessions[0])
        assertEquals(session2, manager.sessions[1])
        assertEquals(session4, manager.sessions[2])

        // session1 <- session2 <- session4
        assertNull(session1.parentId)
        assertEquals(session1.id, session2.parentId)
        assertEquals(session2.id, session4.parentId)

        manager.remove(session1)

        assertEquals(session2, manager.sessions[0])
        assertEquals(session4, manager.sessions[1])

        // session2 <- session4
        assertNull(session2.parentId)
        assertEquals(session2.id, session4.parentId)
    }

    @Test
    fun `SessionManager should not select custom tab session after removing selected session`() {
        val manager = SessionManager(mock())
        assertNull(manager.selectedSession)

        val customTabSession = Session("https://www.mozilla.org")
        customTabSession.customTabConfig = mock()
        manager.add(customTabSession)

        assertNull(manager.selectedSession)

        val session = Session("https://www.firefox.com")
        manager.add(session)

        assertEquals(session, manager.selectedSession)

        manager.remove(session)

        assertNull(manager.selectedSession)
    }

    @Test
    fun `SessionManager should not select parent if it is a custom tab session`() {
        val parent = Session("https://www.mozilla.org")
        parent.customTabConfig = mock()

        val session1 = Session("https://www.firefox.com")
        val session2 = Session("https://getpocket.com")
        val child = Session("https://www.mozilla.org/en-US/internet-health/")

        val manager = SessionManager(mock())
        manager.add(parent)
        manager.add(session1)
        manager.add(session2)
        manager.add(child, parent = parent)

        manager.select(child)
        manager.remove(child, selectParentIfExists = true)

        assertEquals(session1, manager.selectedSession)
        assertEquals("https://www.firefox.com", manager.selectedSessionOrThrow.url)
    }

    @Test
    fun `SessionManager will select new non custom tab session if selected session gets removed`() {
        val session1 = Session("https://www.firefox.com").apply { customTabConfig = mock() }
        val session2 = Session("https://developer.mozilla.org/en-US/")
        val session3 = Session("https://www.mozilla.org/en-US/internet-health/").apply { customTabConfig = mock() }
        val session4 = Session("https://www.mozilla.org/en-US/technology/")
        val session5 = Session("https://example.org/555").apply { customTabConfig = mock() }
        val session6 = Session("https://example.org/Hello").apply { customTabConfig = mock() }
        val session7 = Session("https://example.org/World").apply { customTabConfig = mock() }
        val session8 = Session("https://example.org/JustTestingThings").apply { customTabConfig = mock() }
        val session9 = Session("https://example.org/NoCustomTab")

        val manager = SessionManager(mock())
        manager.add(session1)
        manager.add(session2)
        manager.add(session3)
        manager.add(session4)
        manager.add(session5)
        manager.add(session6)
        manager.add(session7)
        manager.add(session8)
        manager.add(session9)

        manager.select(session4)
        assertEquals(session4, manager.selectedSession)

        manager.remove(session4)
        assertEquals(session9, manager.selectedSession)

        manager.remove(session9)
        assertEquals(session2, manager.selectedSession)

        manager.remove(session2)
        assertNull(manager.selectedSession)
    }

    @Test(expected = java.lang.IllegalStateException::class)
    fun `SessionManager throws if parent to be selected cannot be found`() {
        // This should never be possible with the current implementation of SessionManager. However this test makes
        // sure that if this situation occurs (bug) then we throw an exception.

        val parent = Session("https://www.mozilla.org")
        val session = Session("https://www.firefox.com")

        val manager = SessionManager(mock())
        manager.add(parent)
        manager.add(session, parent = parent)

        manager.select(session)

        // The parent id is marked as "internal". Component consuemrs normally cannot modify it.
        session.parentId = "DoesNotExist"

        manager.remove(session, selectParentIfExists = true)
    }

    @Test
    fun `SessionManager will select nearby private session if selected private session gets removed`() {
        val manager = SessionManager(mock())
        assertNull(manager.selectedSession)

        val private1 = Session("https://example.org/private1", private = true)
        manager.add(private1)

        val regular1 = Session("https://www.mozilla.org", private = false)
        manager.add(regular1)

        val regular2 = Session("https://www.firefox.com", private = false)
        manager.add(regular2)

        val private2 = Session("https://example.org/private2", private = true)
        manager.add(private2)

        val private3 = Session("https://example.org/private3", private = true)
        manager.add(private3)

        manager.select(private2)
        manager.remove(private2)
        assertEquals(private3, manager.selectedSession)

        manager.remove(private3)
        assertEquals(private1, manager.selectedSession)

        // Removing the last private session should cause a regular session to be selected
        manager.remove(private1)
        assertEquals(regular2, manager.selectedSession)
    }

    @Test
    fun `SessionManager will select nearby regular session if selected regular session gets removed`() {
        val manager = SessionManager(mock())
        assertNull(manager.selectedSession)

        val regular1 = Session("https://www.mozilla.org", private = false)
        manager.add(regular1)

        val private1 = Session("https://example.org/private1", private = true)
        manager.add(private1)

        val private2 = Session("https://example.org/private2", private = true)
        manager.add(private2)

        val regular2 = Session("https://www.firefox.com", private = false)
        manager.add(regular2)

        val regular3 = Session("https://www.firefox.org", private = false)
        manager.add(regular3)

        manager.select(regular2)
        manager.remove(regular2)
        assertEquals(regular3, manager.selectedSession)

        manager.remove(regular3)
        assertEquals(regular1, manager.selectedSession)

        // Removing the last regular session should NOT cause a private session to be selected
        manager.remove(regular1)
        assertNull(manager.selectedSession)
    }

    @Test
    fun `SessionManager#runWithSession executes the block when session found`() {
        val sessionManager = spy(SessionManager(mock()))

        `when`(sessionManager.findSessionById(anyString())).thenReturn(mock())

        val executed = sessionManager.runWithSession("123") { true }

        assertTrue(executed)
    }

    @Test
    fun `SessionManager#runWithSession with null session ID`() {
        val sessionManager = spy(SessionManager(mock()))

        val executed = sessionManager.runWithSession(null) { true }

        assertFalse(executed)
    }

    @Test
    fun `SessionManager#runWithSession with null session`() {
        val sessionManager = spy(SessionManager(mock()))

        val executed = sessionManager.runWithSession("123") { true }

        assertFalse(executed)
    }

    @Test
    fun `SessionManager#runWithSessionIdOrSelected executes the block when session found`() {
        val sessionManager = spy(SessionManager(mock()))

        `when`(sessionManager.findSessionById(anyString())).thenReturn(mock())

        val executed = sessionManager.runWithSessionIdOrSelected("123") { }

        assertTrue(executed)
    }

    @Test
    fun `SessionManager#runWithSessionIdOrSelected with null or empty session ID`() {
        val sessionManager = spy(SessionManager(mock()))

        var executed = sessionManager.runWithSessionIdOrSelected(null) { }

        assertFalse(executed)

        executed = sessionManager.runWithSessionIdOrSelected("") { }
        assertFalse(executed)
    }

    @Test
    fun `SessionManager#runWithSessionIdOrSelected with null session will use the selected session`() {
        val sessionManager = spy(SessionManager(mock()))
        val selectedSession = Session("", id = "selectedSessionId")
        sessionManager.add(selectedSession)
        sessionManager.select(selectedSession)

        var selectedSessionId = "123"
        val executed = sessionManager.runWithSessionIdOrSelected(null) { session ->
            selectedSessionId = session.id
        }

        assertTrue(executed)
        assertTrue(selectedSessionId == "selectedSessionId")
    }

    @Test
    fun `SessionManager#unWithSessionIdOrSelected should run for either provided or selected session, but not both`() {
        val sessionManager = spy(SessionManager(mock()))
        val anotherSession = Session("", id = "anotherSessionId")
        val selectedSession = Session("", id = "selectedSessionId")

        sessionManager.add(anotherSession)
        sessionManager.add(selectedSession)
        sessionManager.select(selectedSession)

        var runSessionId = "123"
        val executed = sessionManager.runWithSessionIdOrSelected("anotherSessionId") { session ->
            runSessionId = session.id
        }

        assertTrue(executed)
        assertTrue(runSessionId == "anotherSessionId")
    }

    @Test(expected = IllegalArgumentException::class)
    fun `SessionManager throws exception when adding session that already exists`() {
        val session = Session("https://www.firefox.com")

        val manager = SessionManager(mock())
        manager.add(session)
        manager.add(session)
    }

    @Test(expected = IllegalArgumentException::class)
    fun `SessionManager throws exception when restoring session that already exists`() {
        val manager = SessionManager(engine = mock())
        val session = Session("https://www.mozilla.org")
        manager.add(session)

        assertEquals("https://www.mozilla.org", manager.selectedSessionOrThrow.url)

        val snapshot = SessionManager.Snapshot(
            listOf(
                SessionManager.Snapshot.Item(session = session)
            ),
            selectedSessionIndex = 1
        )

        manager.restore(snapshot)
    }

    @Test
    fun `WHEN adding multiple sessions THEN sessions get added and selection gets updated`() {
        val manager = SessionManager(engine = mock())

        val sessions = listOf(
            Session("https://www.mozilla.org"),
            Session("https://www.example.org"),
            Session("https://www.firefox.com", private = true)
        )

        assertEquals(0, manager.sessions.size)
        assertNull(manager.selectedSession)

        manager.add(sessions)

        assertEquals(3, manager.sessions.size)
        assertEquals("https://www.mozilla.org", manager.selectedSessionOrThrow.url)
    }

    @Test(expected = java.lang.IllegalArgumentException::class)
    fun `WHEN adding multiple session containing custom tab THEN adding fails`() {
        val manager = SessionManager(engine = mock())

        val sessions = listOf(
                Session("https://www.mozilla.org"),
                Session("https://www.example.org").apply {
                    customTabConfig = mock()
                },
                Session("https://www.firefox.com", private = true)
        )

        manager.add(sessions)
    }

    @Test(expected = java.lang.IllegalArgumentException::class)
    fun `WHEN adding multiple session AND parent id THEN adding fails`() {
        val manager = SessionManager(engine = mock())

        val sessions = listOf(
            Session("https://www.mozilla.org"),
            Session("https://www.example.org").apply {
                parentId = "some-parent"
            },
            Session("https://www.firefox.com", private = true)
        )

        manager.add(sessions)
    }

    @Test
    fun `When adding multiple sessions then non-private session will get selected`() {
        val manager = SessionManager(engine = mock())

        val sessions = listOf(
            Session("https://www.mozilla.org", private = true),
            Session("https://www.example.org", private = true),
            Session("https://getpocket.com"),
            Session("https://www.firefox.com", private = true)
        )

        assertEquals(0, manager.sessions.size)
        assertNull(manager.selectedSession)

        manager.add(sessions)

        assertEquals(4, manager.sessions.size)
        assertEquals("https://getpocket.com", manager.selectedSessionOrThrow.url)
    }

    @Test
    fun `WHEN adding multiple private sessions THEN none is selected`() {
        val manager = SessionManager(engine = mock())

        val sessions = listOf(
            Session("https://www.mozilla.org", private = true),
            Session("https://www.example.org", private = true),
            Session("https://www.firefox.com", private = true)
        )

        assertEquals(0, manager.sessions.size)
        assertNull(manager.selectedSession)

        manager.add(sessions)

        assertEquals(3, manager.sessions.size)
        assertNull(manager.selectedSession)
    }

    @Test
    fun `WHEN adding multiple sessions AND selection already exist THEN selection is not updated`() {
        val manager = SessionManager(engine = mock())

        manager.add(Session("https://www.mozilla.org"))
        manager.add(Session("https://www.example.org"))

        val sessions = listOf(
                Session("htttps://getpocket.com"),
                Session("https://www.firefox.com", private = true)
        )

        assertEquals(2, manager.sessions.size)
        assertEquals("https://www.mozilla.org", manager.selectedSessionOrThrow.url)

        manager.add(sessions)

        assertEquals(4, manager.sessions.size)
        assertEquals("https://www.mozilla.org", manager.selectedSessionOrThrow.url)
    }

    @Test
    fun `WHEN adding multiple sessions THEN observer is notified`() {
        val manager = SessionManager(engine = mock())

        val observer: SessionManager.Observer = mock()
        manager.register(observer)

        val sessions = listOf(
            Session("htttps://getpocket.com"),
            Session("https://www.firefox.com", private = true)
        )

        verify(observer, never()).onSessionsRestored()

        manager.add(sessions)

        verify(observer).onSessionsRestored()
    }
}
