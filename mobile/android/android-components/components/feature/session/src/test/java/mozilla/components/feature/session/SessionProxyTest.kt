/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

package mozilla.components.feature.session

import mozilla.components.browser.session.Session
import mozilla.components.concept.engine.EngineSession
import org.junit.Assert.assertEquals
import org.junit.Test

class SessionProxyTest {

    @Test
    fun testSessionProxyObservesChanges() {
        val session = Session("")
        val engineSession = object : EngineSession() {
            override fun goBack() { }

            override fun goForward() { }

            override fun reload() { }

            override fun loadUrl(url: String) {
                notifyObservers { onLocationChange(url) }
                notifyObservers { onProgress(100) }
                notifyObservers { onLoadingStateChange(true) }
                notifyObservers { onNavigationStateChange(true, true) }
            }
        }

        // SessionProxy registers as an engine session observer
        SessionProxy(session, engineSession)

        engineSession.loadUrl("http://mozilla.org")
        assertEquals("http://mozilla.org", session.url)
        assertEquals(100, session.progress)
        assertEquals(true, session.loading)
        assertEquals(true, session.canGoForward)
        assertEquals(true, session.canGoBack)
    }
}