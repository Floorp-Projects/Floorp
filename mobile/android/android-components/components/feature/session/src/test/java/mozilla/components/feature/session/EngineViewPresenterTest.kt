/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

package mozilla.components.feature.session

import mozilla.components.browser.session.Session
import mozilla.components.browser.session.SessionManager
import mozilla.components.concept.engine.EngineView
import org.junit.Test
import org.mockito.Mockito.`when`
import org.mockito.Mockito.mock
import org.mockito.Mockito.spy
import org.mockito.Mockito.verify

class EngineViewPresenterTest {
    private val sessionManager = mock(SessionManager::class.java)
    private val engineView = mock(EngineView::class.java)
    private val session = mock(Session::class.java)

    @Test
    fun testStartRegistersObserver() {
        val engineViewPresenter = EngineViewPresenter(sessionManager, engineView)
        engineViewPresenter.start()
        verify(sessionManager).register(engineViewPresenter)
    }

    @Test
    fun testStartRendersSession() {
        val testSession = mock(Session::class.java)
        `when`(sessionManager.selectedSession).thenReturn(session)
        `when`(sessionManager.findSessionById("testSession")).thenReturn(testSession)

        var engineViewPresenter = spy(EngineViewPresenter(sessionManager, engineView))
        engineViewPresenter.start()
        verify(engineViewPresenter).renderSession(session)

        engineViewPresenter = spy(EngineViewPresenter(sessionManager, engineView, "testSession"))
        engineViewPresenter.start()
        verify(engineViewPresenter).renderSession(testSession)
    }

    @Test
    fun testStopUnregistersObserver() {
        val engineViewPresenter = EngineViewPresenter(sessionManager, engineView)
        engineViewPresenter.stop()
        verify(sessionManager).unregister(engineViewPresenter)
    }
}
