package mozilla.components.browser.engine.system

import android.webkit.WebView
import mozilla.components.concept.engine.EngineSession
import org.junit.Assert
import org.junit.Test
import org.junit.runner.RunWith
import org.mockito.Mockito.`when`
import org.mockito.Mockito.mock
import org.mockito.Mockito.spy
import org.mockito.Mockito.verify
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
        })

        engineView.currentWebView.webChromeClient.onProgressChanged(null, 100)
        Assert.assertEquals(100, observedProgress)
    }

    @Test
    fun testLoadUrl() {
        val engineSession = spy(SystemEngineSession())
        val webView = mock(WebView::class.java)
        `when`(engineSession.currentView()).thenReturn(webView)

        engineSession.loadUrl("http://mozilla.org")
        verify(webView).loadUrl("http://mozilla.org")
    }

    @Test
    fun testGoBack() {
        val engineSession = spy(SystemEngineSession())
        val webView = mock(WebView::class.java)
        `when`(engineSession.currentView()).thenReturn(webView)

        engineSession.goBack()
        verify(webView).goBack()
    }

    @Test
    fun testGoForward() {
        val engineSession = spy(SystemEngineSession())
        val webView = mock(WebView::class.java)
        `when`(engineSession.currentView()).thenReturn(webView)

        engineSession.goForward()
        verify(webView).goForward()
    }
}