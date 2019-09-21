/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

package mozilla.components.feature.session

import mozilla.components.browser.session.Session
import mozilla.components.browser.session.SessionManager
import mozilla.components.concept.engine.EngineSession
import mozilla.components.support.test.mock
import mozilla.components.support.test.whenever
import org.junit.Assert.assertFalse
import org.junit.Assert.assertTrue
import org.junit.Test
import org.mockito.ArgumentMatchers.anyString
import org.mockito.Mockito.never
import org.mockito.Mockito.spy
import org.mockito.Mockito.verify

class FullScreenFeatureTest {

    private val sessionManager: SessionManager = mock()
    private val selectedSession: Session = mock()
    private val useCases = spy(SessionUseCases(sessionManager))

    @Test
    fun `start without a sessionId`() {
        val fullscreenFeature = spy(FullScreenFeature(sessionManager, useCases) {})

        fullscreenFeature.start()

        verify(fullscreenFeature).observeIdOrSelected(null)
        verify(sessionManager, never()).findSessionById(anyString())
        verify(fullscreenFeature).observeSelected()
    }

    @Test
    fun `start with a sessionId`() {
        val fullscreenFeature = spy(FullScreenFeature(sessionManager, useCases, "abc") {})
        whenever(sessionManager.findSessionById(anyString())).thenReturn(selectedSession)

        fullscreenFeature.start()

        verify(fullscreenFeature).observeIdOrSelected(anyString())
        verify(sessionManager).findSessionById(anyString())
        verify(fullscreenFeature).observeFixed(selectedSession)
    }

    @Test
    fun `invoke listener when fullscreen observer invoked`() {
        var fullscreenChanged = false
        val fullscreenFeature = spy(FullScreenFeature(sessionManager, useCases) { b -> fullscreenChanged = b })

        fullscreenFeature.onFullScreenChanged(mock(), true)

        assertTrue(fullscreenChanged)
    }

    @Test
    fun `onBackPressed returns false with no activeSession`() {
        val fullscreenFeature = spy(FullScreenFeature(sessionManager, useCases) {})

        assertFalse(fullscreenFeature.onBackPressed())
    }

    @Test
    fun `onBackPressed returns false with activeSession but not in fullscreen mode`() {
        val fullscreenFeature = spy(FullScreenFeatureTest(sessionManager, useCases) {})
        val activeSession: Session = mock()

        whenever(fullscreenFeature.activeSession).thenReturn(activeSession)
        whenever(activeSession.fullScreenMode).thenReturn(false)

        assertFalse(fullscreenFeature.onBackPressed())
    }

    @Test
    fun `onBackPressed returns true with activeSession and fullscreen mode`() {
        val activeSession: Session = mock()
        val engineSession: EngineSession = mock()
        val fullscreenFeature = spy(FullScreenFeatureTest(sessionManager, useCases) {})

        whenever(sessionManager.getOrCreateEngineSession(activeSession)).thenReturn(engineSession)
        whenever(fullscreenFeature.activeSession).thenReturn(activeSession)
        whenever(activeSession.fullScreenMode).thenReturn(true)
        whenever(useCases.exitFullscreen).thenReturn(mock())

        assertTrue(fullscreenFeature.onBackPressed())
        verify(useCases.exitFullscreen).invoke(activeSession)
    }

    private class FullScreenFeatureTest(
        sessionManager: SessionManager,
        sessionUseCases: SessionUseCases,
        sessionId: String? = null,
        fullScreenChanged: (Boolean) -> Unit
    ) : FullScreenFeature(sessionManager, sessionUseCases, sessionId, fullScreenChanged) {
        public override var activeSession: Session? = null
    }
}
