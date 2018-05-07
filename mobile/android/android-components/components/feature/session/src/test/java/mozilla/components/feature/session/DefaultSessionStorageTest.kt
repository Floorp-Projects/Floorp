/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

package mozilla.components.feature.session

import mozilla.components.browser.session.Session
import mozilla.components.concept.engine.Engine
import mozilla.components.concept.engine.EngineSession
import org.junit.Assert.assertEquals
import org.junit.Assert.assertNotNull
import org.junit.Assert.assertNull
import org.junit.Assert.assertTrue
import org.junit.Test
import org.junit.runner.RunWith
import org.mockito.Mockito.`when`
import org.mockito.Mockito.mock
import org.robolectric.RobolectricTestRunner
import org.robolectric.RuntimeEnvironment

@RunWith(RobolectricTestRunner::class)
class DefaultSessionStorageTest {

    @Test
    fun testAddAndGet() {
        val session = SessionProxy(mock(Session::class.java), mock(EngineSession::class.java))

        val storage = DefaultSessionStorage(RuntimeEnvironment.application)
        val id = storage.add(session)

        assertNotNull(id)
        assertEquals(session, storage.get(id))
    }

    @Test
    fun testRemoveAndGet() {
        val session = SessionProxy(mock(Session::class.java), mock(EngineSession::class.java))

        val storage = DefaultSessionStorage(RuntimeEnvironment.application)
        val id = storage.add(session)

        assertNotNull(storage.get(id))
        assertTrue(storage.remove(id))
        assertNull(storage.get(id))
    }

    @Test
    fun testPersistAndRestore() {
        val session = Session("http://mozilla.org")

        val engine = mock(Engine::class.java)
        `when`(engine.createSession()).thenReturn(mock(EngineSession::class.java)
        )
        val storage = DefaultSessionStorage(RuntimeEnvironment.application)
        val id = storage.add(SessionProxy(session, mock(EngineSession::class.java)))

        assertTrue(storage.persist(session))

        val sessions = storage.restore(engine)
        assertEquals(1, sessions.size)
        assertEquals(session.url, sessions[0].session.url)
        assertEquals(session.url, storage.get(id)?.session?.url)
    }
}