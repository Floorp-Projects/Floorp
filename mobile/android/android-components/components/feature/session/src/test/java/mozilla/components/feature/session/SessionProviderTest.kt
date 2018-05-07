/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

package mozilla.components.feature.session

import mozilla.components.browser.session.Session
import mozilla.components.concept.engine.Engine
import mozilla.components.concept.engine.EngineSession
import org.junit.Assert.assertEquals
import org.junit.Test
import org.junit.runner.RunWith
import org.mockito.ArgumentMatchers.any
import org.mockito.ArgumentMatchers.eq
import org.mockito.Mockito.`when`
import org.mockito.Mockito.mock
import org.mockito.Mockito.verify
import org.robolectric.RobolectricTestRunner
import org.robolectric.RuntimeEnvironment
import java.util.concurrent.ScheduledExecutorService
import java.util.concurrent.TimeUnit

@RunWith(RobolectricTestRunner::class)
class SessionProviderTest {

    @Test
    fun testStartRestoresStorage() {
        val storage = mock(SessionStorage::class.java)
        val engine = mock(Engine::class.java)
        val engineSession = mock(EngineSession::class.java)
        val session = Session("http://mozilla.org")
        val sessionList = listOf(SessionProxy(session, engineSession))
        `when`(storage.restore(engine)).thenReturn(sessionList)
        `when`(storage.getSelected()).thenReturn(session)

        val provider = SessionProvider(context = RuntimeEnvironment.application, storage = storage)
        provider.start(engine)

        assertEquals(2, provider.sessionManager.size)
        assertEquals(session, provider.sessionManager.selectedSession)
    }

    @Test
    fun testStartSchedulesPeriodicSaves() {
        val scheduler = mock(ScheduledExecutorService::class.java)

        val provider = SessionProvider(
                context = RuntimeEnvironment.application,
                savePeriodically = true,
                scheduler = scheduler)
        provider.start(mock(Engine::class.java))

        verify(scheduler).scheduleAtFixedRate(any(Runnable::class.java), eq(300L), eq(300L), eq(TimeUnit.SECONDS))
    }

    @Test
    fun testStopShutsDownScheduler() {
        val scheduler = mock(ScheduledExecutorService::class.java)

        val provider = SessionProvider(
                context = RuntimeEnvironment.application,
                savePeriodically = true,
                scheduler = scheduler)
        provider.stop()

        verify(scheduler).shutdown()
    }

    @Test
    fun testGetOrCreateEngineSession() {
        val provider = SessionProvider(RuntimeEnvironment.application)
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
}