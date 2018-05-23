/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

package mozilla.components.feature.session

import android.util.AtomicFile
import mozilla.components.browser.session.Session
import mozilla.components.concept.engine.Engine
import mozilla.components.concept.engine.EngineSession
import org.json.JSONException
import org.junit.Assert.assertEquals
import org.junit.Assert.assertFalse
import org.junit.Assert.assertTrue
import org.junit.Test
import org.junit.runner.RunWith
import org.mockito.ArgumentMatchers.any
import org.mockito.ArgumentMatchers.anyString
import org.mockito.Mockito.`when`
import org.mockito.Mockito.doReturn
import org.mockito.Mockito.doThrow
import org.mockito.Mockito.mock
import org.mockito.Mockito.spy
import org.robolectric.RobolectricTestRunner
import org.robolectric.RuntimeEnvironment
import java.io.FileNotFoundException
import java.io.IOException

@RunWith(RobolectricTestRunner::class)
class DefaultSessionStorageTest {

    @Test
    fun testPersistAndRestore() {
        val session = Session("http://mozilla.org")
        val engineSession = mock(EngineSession::class.java)

        val engine = mock(Engine::class.java)
        `when`(engine.createSession()).thenReturn(engineSession)

        val storage = DefaultSessionStorage(RuntimeEnvironment.application)
        val persisted = storage.persist(mapOf(session to engineSession), session.id)
        assertTrue(persisted)

        val (sessions, selectedSession) = storage.restore(engine)
        assertEquals(1, sessions.size)
        assertEquals(session.url, sessions.keys.first().url)
        assertEquals(session.id, selectedSession)
    }

    @Test
    fun testRestoreReturnsEmptyOnIOException() {
        val engine = mock(Engine::class.java)
        val context = spy(RuntimeEnvironment.application)
        val storage = spy(DefaultSessionStorage(context))
        val file = mock(AtomicFile::class.java)

        doReturn(file).`when`(storage).getFile()
        doThrow(FileNotFoundException::class.java).`when`(file).openRead()

        val (sessions, selectedSession) = storage.restore(engine)
        assertEquals(0, sessions.size)
        assertEquals("", selectedSession)
    }

    @Test
    fun testRestoreReturnsEmptyOnJSONException() {
        val session = Session("http://mozilla.org")
        val engineSession = mock(EngineSession::class.java)

        val engine = mock(Engine::class.java)
        `when`(engine.createSession()).thenReturn(engineSession)

        val storage = spy(DefaultSessionStorage(RuntimeEnvironment.application))
        val persisted = storage.persist(mapOf(session to engineSession), session.id)
        assertTrue(persisted)

        doThrow(JSONException::class.java).`when`(storage).deserializeSession(anyString(), anyJson())

        val (sessions, selectedSession) = storage.restore(engine)
        assertEquals(0, sessions.size)
        assertEquals("", selectedSession)
    }

    @Test
    fun testPersistReturnsFalseOnIOException() {
        val session = Session("http://mozilla.org")
        val engineSession = mock(EngineSession::class.java)
        val context = spy(RuntimeEnvironment.application)
        val storage = spy(DefaultSessionStorage(context))
        val file = mock(AtomicFile::class.java)

        doReturn(file).`when`(storage).getFile()
        doThrow(IOException::class.java).`when`(file).startWrite()

        assertFalse(storage.persist(mapOf(session to engineSession), session.id))
    }

    @Test
    fun testPersistReturnsFalseOnJSONException() {
        val session = Session("http://mozilla.org")
        val engineSession = mock(EngineSession::class.java)
        val context = spy(RuntimeEnvironment.application)
        val storage = spy(DefaultSessionStorage(context))
        val file = mock(AtomicFile::class.java)

        doReturn(file).`when`(storage).getFile()
        doThrow(JSONException::class.java).`when`(storage).serializeSession(session)

        assertFalse(storage.persist(mapOf(session to engineSession), session.id))
    }

    private fun <T> anyJson(): T {
        any<T>()
        return null as T
    }
}