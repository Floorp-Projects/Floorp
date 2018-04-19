/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

package mozilla.components.feature.session

import mozilla.components.browser.session.SessionManager
import mozilla.components.concept.engine.Engine
import mozilla.components.concept.engine.EngineView
import org.junit.Test
import org.mockito.Mockito
import org.mockito.Mockito.verify

class SessionFeatureTest {
    private val sessionManager = Mockito.mock(SessionManager::class.java)
    private val engine = Mockito.mock(Engine::class.java)
    private val engineView = Mockito.mock(EngineView::class.java)
    private val sessionMapping = Mockito.mock(SessionMapping::class.java)

    @Test
    fun testStartEngineViewPresenter() {
        val feature = SessionFeature(sessionManager, engine, engineView, sessionMapping)
        feature.start()
        verify(sessionManager).register(feature.presenter)
    }

    @Test
    fun testStopEngineViewPresenter() {
        val feature = SessionFeature(sessionManager, engine, engineView, sessionMapping)
        feature.stop()
        verify(sessionManager).unregister(feature.presenter)
    }
}