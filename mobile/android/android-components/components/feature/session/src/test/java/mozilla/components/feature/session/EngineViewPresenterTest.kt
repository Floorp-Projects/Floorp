/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

package mozilla.components.feature.session

import mozilla.components.browser.session.SessionManager
import mozilla.components.concept.engine.Engine
import mozilla.components.concept.engine.EngineView
import org.junit.Test
import org.mockito.Mockito.mock
import org.mockito.Mockito.verify

class EngineViewPresenterTest {
    private val sessionManager = mock(SessionManager::class.java)
    private val engine = mock(Engine::class.java)
    private val engineView = mock(EngineView::class.java)
    private val sessionMapping = mock(SessionMapping::class.java)

    @Test
    fun testStartRegistersObserver() {
        val engineViewPresenter = EngineViewPresenter(sessionManager, engine, engineView, sessionMapping)
        engineViewPresenter.start()
        verify(sessionManager).register(engineViewPresenter)
    }

    @Test
    fun testStopUnregistersObserver() {
        val engineViewPresenter = EngineViewPresenter(sessionManager, engine, engineView, sessionMapping)
        engineViewPresenter.stop()
        verify(sessionManager).unregister(engineViewPresenter)
    }
}
