package mozilla.components.browser.engine.system

import android.os.Bundle
import android.webkit.WebSettings
import android.webkit.WebView
import kotlinx.coroutines.experimental.runBlocking
import mozilla.components.browser.engine.system.matcher.UrlMatcher
import mozilla.components.concept.engine.DefaultSettings
import mozilla.components.concept.engine.EngineSession
import org.junit.Assert
import org.junit.Assert.assertEquals
import org.junit.Assert.assertFalse
import org.junit.Assert.assertNotNull
import org.junit.Assert.assertTrue
import org.junit.Assert.fail
import org.junit.Test
import org.junit.runner.RunWith
import org.mockito.ArgumentMatchers.any
import org.mockito.ArgumentMatchers.anyString
import org.mockito.ArgumentMatchers.eq
import org.mockito.Mockito.`when`
import org.mockito.Mockito.mock
import org.mockito.Mockito.spy
import org.mockito.Mockito.verify
import org.mockito.Mockito.never
import org.robolectric.RobolectricTestRunner
import org.robolectric.RuntimeEnvironment

@RunWith(RobolectricTestRunner::class)
class SystemEngineSessionTest {

    @Test
    fun testWebChromeClientNotifiesObservers() {
        val engineSession = SystemEngineSession()
        val engineView = SystemEngineView(RuntimeEnvironment.application)
        engineView.render(engineSession)

        var observedProgress = 0
        engineSession.register(object : EngineSession.Observer {
            override fun onProgress(progress: Int) { observedProgress = progress }
        })

        engineView.currentWebView.webChromeClient.onProgressChanged(null, 100)
        Assert.assertEquals(100, observedProgress)
    }

    @Test
    fun testLoadUrl() {
        val engineSession = spy(SystemEngineSession())
        val webView = mock(WebView::class.java)

        engineSession.loadUrl("")
        verify(webView, never()).loadUrl(anyString())

        `when`(engineSession.currentView()).thenReturn(webView)

        engineSession.loadUrl("http://mozilla.org")
        verify(webView).loadUrl("http://mozilla.org")
    }

    @Test
    fun testLoadData() {
        val engineSession = spy(SystemEngineSession())
        val webView = mock(WebView::class.java)

        engineSession.loadData("<html><body>Hello!</body></html>")
        verify(webView, never()).loadData(anyString(), eq("text/html"), eq("UTF-8"))

        `when`(engineSession.currentView()).thenReturn(webView)

        engineSession.loadData("<html><body>Hello!</body></html>")
        verify(webView).loadData(eq("<html><body>Hello!</body></html>"), eq("text/html"), eq("UTF-8"))

        engineSession.loadData("Hello!", "text/plain", "UTF-8")
        verify(webView).loadData(eq("Hello!"), eq("text/plain"), eq("UTF-8"))

        engineSession.loadData("ahr0cdovl21vemlsbgeub3jn==", "text/plain", "base64")
        verify(webView).loadData(eq("ahr0cdovl21vemlsbgeub3jn=="), eq("text/plain"), eq("base64"))
    }

    @Test
    fun testStopLoading() {
        val engineSession = spy(SystemEngineSession())
        val webView = mock(WebView::class.java)

        engineSession.stopLoading()
        verify(webView, never()).stopLoading()

        `when`(engineSession.currentView()).thenReturn(webView)

        engineSession.stopLoading()
        verify(webView).stopLoading()
    }

    @Test
    fun testReload() {
        val engineSession = spy(SystemEngineSession())
        val webView = mock(WebView::class.java)

        engineSession.reload()
        verify(webView, never()).reload()

        `when`(engineSession.currentView()).thenReturn(webView)

        engineSession.reload()
        verify(webView).reload()
    }

    @Test
    fun testGoBack() {
        val engineSession = spy(SystemEngineSession())
        val webView = mock(WebView::class.java)

        engineSession.goBack()
        verify(webView, never()).goBack()

        `when`(engineSession.currentView()).thenReturn(webView)

        engineSession.goBack()
        verify(webView).goBack()
    }

    @Test
    fun testGoForward() {
        val engineSession = spy(SystemEngineSession())
        val webView = mock(WebView::class.java)

        engineSession.goForward()
        verify(webView, never()).goForward()

        `when`(engineSession.currentView()).thenReturn(webView)

        engineSession.goForward()
        verify(webView).goForward()
    }

    @Test
    fun testSaveState() {
        val engineSession = spy(SystemEngineSession())
        val webView = mock(WebView::class.java)

        engineSession.saveState()
        verify(webView, never()).saveState(any(Bundle::class.java))

        `when`(engineSession.currentView()).thenReturn(webView)

        engineSession.saveState()
        verify(webView).saveState(any(Bundle::class.java))
    }

    @Test
    fun testRestoreState() {
        val engineSession = spy(SystemEngineSession())
        val webView = mock(WebView::class.java)

        engineSession.restoreState(emptyMap())
        verify(webView, never()).restoreState(any(Bundle::class.java))

        `when`(engineSession.currentView()).thenReturn(webView)

        engineSession.restoreState(emptyMap())
        verify(webView).restoreState(any(Bundle::class.java))
    }

    @Test
    fun testEnableTrackingProtection() {
        SystemEngineView.URL_MATCHER = UrlMatcher(arrayOf(""))

        val engineSession = spy(SystemEngineSession())
        val webView = mock(WebView::class.java)
        `when`(webView.context).thenReturn(RuntimeEnvironment.application)
        `when`(engineSession.currentView()).thenReturn(webView)

        var enabledObserved: Boolean? = null
        engineSession.register(object : EngineSession.Observer {
            override fun onTrackerBlockingEnabledChange(enabled: Boolean) {
                enabledObserved = enabled
            }
        })

        assertFalse(engineSession.trackingProtectionEnabled)
        runBlocking { engineSession.enableTrackingProtection() }
        assertTrue(engineSession.trackingProtectionEnabled)
        assertNotNull(enabledObserved)
        assertTrue(enabledObserved!!)
    }

    @Test
    fun testDisableTrackingProtection() {
        val engineSession = spy(SystemEngineSession())
        var enabledObserved: Boolean? = null
        engineSession.register(object : EngineSession.Observer {
            override fun onTrackerBlockingEnabledChange(enabled: Boolean) {
                enabledObserved = enabled
            }
        })

        engineSession.trackingProtectionEnabled = true

        engineSession.disableTrackingProtection()
        assertFalse(engineSession.trackingProtectionEnabled)
        assertNotNull(enabledObserved)
        assertFalse(enabledObserved!!)
    }

    @Test
    fun testSettings() {
        val engineSession = spy(SystemEngineSession())
        val webView = mock(WebView::class.java)
        `when`(webView.context).thenReturn(RuntimeEnvironment.application)
        val webViewSettings = mock(WebSettings::class.java)
        `when`(webViewSettings.javaScriptEnabled).thenReturn(false)
        `when`(webViewSettings.domStorageEnabled).thenReturn(false)
        `when`(webView.settings).thenReturn(webViewSettings)

        try {
            engineSession.settings.javascriptEnabled = true
            fail("Expected IllegalStateException")
        } catch (e: IllegalStateException) { }

        `when`(engineSession.currentView()).thenReturn(webView)
        assertFalse(engineSession.settings.javascriptEnabled)
        engineSession.settings.javascriptEnabled = true
        verify(webViewSettings).javaScriptEnabled = true

        assertFalse(engineSession.settings.domStorageEnabled)
        engineSession.settings.domStorageEnabled = true
        verify(webViewSettings).domStorageEnabled = true

        assertEquals(EngineSession.TrackingProtectionPolicy.none(), engineSession.settings.trackingProtectionPolicy)
        engineSession.settings.trackingProtectionPolicy = EngineSession.TrackingProtectionPolicy.all()
        verify(engineSession).enableTrackingProtection(EngineSession.TrackingProtectionPolicy.all())

        engineSession.settings.trackingProtectionPolicy = null
        verify(engineSession).disableTrackingProtection()
    }

    @Test
    fun testDefaultSettings() {
        val defaultSettings = DefaultSettings(false, false, EngineSession.TrackingProtectionPolicy.all())
        val engineSession = spy(SystemEngineSession(defaultSettings))
        val webView = mock(WebView::class.java)
        `when`(webView.context).thenReturn(RuntimeEnvironment.application)
        `when`(engineSession.currentView()).thenReturn(webView)

        val webViewSettings = mock(WebSettings::class.java)
        `when`(webView.settings).thenReturn(webViewSettings)

        engineSession.initSettings()
        verify(webViewSettings).domStorageEnabled = false
        verify(webViewSettings).javaScriptEnabled = false
        verify(engineSession).enableTrackingProtection(EngineSession.TrackingProtectionPolicy.all())
    }
}