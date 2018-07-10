/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

package mozilla.components.feature.session

import mozilla.components.browser.session.Session
import mozilla.components.browser.session.SessionManager
import mozilla.components.browser.session.storage.SessionStorage
import mozilla.components.concept.engine.EngineSession
import mozilla.components.concept.engine.EngineView
import org.junit.Test
import org.mockito.Mockito.`when`
import org.mockito.Mockito.mock
import org.mockito.Mockito.never
import org.mockito.Mockito.verify

class SessionFeatureTest {
    private val sessionManager = mock(SessionManager::class.java)
    private val engineView = mock(EngineView::class.java)
    private val sessionStorage = mock(SessionStorage::class.java)
    private val sessionUseCases = SessionUseCases(sessionManager)

    @Test
    fun testStart() {
        val feature = SessionFeature(sessionManager, sessionUseCases, engineView, sessionStorage)
        feature.start()
        verify(sessionManager).register(feature.presenter)
        verify(sessionStorage).start(sessionManager)
    }

    @Test
    fun testHandleBackPressed() {
        val session = Session("https://www.mozilla.org")
        `when`(sessionManager.selectedSession).thenReturn(session)
        `when`(sessionManager.selectedSessionOrThrow).thenReturn(session)

        val engineSession = mock(EngineSession::class.java)
        `when`(sessionManager.getOrCreateEngineSession(session)).thenReturn(engineSession)

        val feature = SessionFeature(sessionManager, sessionUseCases, engineView)

        feature.handleBackPressed()
        verify(engineSession, never()).goBack()

        session.canGoBack = true
        feature.handleBackPressed()
        verify(engineSession).goBack()
    }

    @Test
    fun testStop() {
        val feature = SessionFeature(sessionManager, sessionUseCases, engineView, sessionStorage)
        feature.stop()
        verify(sessionManager).unregister(feature.presenter)
        verify(sessionStorage).stop()
    }
}