/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

package mozilla.components.feature.session

import mozilla.components.browser.session.Session
import mozilla.components.concept.engine.Engine
import mozilla.components.concept.engine.EngineSession
import mozilla.components.concept.session.storage.SessionStorage
import org.junit.Assert.assertEquals
import org.junit.Assert.assertNotNull
import org.junit.Assert.assertNull
import org.junit.Test
import org.junit.runner.RunWith
import org.mockito.ArgumentMatchers.any
import org.mockito.ArgumentMatchers.eq
import org.mockito.Mockito.`when`
import org.mockito.Mockito.mock
import org.mockito.Mockito.never
import org.mockito.Mockito.verify
import org.robolectric.RobolectricTestRunner
import java.util.concurrent.ScheduledExecutorService
import java.util.concurrent.ScheduledFuture
import java.util.concurrent.TimeUnit

@RunWith(RobolectricTestRunner::class)
class SessionProviderTest {

    @Test
    fun testStartRestoresStorage() {
        val storage = mock(SessionStorage::class.java)
        val engine = mock(Engine::class.java)
        val engineSession = mock(EngineSession::class.java)
        val session = Session("http://mozilla.org")
        val sessionMap = mapOf(session to engineSession)

        `when`(storage.restore(engine)).thenReturn(Pair(sessionMap, session.id))

        val provider = SessionProvider(sessionStorage = storage)
        provider.start(engine)

        assertEquals(2, provider.sessionManager.size)
        assertEquals(session, provider.sessionManager.selectedSession)
    }

    @Test
    fun testStartSchedulesPeriodicSaves() {
        val storage = mock(SessionStorage::class.java)
        val engine = mock(Engine::class.java)
        val scheduler = mock(ScheduledExecutorService::class.java)

        `when`(storage. restore(engine)).thenReturn(Pair(emptyMap(), ""))

        val provider = SessionProvider(
                sessionStorage = storage,
                savePeriodically = true,
                scheduler = scheduler)
        provider.start(engine)

        verify(scheduler).scheduleAtFixedRate(any(Runnable::class.java), eq(300L), eq(300L), eq(TimeUnit.SECONDS))
    }

    @Test
    fun testStopCancelsScheduledTask() {
        val engine = mock(Engine::class.java)

        val storage = mock(SessionStorage::class.java)
        `when`(storage. restore(engine)).thenReturn(Pair(emptyMap(), ""))

        val scheduler = mock(ScheduledExecutorService::class.java)
        val scheduledFuture = mock(ScheduledFuture::class.java)
        `when`(scheduler.scheduleAtFixedRate(any(Runnable::class.java), eq(300L), eq(300L), eq(TimeUnit.SECONDS))).thenReturn(scheduledFuture)

        val provider = SessionProvider(
                sessionStorage = storage,
                savePeriodically = true,
                scheduler = scheduler)

        provider.stop()
        verify(scheduledFuture, never()).cancel(eq(false))

        provider.start(engine)
        provider.stop()
        verify(scheduledFuture).cancel(eq(false))
    }

    @Test
    fun testGetOrCreateEngineSession() {
        val provider = SessionProvider()
        val session = mock(Session::class.java)
        val engine = mock(Engine::class.java)
        val engineSession1 = mock (EngineSession::class.java)
        val engineSession2 = mock (EngineSession::class.java)

        `when`(engine.createSession()).thenReturn(engineSession1)
        var actualEngineSession = provider.getOrCreateEngineSession(engine, session)
        assertEquals(engineSession1, actualEngineSession)

        `when`(engine.createSession()).thenReturn(engineSession2)
        actualEngineSession = provider.getOrCreateEngineSession(engine, session)
        // Should still be the original (already created) engine session
        assertEquals(engineSession1, actualEngineSession)
    }

    @Test
    fun testGetEngineSession() {
        val provider = SessionProvider()
        val session = mock(Session::class.java)

        assertNull(provider.getEngineSession(session))

        val engine = mock(Engine::class.java)
        val engineSession = mock (EngineSession::class.java)
        `when`(engine.createSession()).thenReturn(engineSession)
        provider.getOrCreateEngineSession(engine, session)

        assertNotNull(provider.getEngineSession(session))
        assertEquals(engineSession, provider.getEngineSession(session))
    }

    @Test
    fun testSelectedSession() {
        val session = Session("http://mozilla.org")
        val provider = SessionProvider(initialSession = session)
        provider.sessionManager.select(session)

        assertEquals(session, provider.selectedSession)
    }
}