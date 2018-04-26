/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

package mozilla.components.browser.engine.system

import mozilla.components.concept.engine.EngineSession
import org.junit.Assert.assertEquals
import org.junit.Assert.assertNotNull
import org.junit.Test
import org.junit.runner.RunWith
import org.robolectric.RobolectricTestRunner
import org.robolectric.RuntimeEnvironment

@RunWith(RobolectricTestRunner::class)
class SystemEngineViewTest {

    @Test
    fun testEngineViewInitialization() {
        val engineView = SystemEngineView(RuntimeEnvironment.application)

        assertNotNull(engineView.currentWebView.webChromeClient)
        assertNotNull(engineView.currentWebView.webViewClient)
        assertEquals(engineView.currentWebView, engineView.getChildAt(0))
    }

    @Test
    fun testWebViewClientNotifiesObservers() {
        val engineSession = SystemEngineSession()
        val engineView = SystemEngineView(RuntimeEnvironment.application)
        engineView.render(engineSession)

        var observedUrl = ""
        var observedLoadingState = false
        var observedCanGoBack = false
        var observedCanGoForward = false
        engineSession.register(object : EngineSession.Observer {
            override fun onLoadingStateChange(loading: Boolean) { observedLoadingState = loading }
            override fun onLocationChange(url: String) { observedUrl = url }
            override fun onProgress(progress: Int) { }
            override fun onNavigationStateChange(canGoBack: Boolean?, canGoForward: Boolean?) {
                observedCanGoBack = true
                observedCanGoForward = true
            }
        })

        engineView.currentWebView.webViewClient.onPageStarted(null, "http://mozilla.org", null)
        assertEquals(true, observedLoadingState)

        observedLoadingState = true
        engineView.currentWebView.webViewClient.onPageFinished(null, "http://mozilla.org")
        assertEquals("http://mozilla.org", observedUrl)
        assertEquals(false, observedLoadingState)
        assertEquals(true, observedCanGoBack)
        assertEquals(true, observedCanGoForward)
    }

    @Test
    fun testWebChromeClientNotifiesObservers() {
        val engineSession = SystemEngineSession()
        val engineView = SystemEngineView(RuntimeEnvironment.application)
        engineView.render(engineSession)

        var observedProgress = 0
        engineSession.register(object : EngineSession.Observer {
            override fun onLoadingStateChange(loading: Boolean) { }
            override fun onLocationChange(url: String) { }
            override fun onProgress(progress: Int) { observedProgress = progress }
            override fun onNavigationStateChange(canGoBack: Boolean?, canGoForward: Boolean?) { }
        })

        engineView.currentWebView.webChromeClient.onProgressChanged(null, 100)
        assertEquals(100, observedProgress)
    }
}