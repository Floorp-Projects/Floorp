/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

package mozilla.components.browser.session.storage

import android.util.AtomicFile
import mozilla.components.browser.session.Session
import mozilla.components.browser.session.SessionManager
import mozilla.components.concept.engine.Engine
import mozilla.components.concept.engine.EngineSession
import org.json.JSONException
import org.junit.Assert.assertEquals
import org.junit.Assert.assertFalse
import org.junit.Assert.assertNotNull
import org.junit.Assert.assertTrue
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
    fun testPersistAndRestore() {
        val session = Session("http://mozilla.org")
        val engineSessionState = mutableMapOf("k0" to "v0", "k1" to 1, "k2" to true, "k3" to emptyList<Any>())

        val engineSession = mock(EngineSession::class.java)
        `when`(engineSession.saveState()).thenReturn(engineSessionState)

        val engine = mock(Engine::class.java)
        `when`(engine.createSession()).thenReturn(mock(EngineSession::class.java))

        val sessionManager = SessionManager(engine)
        sessionManager.add(session, true, engineSession)

        // Persist current sessions
        val storage = DefaultSessionStorage(RuntimeEnvironment.application)
        val persisted = storage.persist(sessionManager)
        assertTrue(persisted)

        // Restore session using a fresh SessionManager
        val restoredSessionManager = SessionManager(engine)
        val restored = storage.restore(engine, restoredSessionManager)
        assertTrue(restored)
        assertEquals(1, restoredSessionManager.sessions.size)

        val restoredSession = restoredSessionManager.sessions.first()
        assertEquals(session, restoredSession)
        assertEquals(session.id, restoredSessionManager.selectedSession.id)
        assertEquals(session.url, restoredSession.url)

        val restoredEngineSession = restoredSessionManager.sessions.first().engineSessionHolder.engineSession
        assertNotNull(restoredEngineSession)
        verify(restoredEngineSession)!!.restoreState(engineSessionState.filter { it.key != "k3" })
    }

    @Test
    fun testRestoreReturnsFalseOnIOException() {
        val engine = mock(Engine::class.java)
        val context = spy(RuntimeEnvironment.application)
        val storage = spy(DefaultSessionStorage(context))
        val file = mock(AtomicFile::class.java)

        doReturn(file).`when`(storage).getFile()
        doThrow(FileNotFoundException::class.java).`when`(file).openRead()

        val restored = storage.restore(engine, SessionManager(engine))
        assertFalse(restored)
    }

    @Test
    fun testRestoreReturnsFalseOnJSONException() {
        val session = Session("http://mozilla.org")
        val engineSession = mock(EngineSession::class.java)

        val engine = mock(Engine::class.java)
        `when`(engine.createSession()).thenReturn(engineSession)

        val sessionManager = SessionManager(engine)
        sessionManager.add(session, true, engineSession)

        val storage = spy(DefaultSessionStorage(RuntimeEnvironment.application))
        val persisted = storage.persist(sessionManager)
        assertTrue(persisted)

        doThrow(JSONException::class.java).`when`(storage).deserializeSession(anyString(), anyJson())
        val restored = storage.restore(engine, SessionManager(engine))
        assertFalse(restored)
    }

    @Test
    fun testPersistReturnsFalseOnIOException() {
        val engine = mock(Engine::class.java)
        val session = Session("http://mozilla.org")
        val engineSession = mock(EngineSession::class.java)
        val context = spy(RuntimeEnvironment.application)
        val storage = spy(DefaultSessionStorage(context))
        val file = mock(AtomicFile::class.java)

        val sessionManager = SessionManager(engine)
        sessionManager.add(session, true, engineSession)

        doReturn(file).`when`(storage).getFile()
        doThrow(IOException::class.java).`when`(file).startWrite()

        val persisted = storage.persist(sessionManager)
        assertFalse(persisted)
    }

    @Test
    fun testPersistReturnsFalseOnJSONException() {
        val engine = mock(Engine::class.java)
        val session = Session("http://mozilla.org")
        val engineSession = mock(EngineSession::class.java)
        val context = spy(RuntimeEnvironment.application)
        val storage = spy(DefaultSessionStorage(context))
        val file = mock(AtomicFile::class.java)

        val sessionManager = SessionManager(engine)
        sessionManager.add(session, true, engineSession)

        doReturn(file).`when`(storage).getFile()
        doThrow(JSONException::class.java).`when`(storage).serializeSession(session)

        val persisted = storage.persist(sessionManager)
        assertFalse(persisted)
    }

    @Test
    fun testStartSchedulesPeriodicSaves() {
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
    fun testStopCancelsScheduledTask() {
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

    @Suppress("UNCHECKED_CAST")
    private fun <T> anyJson(): T {
        any<T>()
        return null as T
    }
}