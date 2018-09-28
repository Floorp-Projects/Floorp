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
        val session1 = Session("http://mozilla.org")
        val session2 = Session("http://getpocket.com")
        val engineSessionState = mutableMapOf("k0" to "v0", "k1" to 1, "k2" to true, "k3" to emptyList<Any>())

        val engineSession = mock(EngineSession::class.java)
        `when`(engineSession.saveState()).thenReturn(engineSessionState)

        val engine = mock(Engine::class.java)
        `when`(engine.name()).thenReturn("gecko")
        `when`(engine.createSession()).thenReturn(mock(EngineSession::class.java))

        // Engine session just for one of the sessions for simplicity.
        val sessionsSnapshot = mutableListOf<SessionWithState>()
        sessionsSnapshot.add(SessionWithState(session1, selected = true, engineSession = engineSession))
        sessionsSnapshot.add(SessionWithState(session2, selected = false))

        // Persist the snapshot
        val storage = DefaultSessionStorage(RuntimeEnvironment.application)
        val persisted = storage.persist(engine, sessionsSnapshot)
        assertTrue(persisted)

        // Read it back
        val restoredSnapshot = storage.read(engine)
        assertNotNull(restoredSnapshot)
        assertEquals(2, restoredSnapshot!!.size)

        assertEquals(session1, restoredSnapshot[0].session)
        assertEquals(session1.url, restoredSnapshot[0].session.url)
        assertEquals(true, restoredSnapshot[0].selected)

        assertEquals(session2, restoredSnapshot[1].session)
        assertEquals(session2.url, restoredSnapshot[1].session.url)
        assertEquals(false, restoredSnapshot[1].selected)

        val restoredEngineSession = restoredSnapshot[0].engineSession
        assertNotNull(restoredEngineSession)
        verify(restoredEngineSession)!!.restoreState(engineSessionState.filter { it.key != "k3" })
    }

    @Test
    fun persistAndRestoreEmptySnapshot() {
        val engine = mock(Engine::class.java)
        `when`(engine.name()).thenReturn("gecko")

        // Persist empty snapshot
        val storage = DefaultSessionStorage(RuntimeEnvironment.application)
        val persisted = storage.persist(engine, listOf())
        assertTrue(persisted)

        // Restore sessions using a fresh SessionManager
        val restoredSnapshot = storage.read(engine)
        assertEquals(0, restoredSnapshot!!.size)
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

        val sessionsSnapshot = listOf(SessionWithState(session, true, engineSession))

        val storage = spy(DefaultSessionStorage(RuntimeEnvironment.application))
        val persisted = storage.persist(engine, sessionsSnapshot)
        assertTrue(persisted)

        doThrow(JSONException::class.java).`when`(storage).deserializeSession(anyString(), anyJson())
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

        val sessionsSnapshot = listOf(SessionWithState(session, true, engineSession))

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

        val sessionsSnapshot = listOf(SessionWithState(session, true, engineSession))

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
        json.put("url", "testUrl")
        json.put("source", Source.NEW_TAB.name)
        assertEquals(Source.NEW_TAB, storage.deserializeSession("testId", json).source)

        val jsonInvalid = JSONObject()
        jsonInvalid.put("url", "testUrl")
        jsonInvalid.put("source", "invalidSource")
        assertEquals(Source.NONE, storage.deserializeSession("testId", jsonInvalid).source)
    }

    @Suppress("UNCHECKED_CAST")
    private fun <T> anyJson(): T {
        any<T>()
        return null as T
    }
}