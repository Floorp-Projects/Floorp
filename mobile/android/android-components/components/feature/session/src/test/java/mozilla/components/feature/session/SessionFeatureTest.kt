/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

package mozilla.components.feature.session

import mozilla.components.browser.session.Session
import mozilla.components.browser.session.SessionManager
import mozilla.components.concept.engine.Engine
import mozilla.components.concept.engine.EngineSession
import mozilla.components.concept.engine.EngineView
import org.junit.Before
import org.junit.Test
import org.mockito.Mockito.`when`
import org.mockito.Mockito.mock
import org.mockito.Mockito.never
import org.mockito.Mockito.verify

class SessionFeatureTest {
    private val sessionManager = mock(SessionManager::class.java)
    private val engine = mock(Engine::class.java)
    private val engineView = mock(EngineView::class.java)
    private val sessionProvider = mock(SessionProvider::class.java)
    private val sessionUseCases = SessionUseCases(sessionProvider, engine)

    @Before
    fun setup() {
        `when`(sessionProvider.sessionManager).thenReturn(sessionManager)
    }

    @Test
    fun testStartEngineViewPresenter() {
        val feature = SessionFeature(sessionProvider, sessionUseCases, engine, engineView)
        feature.start()
        verify(sessionManager).register(feature.presenter)
    }

    @Test
    fun testHandleBackPressed() {
        val session = Session("https://www.mozilla.org")
        `when`(sessionProvider.selectedSession).thenReturn(session)

        val engineSession = mock(EngineSession::class.java)
        `when`(sessionProvider.getOrCreateEngineSession(engine, session)).thenReturn(engineSession)

        val feature = SessionFeature(sessionProvider, sessionUseCases, engine, engineView)

        feature.handleBackPressed()
        verify(engineSession, never()).goBack()

        session.canGoBack = true
        feature.handleBackPressed()
        verify(engineSession).goBack()
    }

    @Test
    fun testStopEngineViewPresenter() {
        val feature = SessionFeature(sessionProvider, sessionUseCases, engine, engineView)
        feature.stop()
        verify(sessionManager).unregister(feature.presenter)
    }
}