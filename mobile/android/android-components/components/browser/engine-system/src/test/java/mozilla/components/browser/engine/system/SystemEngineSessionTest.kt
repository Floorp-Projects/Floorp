package mozilla.components.browser.engine.system

import android.content.Context
import android.net.Uri
import android.os.Bundle
import android.os.Looper
import android.webkit.WebBackForwardList
import android.webkit.WebChromeClient
import android.webkit.WebResourceRequest
import android.webkit.WebSettings
import android.webkit.WebStorage
import android.webkit.WebView
import android.webkit.WebViewClient
import android.webkit.WebViewDatabase
import androidx.test.core.app.ApplicationProvider.getApplicationContext
import kotlinx.coroutines.runBlocking
import mozilla.components.browser.engine.system.matcher.UrlMatcher
import mozilla.components.browser.errorpages.ErrorType
import mozilla.components.concept.engine.DefaultSettings
import mozilla.components.concept.engine.EngineSession
import mozilla.components.concept.engine.EngineSessionState
import mozilla.components.concept.engine.request.RequestInterceptor
import mozilla.components.support.test.mock
import org.junit.Assert
import org.junit.Assert.assertEquals
import org.junit.Assert.assertFalse
import org.junit.Assert.assertNotNull
import org.junit.Assert.assertNull
import org.junit.Assert.assertTrue
import org.junit.Assert.fail
import org.junit.Test
import org.junit.runner.RunWith
import org.mockito.ArgumentMatchers.any
import org.mockito.ArgumentMatchers.anyString
import org.mockito.ArgumentMatchers.eq
import org.mockito.Mockito.`when`
import org.mockito.Mockito.doReturn
import org.mockito.Mockito.mock
import org.mockito.Mockito.never
import org.mockito.Mockito.spy
import org.mockito.Mockito.verify
import org.robolectric.RobolectricTestRunner
import java.lang.reflect.Modifier

@RunWith(RobolectricTestRunner::class)
class SystemEngineSessionTest {

    @Test
    fun webChromeClientNotifiesObservers() {
        val engineSession = SystemEngineSession(getApplicationContext())
        val engineView = SystemEngineView(getApplicationContext())
        engineView.render(engineSession)

        var observedProgress = 0
        engineSession.register(object : EngineSession.Observer {
            override fun onProgress(progress: Int) { observedProgress = progress }
        })

        engineSession.webView.webChromeClient.onProgressChanged(null, 100)
        assertEquals(100, observedProgress)
    }

    @Test
    fun loadUrl() {
        var loadedUrl: String? = null
        var loadHeaders: Map<String, String>? = null

        val engineSession = spy(SystemEngineSession(getApplicationContext()))
        val webView = spy(object : WebView(getApplicationContext()) {
            override fun loadUrl(url: String?, additionalHttpHeaders: MutableMap<String, String>?) {
                loadedUrl = url
                loadHeaders = additionalHttpHeaders
            }
        })
        engineSession.webView = webView

        engineSession.loadUrl("")
        verify(webView, never()).loadUrl(anyString())

        engineSession.loadUrl("http://mozilla.org")
        verify(webView).loadUrl(eq("http://mozilla.org"), any())

        assertEquals("http://mozilla.org", loadedUrl)

        assertNotNull(loadHeaders)
        assertEquals(1, loadHeaders!!.size)
        assertTrue(loadHeaders!!.containsKey("X-Requested-With"))
        assertEquals("", loadHeaders!!["X-Requested-With"])
    }

    @Test
    fun loadData() {
        val engineSession = spy(SystemEngineSession(getApplicationContext()))
        val webView = mock(WebView::class.java)

        engineSession.loadData("<html><body>Hello!</body></html>")
        verify(webView, never()).loadData(anyString(), eq("text/html"), eq("UTF-8"))

        engineSession.webView = webView

        engineSession.loadData("<html><body>Hello!</body></html>")
        verify(webView).loadData(eq("<html><body>Hello!</body></html>"), eq("text/html"), eq("UTF-8"))

        engineSession.loadData("Hello!", "text/plain", "UTF-8")
        verify(webView).loadData(eq("Hello!"), eq("text/plain"), eq("UTF-8"))

        engineSession.loadData("ahr0cdovl21vemlsbgeub3jn==", "text/plain", "base64")
        verify(webView).loadData(eq("ahr0cdovl21vemlsbgeub3jn=="), eq("text/plain"), eq("base64"))
    }

    @Test
    fun stopLoading() {
        val engineSession = spy(SystemEngineSession(getApplicationContext()))
        val webView = mock(WebView::class.java)

        engineSession.stopLoading()
        verify(webView, never()).stopLoading()

        engineSession.webView = webView

        engineSession.stopLoading()
        verify(webView).stopLoading()
    }

    @Test
    fun reload() {
        val engineSession = spy(SystemEngineSession(getApplicationContext()))
        val webView = mock(WebView::class.java)

        engineSession.reload()
        verify(webView, never()).reload()

        engineSession.webView = webView

        engineSession.reload()
        verify(webView).reload()
    }

    @Test
    fun goBack() {
        val engineSession = spy(SystemEngineSession(getApplicationContext()))
        val webView = mock(WebView::class.java)

        engineSession.goBack()
        verify(webView, never()).goBack()

        engineSession.webView = webView

        engineSession.goBack()
        verify(webView).goBack()
    }

    @Test
    fun goForward() {
        val engineSession = spy(SystemEngineSession(getApplicationContext()))
        val webView = mock(WebView::class.java)

        engineSession.goForward()
        verify(webView, never()).goForward()

        engineSession.webView = webView

        engineSession.goForward()
        verify(webView).goForward()
    }

    @Test
    fun saveState() {
        val engineSession = spy(SystemEngineSession(getApplicationContext()))
        val webView = mock(WebView::class.java)

        engineSession.saveState()
        verify(webView, never()).saveState(any(Bundle::class.java))

        engineSession.webView = webView

        engineSession.saveState()
        verify(webView).saveState(any(Bundle::class.java))
    }

    @Test
    fun saveStateRunsOnMainThread() {
        val engineSession = spy(SystemEngineSession(getApplicationContext()))
        var calledOnMainThread = false
        var saveStateCalled = false
        val webView = object : WebView(getApplicationContext()) {
            override fun saveState(outState: Bundle?): WebBackForwardList? {
                saveStateCalled = true
                calledOnMainThread = Looper.getMainLooper().isCurrentThread
                return null
            }
        }
        engineSession.webView = webView

        engineSession.saveState()
        assertTrue(saveStateCalled)
        assertTrue(calledOnMainThread)
    }

    @Test
    fun restoreState() {
        val engineSession = spy(SystemEngineSession(getApplicationContext()))
        val webView = mock(WebView::class.java)

        try {
            engineSession.restoreState(mock(EngineSessionState::class.java))
            fail("Expected IllegalArgumentException")
        } catch (e: IllegalArgumentException) {}
        engineSession.restoreState(SystemEngineSessionState(Bundle()))
        verify(webView, never()).restoreState(any(Bundle::class.java))

        engineSession.webView = webView

        engineSession.restoreState(SystemEngineSessionState(Bundle()))
        verify(webView).restoreState(any(Bundle::class.java))
    }

    @Test
    fun enableTrackingProtection() {
        SystemEngineView.URL_MATCHER = UrlMatcher(arrayOf(""))

        val engineSession = spy(SystemEngineSession(getApplicationContext()))
        val webView = mock(WebView::class.java)
        `when`(webView.context).thenReturn(getApplicationContext())
        engineSession.webView = webView

        var enabledObserved: Boolean? = null
        engineSession.register(object : EngineSession.Observer {
            override fun onTrackerBlockingEnabledChange(enabled: Boolean) {
                enabledObserved = enabled
            }
        })

        assertNull(engineSession.trackingProtectionPolicy)
        runBlocking { engineSession.enableTrackingProtection() }
        assertEquals(EngineSession.TrackingProtectionPolicy.all(), engineSession.trackingProtectionPolicy)
        assertNotNull(enabledObserved)
        assertTrue(enabledObserved as Boolean)
    }

    @Test
    fun disableTrackingProtection() {
        val engineSession = spy(SystemEngineSession(getApplicationContext()))
        var enabledObserved: Boolean? = null
        engineSession.register(object : EngineSession.Observer {
            override fun onTrackerBlockingEnabledChange(enabled: Boolean) {
                enabledObserved = enabled
            }
        })

        engineSession.trackingProtectionPolicy = EngineSession.TrackingProtectionPolicy.all()

        engineSession.disableTrackingProtection()
        assertNull(engineSession.trackingProtectionPolicy)
        assertNotNull(enabledObserved)
        assertFalse(enabledObserved as Boolean)
    }

    @Test
    fun initSettings() {
        val engineSession = spy(SystemEngineSession(getApplicationContext()))
        assertNotNull(engineSession.internalSettings)

        val webViewSettings = mock(WebSettings::class.java)
        `when`(webViewSettings.displayZoomControls).thenReturn(true)
        `when`(webViewSettings.allowContentAccess).thenReturn(true)
        `when`(webViewSettings.allowFileAccess).thenReturn(true)
        `when`(webViewSettings.mediaPlaybackRequiresUserGesture).thenReturn(true)
        `when`(webViewSettings.supportMultipleWindows()).thenReturn(false)

        val webView = mock(WebView::class.java)
        `when`(webView.context).thenReturn(getApplicationContext())
        `when`(webView.settings).thenReturn(webViewSettings)
        `when`(webView.isVerticalScrollBarEnabled).thenReturn(true)
        `when`(webView.isHorizontalScrollBarEnabled).thenReturn(true)
        engineSession.webView = webView

        assertFalse(engineSession.settings.javascriptEnabled)
        engineSession.settings.javascriptEnabled = true
        verify(webViewSettings).javaScriptEnabled = true

        assertFalse(engineSession.settings.domStorageEnabled)
        engineSession.settings.domStorageEnabled = true
        verify(webViewSettings).domStorageEnabled = true

        assertNull(engineSession.settings.userAgentString)
        engineSession.settings.userAgentString = "userAgent"
        verify(webViewSettings).userAgentString = "userAgent"

        assertTrue(engineSession.settings.mediaPlaybackRequiresUserGesture)
        engineSession.settings.mediaPlaybackRequiresUserGesture = false
        verify(webViewSettings).mediaPlaybackRequiresUserGesture = false

        assertFalse(engineSession.settings.javaScriptCanOpenWindowsAutomatically)
        engineSession.settings.javaScriptCanOpenWindowsAutomatically = true
        verify(webViewSettings).javaScriptCanOpenWindowsAutomatically = true

        assertTrue(engineSession.settings.displayZoomControls)
        engineSession.settings.javaScriptCanOpenWindowsAutomatically = false
        verify(webViewSettings).javaScriptCanOpenWindowsAutomatically = false

        assertFalse(engineSession.settings.loadWithOverviewMode)
        engineSession.settings.loadWithOverviewMode = true
        verify(webViewSettings).loadWithOverviewMode = true

        assertTrue(engineSession.settings.allowContentAccess)
        engineSession.settings.allowContentAccess = false
        verify(webViewSettings).allowContentAccess = false

        assertTrue(engineSession.settings.allowFileAccess)
        engineSession.settings.allowFileAccess = false
        verify(webViewSettings).allowFileAccess = false

        assertFalse(engineSession.settings.allowUniversalAccessFromFileURLs)
        engineSession.settings.allowUniversalAccessFromFileURLs = true
        verify(webViewSettings).allowUniversalAccessFromFileURLs = true

        assertFalse(engineSession.settings.allowFileAccessFromFileURLs)
        engineSession.settings.allowFileAccessFromFileURLs = true
        verify(webViewSettings).allowFileAccessFromFileURLs = true

        assertTrue(engineSession.settings.verticalScrollBarEnabled)
        engineSession.settings.verticalScrollBarEnabled = false
        verify(webView).isVerticalScrollBarEnabled = false

        assertTrue(engineSession.settings.horizontalScrollBarEnabled)
        engineSession.settings.horizontalScrollBarEnabled = false
        verify(webView).isHorizontalScrollBarEnabled = false

        assertFalse(engineSession.settings.supportMultipleWindows)
        engineSession.settings.supportMultipleWindows = true
        verify(webViewSettings).setSupportMultipleWindows(true)

        assertTrue(engineSession.webFontsEnabled)
        assertTrue(engineSession.settings.webFontsEnabled)
        engineSession.settings.webFontsEnabled = false
        assertFalse(engineSession.webFontsEnabled)
        assertFalse(engineSession.settings.webFontsEnabled)

        assertNull(engineSession.settings.trackingProtectionPolicy)
        engineSession.settings.trackingProtectionPolicy = EngineSession.TrackingProtectionPolicy.all()
        verify(engineSession).enableTrackingProtection(EngineSession.TrackingProtectionPolicy.all())

        engineSession.settings.trackingProtectionPolicy = null
        verify(engineSession).disableTrackingProtection()

        verify(webViewSettings).setAppCacheEnabled(false)
        verify(webViewSettings).setGeolocationEnabled(false)
        verify(webViewSettings).databaseEnabled = false
        verify(webViewSettings).savePassword = false
        verify(webViewSettings).saveFormData = false
        verify(webViewSettings).builtInZoomControls = true
    }

    @Test
    fun defaultSettings() {
        val defaultSettings = DefaultSettings(
                javascriptEnabled = false,
                domStorageEnabled = false,
                webFontsEnabled = false,
                trackingProtectionPolicy = EngineSession.TrackingProtectionPolicy.all(),
                userAgentString = "userAgent",
                mediaPlaybackRequiresUserGesture = false,
                javaScriptCanOpenWindowsAutomatically = true,
                displayZoomControls = false,
                loadWithOverviewMode = true,
                supportMultipleWindows = true)
        val engineSession = spy(SystemEngineSession(getApplicationContext(), defaultSettings))
        val webView = mock(WebView::class.java)
        `when`(webView.context).thenReturn(getApplicationContext())
        engineSession.webView = webView

        val webViewSettings = mock(WebSettings::class.java)
        `when`(webView.settings).thenReturn(webViewSettings)

        engineSession.initSettings()
        verify(webViewSettings).domStorageEnabled = false
        verify(webViewSettings).javaScriptEnabled = false
        verify(webViewSettings).userAgentString = "userAgent"
        verify(webViewSettings).mediaPlaybackRequiresUserGesture = false
        verify(webViewSettings).javaScriptCanOpenWindowsAutomatically = true
        verify(webViewSettings).displayZoomControls = false
        verify(webViewSettings).loadWithOverviewMode = true
        verify(webViewSettings).setSupportMultipleWindows(true)
        verify(engineSession).enableTrackingProtection(EngineSession.TrackingProtectionPolicy.all())
        assertFalse(engineSession.webFontsEnabled)
    }

    @Test
    fun sharedFieldsAreVolatile() {
        val internalSettings = SystemEngineSession::class.java.getDeclaredField("internalSettings")
        val webFontsEnabledField = SystemEngineSession::class.java.getDeclaredField("webFontsEnabled")
        val trackingProtectionField = SystemEngineSession::class.java.getDeclaredField("trackingProtectionPolicy")
        val historyTrackingDelegate = SystemEngineSession::class.java.getDeclaredField("historyTrackingDelegate")
        val fullScreenCallback = SystemEngineSession::class.java.getDeclaredField("fullScreenCallback")
        val currentUrl = SystemEngineSession::class.java.getDeclaredField("currentUrl")
        val webView = SystemEngineSession::class.java.getDeclaredField("webView")

        assertTrue(Modifier.isVolatile(internalSettings.modifiers))
        assertTrue(Modifier.isVolatile(webFontsEnabledField.modifiers))
        assertTrue(Modifier.isVolatile(trackingProtectionField.modifiers))
        assertTrue(Modifier.isVolatile(historyTrackingDelegate.modifiers))
        assertTrue(Modifier.isVolatile(fullScreenCallback.modifiers))
        assertTrue(Modifier.isVolatile(currentUrl.modifiers))
        assertTrue(Modifier.isVolatile(webView.modifiers))
    }

    @Test
    fun settingInterceptorToProvideAlternativeContent() {
        var interceptorCalledWithUri: String? = null

        val interceptor = object : RequestInterceptor {
            override fun onLoadRequest(session: EngineSession, uri: String): RequestInterceptor.InterceptionResponse? {
                interceptorCalledWithUri = uri
                return RequestInterceptor.InterceptionResponse.Content("<h1>Hello World</h1>")
            }
        }

        val defaultSettings = DefaultSettings(requestInterceptor = interceptor)

        val engineSession = SystemEngineSession(getApplicationContext(), defaultSettings)
        engineSession.webView = spy(engineSession.webView)
        val engineView = SystemEngineView(getApplicationContext())
        engineView.render(engineSession)

        val request: WebResourceRequest = mock()
        doReturn(Uri.parse("sample:about")).`when`(request).url

        val response = engineSession.webView.webViewClient.shouldInterceptRequest(
                engineSession.webView,
            request)

        assertEquals("sample:about", interceptorCalledWithUri)

        assertNotNull(response)

        assertEquals("<h1>Hello World</h1>", response.data.bufferedReader().use { it.readText() })
        assertEquals("text/html", response.mimeType)
        assertEquals("UTF-8", response.encoding)
    }

    @Test
    fun settingInterceptorToProvideAlternativeUrl() {
        var interceptorCalledWithUri: String? = null

        val interceptor = object : RequestInterceptor {
            override fun onLoadRequest(session: EngineSession, uri: String): RequestInterceptor.InterceptionResponse? {
                interceptorCalledWithUri = uri
                return RequestInterceptor.InterceptionResponse.Url("https://mozilla.org")
            }
        }

        val defaultSettings = DefaultSettings(requestInterceptor = interceptor)

        val engineSession = SystemEngineSession(getApplicationContext(), defaultSettings)
        engineSession.webView = spy(engineSession.webView)
        val engineView = SystemEngineView(getApplicationContext())
        engineView.render(engineSession)

        val request: WebResourceRequest = mock()
        doReturn(Uri.parse("sample:about")).`when`(request).url

        val response = engineSession.webView.webViewClient.shouldInterceptRequest(
                engineSession.webView,
                request)

        assertNull(response)
        assertEquals("sample:about", interceptorCalledWithUri)
        assertEquals("https://mozilla.org", engineSession.webView.url)
    }

    @Test
    fun onLoadRequestWithoutInterceptor() {
        val defaultSettings = DefaultSettings()

        val engineSession = SystemEngineSession(getApplicationContext(), defaultSettings)
        engineSession.webView = spy(engineSession.webView)
        val engineView = SystemEngineView(getApplicationContext())
        engineView.render(engineSession)

        val request: WebResourceRequest = mock()
        doReturn(Uri.parse("sample:about")).`when`(request).url

        val response = engineSession.webView.webViewClient.shouldInterceptRequest(
            engineSession.webView,
            request)

        assertNull(response)
    }

    @Test
    fun onLoadRequestWithInterceptorThatDoesNotIntercept() {
        var interceptorCalledWithUri: String? = null

        val interceptor = object : RequestInterceptor {
            override fun onLoadRequest(session: EngineSession, uri: String): RequestInterceptor.InterceptionResponse? {
                interceptorCalledWithUri = uri
                return null
            }
        }

        val defaultSettings = DefaultSettings(requestInterceptor = interceptor)

        val engineSession = SystemEngineSession(getApplicationContext(), defaultSettings)
        engineSession.webView = spy(engineSession.webView)
        val engineView = SystemEngineView(getApplicationContext())
        engineView.render(engineSession)

        val request: WebResourceRequest = mock()
        doReturn(Uri.parse("sample:about")).`when`(request).url

        val response = engineSession.webView.webViewClient.shouldInterceptRequest(
            engineSession.webView,
            request)

        assertEquals("sample:about", interceptorCalledWithUri)
        assertNull(response)
    }

    @Test
    fun webViewErrorMappingToErrorType() {
        Assert.assertEquals(
            ErrorType.ERROR_UNKNOWN_HOST,
            SystemEngineSession.webViewErrorToErrorType(WebViewClient.ERROR_HOST_LOOKUP)
        )
        Assert.assertEquals(
            ErrorType.ERROR_CONNECTION_REFUSED,
            SystemEngineSession.webViewErrorToErrorType(WebViewClient.ERROR_CONNECT)
        )
        Assert.assertEquals(
            ErrorType.ERROR_CONNECTION_REFUSED,
            SystemEngineSession.webViewErrorToErrorType(WebViewClient.ERROR_IO)
        )
        Assert.assertEquals(
            ErrorType.ERROR_NET_TIMEOUT,
            SystemEngineSession.webViewErrorToErrorType(WebViewClient.ERROR_TIMEOUT)
        )
        Assert.assertEquals(
            ErrorType.ERROR_REDIRECT_LOOP,
            SystemEngineSession.webViewErrorToErrorType(WebViewClient.ERROR_REDIRECT_LOOP)
        )
        Assert.assertEquals(
            ErrorType.ERROR_UNKNOWN_PROTOCOL,
            SystemEngineSession.webViewErrorToErrorType(WebViewClient.ERROR_UNSUPPORTED_SCHEME)
        )
        Assert.assertEquals(
            ErrorType.ERROR_SECURITY_SSL,
            SystemEngineSession.webViewErrorToErrorType(WebViewClient.ERROR_FAILED_SSL_HANDSHAKE)
        )
        Assert.assertEquals(
            ErrorType.ERROR_MALFORMED_URI,
            SystemEngineSession.webViewErrorToErrorType(WebViewClient.ERROR_BAD_URL)
        )
        Assert.assertEquals(
            ErrorType.UNKNOWN,
            SystemEngineSession.webViewErrorToErrorType(WebViewClient.ERROR_TOO_MANY_REQUESTS)
        )
        Assert.assertEquals(
            ErrorType.ERROR_FILE_NOT_FOUND,
            SystemEngineSession.webViewErrorToErrorType(WebViewClient.ERROR_FILE_NOT_FOUND)
        )
        Assert.assertEquals(
            ErrorType.UNKNOWN,
            SystemEngineSession.webViewErrorToErrorType(-500)
        )
    }

    @Test
    fun desktopMode() {
        val userAgentMobile = "Mozilla/5.0 (Linux; Android 9) AppleWebKit/537.36 Mobile Safari/537.36"
        val engineSession = spy(SystemEngineSession(getApplicationContext()))
        val webView = mock(WebView::class.java)
        val webViewSettings = mock(WebSettings::class.java)
        var desktopMode = false
        engineSession.register(object : EngineSession.Observer {
            override fun onDesktopModeChange(enabled: Boolean) {
                desktopMode = enabled
            }
        })

        engineSession.webView = webView
        `when`(webView.settings).thenReturn(webViewSettings)
        `when`(webViewSettings.userAgentString).thenReturn(userAgentMobile)

        engineSession.toggleDesktopMode(true)
        verify(webViewSettings).useWideViewPort = true
        verify(engineSession).toggleDesktopUA(userAgentMobile, true)
        assertTrue(desktopMode)

        engineSession.toggleDesktopMode(true)
        verify(webView, never()).reload()

        engineSession.toggleDesktopMode(true, true)
        verify(webView).reload()
    }

    @Test
    fun desktopModeUA() {
        val userAgentMobile = "Mozilla/5.0 (Linux; Android 9) AppleWebKit/537.36 Mobile Safari/537.36"
        val userAgentDesktop = "Mozilla/5.0 (Linux; diordnA 9) AppleWebKit/537.36 eliboM Safari/537.36"
        val engineSession = spy(SystemEngineSession(getApplicationContext()))

        assertEquals(engineSession.toggleDesktopUA(userAgentMobile, false), userAgentMobile)
        assertEquals(engineSession.toggleDesktopUA(userAgentMobile, true), userAgentDesktop)
    }

    @Test
    fun findAll() {
        val engineSession = spy(SystemEngineSession(getApplicationContext()))
        val webView = mock(WebView::class.java)
        engineSession.findAll("mozilla")
        verify(webView, never()).findAllAsync(anyString())

        engineSession.webView = webView
        var findObserved: String? = null
        engineSession.register(object : EngineSession.Observer {
            override fun onFind(text: String) {
                findObserved = text
            }
        })
        engineSession.findAll("mozilla")
        verify(webView).findAllAsync("mozilla")
        assertEquals("mozilla", findObserved)
    }

    @Test
    fun findNext() {
        val engineSession = spy(SystemEngineSession(getApplicationContext()))
        val webView = mock(WebView::class.java)
        engineSession.findNext(true)
        verify(webView, never()).findNext(any(Boolean::class.java))

        engineSession.webView = webView
        engineSession.findNext(true)
        verify(webView).findNext(true)
    }

    @Test
    fun clearFindMatches() {
        val engineSession = spy(SystemEngineSession(getApplicationContext()))
        val webView = mock(WebView::class.java)
        engineSession.clearFindMatches()
        verify(webView, never()).clearMatches()

        engineSession.webView = webView
        engineSession.clearFindMatches()
        verify(webView).clearMatches()
    }

    @Test
    fun clearDataMakingExpectedCalls() {
        val engineSession = spy(SystemEngineSession(getApplicationContext()))
        val webView = mock(WebView::class.java)
        val webStorage: WebStorage = mock()
        val webViewDatabase: WebViewDatabase = mock()
        val context: Context = getApplicationContext()

        doReturn(webStorage).`when`(engineSession).webStorage()
        doReturn(webViewDatabase).`when`(engineSession).webViewDatabase(context)
        `when`(webView.context).thenReturn(context)
        engineSession.webView = webView

        engineSession.clearData()
        verify(webView).clearFormData()
        verify(webView).clearHistory()
        verify(webView).clearMatches()
        verify(webView).clearSslPreferences()
        verify(webView).clearCache(true)
        verify(webStorage).deleteAllData()
        verify(webViewDatabase).clearHttpAuthUsernamePassword()
    }

    @Test
    fun testExitFullscreenModeWithWebViewAndCallBack() {
        val engineSession = SystemEngineSession(getApplicationContext())
        val engineView = SystemEngineView(getApplicationContext())
        val customViewCallback = mock(WebChromeClient.CustomViewCallback::class.java)

        engineView.render(engineSession)
        engineSession.exitFullScreenMode()
        verify(customViewCallback, never()).onCustomViewHidden()

        engineSession.fullScreenCallback = customViewCallback
        engineSession.exitFullScreenMode()
        verify(customViewCallback).onCustomViewHidden()
    }

    @Test
    fun closeDestroysWebView() {
        val engineSession = SystemEngineSession(getApplicationContext())
        val webView = mock(WebView::class.java)
        engineSession.webView = webView

        engineSession.close()
        verify(webView).destroy()
    }
}