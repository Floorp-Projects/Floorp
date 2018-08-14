package mozilla.components.browser.engine.system

import android.os.Bundle
import android.webkit.WebView
import mozilla.components.concept.engine.EngineSession
import org.junit.Assert
import org.junit.Test
import org.junit.runner.RunWith
import org.mockito.ArgumentMatchers.any
import org.mockito.ArgumentMatchers.anyString
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
            override fun onLoadingStateChange(loading: Boolean) { }
            override fun onLocationChange(url: String) { }
            override fun onProgress(progress: Int) { observedProgress = progress }
            override fun onNavigationStateChange(canGoBack: Boolean?, canGoForward: Boolean?) { }
            override fun onSecurityChange(secure: Boolean, host: String?, issuer: String?) {}
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
}