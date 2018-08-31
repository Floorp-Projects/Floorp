/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

package mozilla.components.browser.engine.gecko

import org.junit.Test
import org.junit.runner.RunWith
import org.mockito.Mockito.`when`
import org.mockito.Mockito.mock
import org.mockito.Mockito.times
import org.mockito.Mockito.verify
import org.mozilla.geckoview.GeckoSession
import org.mozilla.geckoview.GeckoView
import org.robolectric.RobolectricTestRunner
import org.robolectric.RuntimeEnvironment

@RunWith(RobolectricTestRunner::class)
class GeckoEngineViewTest {

    @Test
    fun testRender() {
        val engineView = GeckoEngineView(RuntimeEnvironment.application)
        val engineSession = mock(GeckoEngineSession::class.java)
        val geckoSession = mock(GeckoSession::class.java)
        val geckoView = mock(GeckoView::class.java)

        `when`(engineSession.geckoSession).thenReturn(geckoSession)
        engineView.currentGeckoView = geckoView

        engineView.render(engineSession)
        verify(geckoView, times(1)).setSession(geckoSession)

        `when`(geckoView.session).thenReturn(geckoSession)
        engineView.render(engineSession)
        verify(geckoView, times(1)).setSession(geckoSession)
    }

    @Test
    fun testLifecycleMethods() {
        val geckoView = mock(GeckoView::class.java)
        val geckoSession = mock(GeckoSession::class.java)
        val engineView = GeckoEngineView(RuntimeEnvironment.application)
        `when`(geckoView.session).thenReturn(geckoSession)
        engineView.currentGeckoView = geckoView

        engineView.onStart()
        verify(geckoSession).setActive(true)

        engineView.onStop()
        verify(geckoSession).setActive(false)
    }
}
