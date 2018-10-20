/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

package mozilla.components.browser.session.storage

import android.util.AtomicFile
import mozilla.components.browser.session.Session
import mozilla.components.browser.session.Session.Source
import mozilla.components.browser.session.SessionManager
import mozilla.components.concept.engine.Engine
import mozilla.components.concept.engine.EngineSession
import org.json.JSONException
import org.json.JSONObject
import org.junit.Assert.assertEquals
import org.junit.Assert.assertNull
import org.junit.Assert.assertNotNull
import org.junit.Assert.assertTrue
import org.junit.Assert.assertFalse
import org.junit.Test
import org.junit.runner.RunWith
import org.mockito.ArgumentMatchers
import org.mockito.ArgumentMatchers.any
import org.mockito.ArgumentMatchers.anyString
import org.mockito.Mockito.`when`
import org.mockito.Mockito.doReturn
import org.mockito.Mockito.doThrow
import org.mockito.Mockito.mock
import org.mockito.Mockito.never
import org.mockito.Mockito.spy
import org.mockito.Mockito.verify
import org.robolectric.RobolectricTestRunner
import org.robolectric.RuntimeEnvironment
import java.io.FileNotFoundException
import java.io.IOException
import java.util.concurrent.ScheduledExecutorService
import java.util.concurrent.ScheduledFuture
import java.util.concurrent.TimeUnit

@RunWith(RobolectricTestRunner::class)
class DefaultSessionStorageTest {

    @Test
    fun persistAndRestore() {
        val session1 = Session("http://mozilla.org", id = "session1")
        val session2 = Session("http://getpocket.com", id = "session2")
        val session3 = Session("http://getpocket.com", id = "session3")
        session3.parentId = "session1"

        val engineSessionState = mutableMapOf("k0" to "v0", "k1" to 1, "k2" to true, "k3" to emptyList<Any>())

        val engineSession = mock(EngineSession::class.java)
        `when`(engineSession.saveState()).thenReturn(engineSessionState)

        val engine = mock(Engine::class.java)
        `when`(engine.name()).thenReturn("gecko")
        `when`(engine.createSession()).thenReturn(mock(EngineSession::class.java))

        // Engine session just for one of the sessions for simplicity.
        val sessionsSnapshot = SessionsSnapshot(
            sessions = listOf(
                SessionWithState(session1, engineSession),
                SessionWithState(session2),
                SessionWithState(session3)
            ),
            selectedSessionIndex = 0
        )

        // Persist the snapshot
        val storage = DefaultSessionStorage(RuntimeEnvironment.application)
        val persisted = storage.persist(engine, sessionsSnapshot)
        assertTrue(persisted)

        // Read it back
        val restoredSnapshot = storage.read(engine)
        assertNotNull(restoredSnapshot)
        assertEquals(3, restoredSnapshot!!.sessions.size)
        assertEquals(0, restoredSnapshot.selectedSessionIndex)

        assertEquals(session1, restoredSnapshot.sessions[0].session)
        assertEquals(session1.url, restoredSnapshot.sessions[0].session.url)
        assertEquals(session1.id, restoredSnapshot.sessions[0].session.id)
        assertNull(restoredSnapshot.sessions[0].session.parentId)

        assertEquals(session2, restoredSnapshot.sessions[1].session)
        assertEquals(session2.url, restoredSnapshot.sessions[1].session.url)
        assertEquals(session2.id, restoredSnapshot.sessions[1].session.id)
        assertNull(restoredSnapshot.sessions[1].session.parentId)

        assertEquals(session3, restoredSnapshot.sessions[2].session)
        assertEquals(session3.url, restoredSnapshot.sessions[2].session.url)
        assertEquals(session3.id, restoredSnapshot.sessions[2].session.id)
        assertEquals("session1", restoredSnapshot.sessions[2].session.parentId)

        val restoredEngineSession = restoredSnapshot.sessions[0].engineSession
        assertNotNull(restoredEngineSession)
        verify(restoredEngineSession)!!.restoreState(engineSessionState.filter { it.key != "k3" })
    }

    @Test
    fun persistAndClear() {
        val session1 = Session("http://mozilla.org", id = "session1")
        val session2 = Session("http://getpocket.com", id = "session2")

        val engineSession = mock(EngineSession::class.java)

        val engine = mock(Engine::class.java)
        `when`(engine.name()).thenReturn("gecko")
        `when`(engine.createSession()).thenReturn(mock(EngineSession::class.java))

        // Engine session just for one of the sessions for simplicity.
        val sessionsSnapshot = SessionsSnapshot(
            sessions = listOf(SessionWithState(session1, engineSession), SessionWithState(session2)),
            selectedSessionIndex = 0
        )

        // Persist the snapshot
        val storage = DefaultSessionStorage(RuntimeEnvironment.application)
        val persisted = storage.persist(engine, sessionsSnapshot)
        assertTrue(persisted)

        storage.clear(engine)

        // Read it back. Expect null, indicating empty.
        val restoredSnapshot = storage.read(engine)
        assertNull(restoredSnapshot)
    }

    @Test(expected = IllegalArgumentException::class)
    fun persistThrowsOnEmptySnapshot() {
        val engine = mock(Engine::class.java)
        `when`(engine.name()).thenReturn("gecko")

        val storage = DefaultSessionStorage(RuntimeEnvironment.application)
        storage.persist(engine, SessionsSnapshot(sessions = listOf(), selectedSessionIndex = 0))
    }

    @Test(expected = IllegalArgumentException::class)
    fun persistThrowsOnIllegalSnapshot() {
        val engine = mock(Engine::class.java)
        `when`(engine.name()).thenReturn("gecko")

        val storage = DefaultSessionStorage(RuntimeEnvironment.application)

        val session = Session("http://mozilla.org")
        val engineSession = mock(EngineSession::class.java)
        val sessionsSnapshot = SessionsSnapshot(
            sessions = listOf(SessionWithState(session, engineSession)),
            selectedSessionIndex = 1
        )
        storage.persist(engine, sessionsSnapshot)
    }

    @Test
    fun restoreReturnsNullOnIOException() {
        val engine = mock(Engine::class.java)
        `when`(engine.name()).thenReturn("gecko")

        val context = spy(RuntimeEnvironment.application)
        val storage = spy(DefaultSessionStorage(context))
        val file = mock(AtomicFile::class.java)

        doReturn(file).`when`(storage).getFile(anyString())
        doThrow(FileNotFoundException::class.java).`when`(file).openRead()

        assertNull(storage.read(engine))
    }

    @Test
    fun restoreReturnsNullOnJSONException() {
        val session = Session("http://mozilla.org")
        val engineSession = mock(EngineSession::class.java)

        val engine = mock(Engine::class.java)
        `when`(engine.name()).thenReturn("gecko")
        `when`(engine.createSession()).thenReturn(engineSession)

        val sessionsSnapshot = SessionsSnapshot(
            sessions = listOf(SessionWithState(session, engineSession)),
            selectedSessionIndex = 0
        )

        val storage = spy(DefaultSessionStorage(RuntimeEnvironment.application))
        val persisted = storage.persist(engine, sessionsSnapshot)
        assertTrue(persisted)

        doThrow(JSONException::class.java).`when`(storage).deserializeSession(anyJson())
        assertNull(storage.read(engine))
    }

    @Test
    fun persistReturnsFalseOnIOException() {
        val engine = mock(Engine::class.java)
        `when`(engine.name()).thenReturn("gecko")

        val session = Session("http://mozilla.org")
        val engineSession = mock(EngineSession::class.java)
        val context = spy(RuntimeEnvironment.application)
        val storage = spy(DefaultSessionStorage(context))
        val file = mock(AtomicFile::class.java)

        val sessionsSnapshot = SessionsSnapshot(
            sessions = listOf(SessionWithState(session, engineSession)),
            selectedSessionIndex = 0
        )

        doReturn(file).`when`(storage).getFile(anyString())
        doThrow(IOException::class.java).`when`(file).startWrite()

        assertFalse(storage.persist(engine, sessionsSnapshot))
    }

    @Test
    fun persistReturnsFalseOnJSONException() {
        val engine = mock(Engine::class.java)
        `when`(engine.name()).thenReturn("gecko")

        val session = Session("http://mozilla.org")
        val engineSession = mock(EngineSession::class.java)
        val context = spy(RuntimeEnvironment.application)
        val storage = spy(DefaultSessionStorage(context))
        val file = mock(AtomicFile::class.java)

        val sessionsSnapshot = SessionsSnapshot(
            sessions = listOf(SessionWithState(session, engineSession)),
            selectedSessionIndex = 0
        )

        doReturn(file).`when`(storage).getFile(anyString())
        doThrow(JSONException::class.java).`when`(storage).serializeSession(session)

        val persisted = storage.persist(engine, sessionsSnapshot)
        assertFalse(persisted)
    }

    @Test
    fun startSchedulesPeriodicSaves() {
        val engine = mock(Engine::class.java)
        val scheduler = mock(ScheduledExecutorService::class.java)
        val scheduledFuture = mock(ScheduledFuture::class.java)
        `when`(scheduler.scheduleAtFixedRate(any(Runnable::class.java),
                ArgumentMatchers.eq(300L),
                ArgumentMatchers.eq(300L),
                ArgumentMatchers.eq(TimeUnit.SECONDS))).thenReturn(scheduledFuture)

        val storage = DefaultSessionStorage(RuntimeEnvironment.application, true, 300, scheduler)
        storage.start(SessionManager(engine))
        verify(scheduler).scheduleAtFixedRate(any(Runnable::class.java),
                ArgumentMatchers.eq(300L),
                ArgumentMatchers.eq(300L),
                ArgumentMatchers.eq(TimeUnit.SECONDS))
    }

    @Test
    fun stopCancelsScheduledTask() {
        val engine = mock(Engine::class.java)
        val scheduler = mock(ScheduledExecutorService::class.java)
        val scheduledFuture = mock(ScheduledFuture::class.java)
        `when`(scheduler.scheduleAtFixedRate(any(Runnable::class.java),
                ArgumentMatchers.eq(300L),
                ArgumentMatchers.eq(300L),
                ArgumentMatchers.eq(TimeUnit.SECONDS))).thenReturn(scheduledFuture)

        val storage = DefaultSessionStorage(RuntimeEnvironment.application, true, 300, scheduler)
        storage.stop()
        verify(scheduledFuture, never()).cancel(ArgumentMatchers.eq(false))

        storage.start(SessionManager(engine))
        storage.stop()
        verify(scheduledFuture).cancel(ArgumentMatchers.eq(false))
    }

    @Test
    fun deserializeWithInvalidSource() {
        val storage = DefaultSessionStorage(RuntimeEnvironment.application)

        val json = JSONObject()
        json.put("uuid", "testId")
        json.put("url", "testUrl")
        json.put("source", Source.NEW_TAB.name)
        json.put("parentUuid", "")
        assertEquals(Source.NEW_TAB, storage.deserializeSession(json).source)

        val jsonInvalid = JSONObject()
        jsonInvalid.put("uuid", "testId")
        jsonInvalid.put("url", "testUrl")
        jsonInvalid.put("source", "invalidSource")
        jsonInvalid.put("parentUuid", "")
        assertEquals(Source.NONE, storage.deserializeSession(jsonInvalid).source)
    }

    @Suppress("UNCHECKED_CAST")
    private fun <T> anyJson(): T {
        any<T>()
        return null as T
    }
}