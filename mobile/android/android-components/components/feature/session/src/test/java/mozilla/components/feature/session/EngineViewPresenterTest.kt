/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

package mozilla.components.feature.session

import mozilla.components.browser.session.SessionManager
import mozilla.components.concept.engine.Engine
import mozilla.components.concept.engine.EngineView
import org.junit.Before
import org.junit.Test
import org.mockito.Mockito.`when`
import org.mockito.Mockito.mock
import org.mockito.Mockito.verify

class EngineViewPresenterTest {
    private val sessionManager = mock(SessionManager::class.java)
    private val engine = mock(Engine::class.java)
    private val engineView = mock(EngineView::class.java)
    private val sessionProvider = mock(SessionProvider::class.java)

    @Before
    fun setup() {
        `when`(sessionProvider.sessionManager).thenReturn(sessionManager)
    }

    @Test
    fun testStartRegistersObserver() {
        val engineViewPresenter = EngineViewPresenter(sessionProvider, engine, engineView)
        engineViewPresenter.start()
        verify(sessionManager).register(engineViewPresenter)
    }

    @Test
    fun testStopUnregistersObserver() {
        val engineViewPresenter = EngineViewPresenter(sessionProvider, engine, engineView)
        engineViewPresenter.stop()
        verify(sessionManager).unregister(engineViewPresenter)
    }
}
