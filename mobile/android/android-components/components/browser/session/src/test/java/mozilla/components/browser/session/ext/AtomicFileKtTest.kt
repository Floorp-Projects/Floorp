/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

package mozilla.components.browser.session.ext

import android.util.AtomicFile
import androidx.test.ext.junit.runners.AndroidJUnit4
import mozilla.components.browser.session.Session
import mozilla.components.browser.session.SessionManager
import mozilla.components.browser.session.storage.SnapshotSerializer
import mozilla.components.browser.session.storage.getFileForEngine
import mozilla.components.concept.engine.Engine
import mozilla.components.concept.engine.EngineSession
import mozilla.components.concept.engine.EngineSessionState
import mozilla.components.support.test.any
import mozilla.components.support.test.mock
import mozilla.components.support.test.robolectric.testContext
import org.json.JSONObject
import org.junit.Assert.assertEquals
import org.junit.Assert.assertNotEquals
import org.junit.Assert.assertNotNull
import org.junit.Assert.assertNull
import org.junit.Test
import org.junit.runner.RunWith
import org.mockito.Mockito.doThrow
import org.mockito.Mockito.mock
import org.mockito.Mockito.verify
import org.mockito.Mockito.`when`
import java.io.File
import java.io.FileNotFoundException
import java.io.IOException
import java.util.UUID

@RunWith(AndroidJUnit4::class)
class AtomicFileKtTest {

    @Test
    fun `writeSnapshot - Fails write on IOException`() {
        val file: AtomicFile = mock()
        doThrow(IOException::class.java).`when`(file).startWrite()

        val snapshot = SessionManager.Snapshot(
            sessions = listOf(
                SessionManager.Snapshot.Item(Session("http://mozilla.org"))
            ),
            selectedSessionIndex = 0
        )

        file.writeSnapshot(snapshot, SnapshotSerializer())

        verify(file).failWrite(any())
    }

    @Test
    fun `readSnapshot - Returns null on FileNotFoundException`() {
        val file: AtomicFile = mock()
        doThrow(FileNotFoundException::class.java).`when`(file).openRead()

        val snapshot = file.readSnapshot(engine = mock(), serializer = SnapshotSerializer())
        assertNull(snapshot)
    }

    @Test
    fun `readSnapshot - Returns null on corrupt JSON`() {
        val file = getFileForEngine(testContext, engine = mock())

        val stream = file.startWrite()
        stream.bufferedWriter().write("{ name: 'Foo")
        file.finishWrite(stream)

        val snapshot = file.readSnapshot(engine = mock(), serializer = SnapshotSerializer())
        assertNull(snapshot)
    }

    @Test
    fun `Read snapshot should contain sessions of written snapshot`() {
        val session1 = Session("http://mozilla.org", id = "session1")
        val session2 = Session("http://getpocket.com", id = "session2")
        val session3 = Session("http://getpocket.com", id = "session3")
        session3.parentId = "session1"

        val engineSessionState = object : EngineSessionState {
            override fun toJSON() = JSONObject()
        }

        val engineSession = mock(EngineSession::class.java)
        `when`(engineSession.saveState()).thenReturn(engineSessionState)

        val engine = mock(Engine::class.java)
        `when`(engine.name()).thenReturn("gecko")
        `when`(engine.createSession()).thenReturn(mock(EngineSession::class.java))
        `when`(engine.createSessionState(any())).thenReturn(engineSessionState)

        // Engine session just for one of the sessions for simplicity.
        val sessionsSnapshot = SessionManager.Snapshot(
            sessions = listOf(
                SessionManager.Snapshot.Item(session1),
                SessionManager.Snapshot.Item(session2),
                SessionManager.Snapshot.Item(session3)
            ),
            selectedSessionIndex = 0
        )

        val file = AtomicFile(File.createTempFile(
            UUID.randomUUID().toString(),
            UUID.randomUUID().toString()))

        file.writeSnapshot(sessionsSnapshot)

        // Read it back
        val restoredSnapshot = file.readSnapshot(engine)
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

        val restoredEngineSession = restoredSnapshot.sessions[0].engineSessionState
        assertNotNull(restoredEngineSession)
    }

    @Test
    fun `Read snapshot item should contain session of written snapshot item`() {
        val session = Session("https://www.mozilla.org", id = "session")
        session.parentId = "parent-session"

        val engine = mock(Engine::class.java)
        val file = writeSnapshotItem(engine, session)

        val restoredItem = file.readSnapshotItem(engine)
        assertNotNull(restoredItem!!)
        assertNotNull(restoredItem.engineSessionState!!)

        assertEquals("session", restoredItem.session.id)
        assertEquals("parent-session", restoredItem.session.parentId)
        assertEquals("https://www.mozilla.org", restoredItem.session.url)
    }

    @Test
    fun `Read snapshot item without restoring session ID`() {
        val session = Session("https://www.mozilla.org", id = "session")

        val engine = mock(Engine::class.java)
        val file = writeSnapshotItem(engine, session)

        val restoredItem = file.readSnapshotItem(engine, restoreSessionId = false)
        assertNotNull(restoredItem!!)
        assertNotNull(restoredItem.engineSessionState!!)

        assertNotEquals("session", restoredItem.session.id)
        assertEquals("https://www.mozilla.org", restoredItem.session.url)
    }

    @Test
    fun `Read snapshot item without restoring parent ID`() {
        val session = Session("https://www.mozilla.org", id = "session")
        session.parentId = "parent-session"

        val engine = mock(Engine::class.java)
        val file = writeSnapshotItem(engine, session)

        val restoredItem = file.readSnapshotItem(engine, restoreSessionId = true, restoreParentId = false)
        assertNotNull(restoredItem!!)
        assertNotNull(restoredItem.engineSessionState!!)

        assertEquals("session", restoredItem.session.id)
        assertNull(restoredItem.session.parentId)
        assertEquals("https://www.mozilla.org", restoredItem.session.url)
    }

    private fun writeSnapshotItem(engine: Engine, session: Session): AtomicFile {
        val engineSessionState = object : EngineSessionState {
            override fun toJSON() = JSONObject()
        }

        val engineSession = mock(EngineSession::class.java)
        `when`(engineSession.saveState()).thenReturn(engineSessionState)

        `when`(engine.name()).thenReturn("gecko")
        `when`(engine.createSession()).thenReturn(mock(EngineSession::class.java))
        `when`(engine.createSessionState(any())).thenReturn(engineSessionState)

        val item = SessionManager.Snapshot.Item(session, engineSession)

        val file = AtomicFile(File.createTempFile(
                UUID.randomUUID().toString(),
                UUID.randomUUID().toString()))

        file.writeSnapshotItem(item)
        return file
    }
}