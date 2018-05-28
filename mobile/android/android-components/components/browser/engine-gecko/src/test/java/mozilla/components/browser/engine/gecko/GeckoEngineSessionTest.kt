/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

package mozilla.components.browser.engine.gecko

import mozilla.components.concept.engine.EngineSession
import org.junit.Assert.assertEquals
import org.junit.Assert.assertNotNull
import org.junit.Assert.assertTrue
import org.junit.Test
import org.junit.runner.RunWith
import org.mockito.Mockito.mock
import org.mozilla.gecko.util.BundleEventListener
import org.mozilla.gecko.util.EventCallback
import org.mozilla.gecko.util.GeckoBundle
import org.mozilla.geckoview.GeckoRuntime
import org.mozilla.geckoview.GeckoSession
import org.robolectric.RobolectricTestRunner

@RunWith(RobolectricTestRunner::class)
class GeckoEngineSessionTest {

    @Test
    fun testEngineSessionInitialization() {
        val runtime = mock(GeckoRuntime::class.java)
        val engineSession = GeckoEngineSession(runtime)

        assertTrue(engineSession.geckoSession.isOpen)
        assertNotNull(engineSession.geckoSession.navigationDelegate)
        assertNotNull(engineSession.geckoSession.progressDelegate)
    }

    @Test
    fun testProgressDelegateNotifiesObservers() {
        val engineSession = GeckoEngineSession(mock(GeckoRuntime::class.java))

        var observedProgress = 0
        var observedLoadingState = false
        var observedSecurityChange = false
        engineSession.register(object : EngineSession.Observer {
            override fun onLoadingStateChange(loading: Boolean) { observedLoadingState = loading }
            override fun onLocationChange(url: String) { }
            override fun onProgress(progress: Int) { observedProgress = progress }
            override fun onNavigationStateChange(canGoBack: Boolean?, canGoForward: Boolean?) { }
            override fun onSecurityChange(secure: Boolean, host: String?, issuer: String?) {
                // We cannot assert on actual parameters as SecurityInfo's fields can't be set
                // from the outside and its constructor isn't accessible either.
                observedSecurityChange = true
            }
        })

        engineSession.geckoSession.progressDelegate.onPageStart(null, "http://mozilla.org")
        assertEquals(PROGRESS_START, observedProgress)
        assertEquals(true, observedLoadingState)

        engineSession.geckoSession.progressDelegate.onPageStop(null, true)
        assertEquals(PROGRESS_STOP, observedProgress)
        assertEquals(false, observedLoadingState)

        val securityInfo = mock(GeckoSession.ProgressDelegate.SecurityInformation::class.java)
        engineSession.geckoSession.progressDelegate.onSecurityChange(null, securityInfo)
        assertTrue(observedSecurityChange)

        observedSecurityChange = false

        engineSession.geckoSession.progressDelegate.onSecurityChange(null, null)
        assertTrue(observedSecurityChange)
    }

    @Test
    fun testNavigationDelegateNotifiesObservers() {
        val engineSession = GeckoEngineSession(mock(GeckoRuntime::class.java))

        var observedUrl = ""
        var observedCanGoBack: Boolean = false
        var observedCanGoForward: Boolean = false
        engineSession.register(object : EngineSession.Observer {
            override fun onLoadingStateChange(loading: Boolean) {}
            override fun onLocationChange(url: String) { observedUrl = url }
            override fun onProgress(progress: Int) { }
            override fun onSecurityChange(secure: Boolean, host: String?, issuer: String?) { }
            override fun onNavigationStateChange(canGoBack: Boolean?, canGoForward: Boolean?) {
                canGoBack?.let { observedCanGoBack = canGoBack }
                canGoForward?.let { observedCanGoForward = canGoForward }
            }
        })

        engineSession.geckoSession.navigationDelegate.onLocationChange(null, "http://mozilla.org")
        assertEquals("http://mozilla.org", observedUrl)

        engineSession.geckoSession.navigationDelegate.onCanGoBack(null, true)
        assertEquals(true, observedCanGoBack)

        engineSession.geckoSession.navigationDelegate.onCanGoForward(null, true)
        assertEquals(true, observedCanGoForward)
    }

    @Test
    fun testLoadUrl() {
        val engineSession = GeckoEngineSession(mock(GeckoRuntime::class.java))
        var loadUriReceived = false
        engineSession.geckoSession.eventDispatcher.registerUiThreadListener(object : BundleEventListener {
            override fun handleMessage(event: String?, message: GeckoBundle?, callback: EventCallback?) {
                loadUriReceived = true
            }
        }, "GeckoView:LoadUri")

        engineSession.loadUrl("http://mozilla.org")
        assertTrue(loadUriReceived)
    }

    @Test
    fun testReload() {
        val engineSession = GeckoEngineSession(mock(GeckoRuntime::class.java))
        engineSession.loadUrl("http://mozilla.org")

        var reloadReceived = false
        engineSession.geckoSession.eventDispatcher.registerUiThreadListener(object : BundleEventListener {
            override fun handleMessage(event: String?, message: GeckoBundle?, callback: EventCallback?) {
                reloadReceived = true
            }
        }, "GeckoView:Reload")

        engineSession.reload()
        assertTrue(reloadReceived)
    }

    @Test
    fun testGoBack() {
        val engineSession = GeckoEngineSession(mock(GeckoRuntime::class.java))
        var eventReceived = false
        engineSession.geckoSession.eventDispatcher.registerUiThreadListener(object : BundleEventListener {
            override fun handleMessage(event: String?, message: GeckoBundle?, callback: EventCallback?) {
                eventReceived = true
            }
        }, "GeckoView:GoBack")

        engineSession.goBack()
        assertTrue(eventReceived)
    }

    @Test
    fun testGoForward() {
        val engineSession = GeckoEngineSession(mock(GeckoRuntime::class.java))
        var eventReceived = false
        engineSession.geckoSession.eventDispatcher.registerUiThreadListener(object : BundleEventListener {
            override fun handleMessage(event: String?, message: GeckoBundle?, callback: EventCallback?) {
                eventReceived = true
            }
        }, "GeckoView:GoForward")

        engineSession.goForward()
        assertTrue(eventReceived)
    }
}