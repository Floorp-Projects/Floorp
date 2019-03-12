/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

package mozilla.components.browser.engine.system

import android.content.Context
import android.graphics.Bitmap
import android.net.Uri
import android.net.http.SslCertificate
import android.net.http.SslError
import android.os.Bundle
import android.os.Message
import android.view.View
import android.webkit.JsPromptResult
import android.webkit.JsResult
import android.webkit.SslErrorHandler
import android.webkit.WebChromeClient
import android.webkit.WebResourceError
import android.webkit.WebResourceRequest
import android.webkit.WebView
import android.webkit.ValueCallback
import android.webkit.WebChromeClient.FileChooserParams.MODE_OPEN_MULTIPLE
import android.webkit.WebViewClient
import android.webkit.WebView.HitTestResult
import androidx.test.core.app.ApplicationProvider
import androidx.test.core.app.ApplicationProvider.getApplicationContext
import kotlinx.coroutines.runBlocking
import mozilla.components.browser.engine.system.SystemEngineView.Companion.MAX_SUCCESSIVE_DIALOG_COUNT
import mozilla.components.browser.engine.system.SystemEngineView.Companion.MAX_SUCCESSIVE_DIALOG_SECONDS_LIMIT
import mozilla.components.browser.engine.system.matcher.UrlMatcher
import mozilla.components.browser.errorpages.ErrorType
import mozilla.components.concept.engine.EngineSession
import mozilla.components.concept.engine.EngineSession.TrackingProtectionPolicy
import mozilla.components.concept.engine.HitResult
import mozilla.components.concept.engine.history.HistoryTrackingDelegate
import mozilla.components.concept.engine.permission.PermissionRequest
import mozilla.components.concept.engine.prompt.PromptRequest
import mozilla.components.concept.engine.request.RequestInterceptor
import mozilla.components.concept.engine.window.WindowRequest
import mozilla.components.support.test.eq
import mozilla.components.support.test.mock
import org.junit.Assert.assertEquals
import org.junit.Assert.assertFalse
import org.junit.Assert.assertNotEquals
import org.junit.Assert.assertNotNull
import org.junit.Assert.assertNull
import org.junit.Assert.assertTrue
import org.junit.Assert.fail
import org.junit.Test
import org.junit.runner.RunWith
import org.mockito.Mockito.`when`
import org.mockito.Mockito.doReturn
import org.mockito.Mockito.mock
import org.mockito.Mockito.never
import org.mockito.Mockito.reset
import org.mockito.Mockito.times
import org.mockito.Mockito.verify
import org.mockito.Mockito.verifyZeroInteractions
import org.robolectric.RobolectricTestRunner
import org.robolectric.RuntimeEnvironment
import java.util.Calendar
import java.util.Calendar.SECOND
import java.util.Calendar.YEAR
import java.util.Date

@RunWith(RobolectricTestRunner::class)
class SystemEngineViewTest {

    @Test
    fun `EngineView initialization`() {
        val engineView = SystemEngineView(getApplicationContext())
        val webView = WebView(getApplicationContext())

        engineView.initWebView(webView)
        assertNotNull(webView.webChromeClient)
        assertNotNull(webView.webViewClient)
    }

    @Test
    fun `EngineView renders WebView`() {
        val engineSession = SystemEngineSession(getApplicationContext())
        val engineView = SystemEngineView(getApplicationContext())

        engineView.render(engineSession)
        assertEquals(engineSession.webView, engineView.getChildAt(0))
    }

    @Test
    fun `WebViewClient notifies observers`() {
        val engineSession = SystemEngineSession(getApplicationContext())
        val engineView = SystemEngineView(getApplicationContext())
        engineView.render(engineSession)

        var observedUrl = ""
        var observedLoadingState = false
        var observedSecurityChange: Triple<Boolean, String?, String?> = Triple(false, null, null)
        engineSession.register(object : EngineSession.Observer {
            override fun onLoadingStateChange(loading: Boolean) { observedLoadingState = loading }
            override fun onLocationChange(url: String) { observedUrl = url }
            override fun onSecurityChange(secure: Boolean, host: String?, issuer: String?) {
                observedSecurityChange = Triple(secure, host, issuer)
            }
        })

        engineSession.webView.webViewClient.onPageStarted(mock(), "https://wiki.mozilla.org/", null)
        assertEquals(true, observedLoadingState)
        assertEquals(observedUrl, "https://wiki.mozilla.org/")

        observedLoadingState = true
        engineSession.webView.webViewClient.onPageFinished(null, "http://mozilla.org")
        assertEquals("http://mozilla.org", observedUrl)
        assertFalse(observedLoadingState)
        assertEquals(Triple(false, null, null), observedSecurityChange)

        val view = mock(WebView::class.java)
        engineSession.webView.webViewClient.onPageFinished(view, "http://mozilla.org")
        assertEquals(Triple(false, null, null), observedSecurityChange)

        val certificate = mock(SslCertificate::class.java)
        val dName = mock(SslCertificate.DName::class.java)
        doReturn("testCA").`when`(dName).oName
        doReturn(certificate).`when`(view).certificate
        engineSession.webView.webViewClient.onPageFinished(view, "http://mozilla.org")

        doReturn("testCA").`when`(dName).oName
        doReturn(dName).`when`(certificate).issuedBy
        doReturn(certificate).`when`(view).certificate
        engineSession.webView.webViewClient.onPageFinished(view, "http://mozilla.org")
        assertEquals(Triple(true, "mozilla.org", "testCA"), observedSecurityChange)
    }

    @Test
    fun `HitResult type handling`() {
        val engineSession = SystemEngineSession(getApplicationContext())
        val engineView = SystemEngineView(getApplicationContext())
        var hitTestResult: HitResult = HitResult.UNKNOWN("")
        engineView.render(engineSession)
        engineSession.register(object : EngineSession.Observer {
            override fun onLongPress(hitResult: HitResult) {
                hitTestResult = hitResult
            }
        })

        engineView.handleLongClick(HitTestResult.EMAIL_TYPE, "mailto:asa@mozilla.com")
        assertTrue(hitTestResult is HitResult.EMAIL)
        assertEquals("mailto:asa@mozilla.com", hitTestResult.src)

        engineView.handleLongClick(HitTestResult.GEO_TYPE, "geo:1,-1")
        assertTrue(hitTestResult is HitResult.GEO)
        assertEquals("geo:1,-1", hitTestResult.src)

        engineView.handleLongClick(HitTestResult.PHONE_TYPE, "tel:+123456789")
        assertTrue(hitTestResult is HitResult.PHONE)
        assertEquals("tel:+123456789", hitTestResult.src)

        engineView.handleLongClick(HitTestResult.IMAGE_TYPE, "image.png")
        assertTrue(hitTestResult is HitResult.IMAGE)
        assertEquals("image.png", hitTestResult.src)

        engineView.handleLongClick(HitTestResult.SRC_ANCHOR_TYPE, "https://mozilla.org")
        assertTrue(hitTestResult is HitResult.UNKNOWN)
        assertEquals("https://mozilla.org", hitTestResult.src)

        var result = engineView.handleLongClick(HitTestResult.SRC_IMAGE_ANCHOR_TYPE, "image.png")
        assertFalse(result) // Intentional for image links; see ImageHandler tests.

        result = engineView.handleLongClick(HitTestResult.EDIT_TEXT_TYPE, "https://mozilla.org")
        assertFalse(result)
    }

    @Test
    fun `ImageHandler notifies observers`() {
        val engineSession = SystemEngineSession(getApplicationContext())
        val engineView = SystemEngineView(getApplicationContext())
        val handler = SystemEngineView.ImageHandler(engineSession)
        val message = mock(Message::class.java)
        val bundle = mock(Bundle::class.java)
        var observerNotified = false

        `when`(message.data).thenReturn(bundle)
        `when`(message.data.getString("url")).thenReturn("https://mozilla.org")
        `when`(message.data.getString("src")).thenReturn("file.png")

        engineView.render(engineSession)
        engineSession.register(object : EngineSession.Observer {
            override fun onLongPress(hitResult: HitResult) {
                observerNotified = true
            }
        })

        handler.handleMessage(message)
        assertTrue(observerNotified)

        observerNotified = false
        val nullHandler = SystemEngineView.ImageHandler(null)
        nullHandler.handleMessage(message)
        assertFalse(observerNotified)
    }

    @Test(expected = IllegalStateException::class)
    fun `null image src`() {
        val engineSession = SystemEngineSession(getApplicationContext())
        val handler = SystemEngineView.ImageHandler(engineSession)
        val message = mock(Message::class.java)
        val bundle = mock(Bundle::class.java)

        `when`(message.data).thenReturn(bundle)
        `when`(message.data.getString("url")).thenReturn("https://mozilla.org")

        handler.handleMessage(message)
    }

    @Test(expected = IllegalStateException::class)
    fun `null image url`() {
        val engineSession = SystemEngineSession(getApplicationContext())
        val handler = SystemEngineView.ImageHandler(engineSession)
        val message = mock(Message::class.java)
        val bundle = mock(Bundle::class.java)

        `when`(message.data).thenReturn(bundle)
        `when`(message.data.getString("src")).thenReturn("file.png")

        handler.handleMessage(message)
    }

    @Test
    fun `WebChromeClient notifies observers`() {
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
    fun `SystemEngineView updates current session url via onPageStart events`() {
        val engineSession = SystemEngineSession(getApplicationContext())
        val engineView = SystemEngineView(getApplicationContext())
        engineView.render(engineSession)

        assertEquals("", engineSession.currentUrl)
        engineSession.webView.webViewClient.onPageStarted(engineSession.webView, "https://www.mozilla.org/", null)
        assertEquals("https://www.mozilla.org/", engineSession.currentUrl)

        engineSession.webView.webViewClient.onPageStarted(engineSession.webView, "https://www.firefox.com/", null)
        assertEquals("https://www.firefox.com/", engineSession.currentUrl)
    }

    @Test
    fun `WebView client notifies navigation state changes`() {
        val engineSession = SystemEngineSession(getApplicationContext())
        val engineView = SystemEngineView(getApplicationContext())

        val observer: EngineSession.Observer = mock()
        val webView: WebView = mock()
        `when`(webView.canGoBack()).thenReturn(true)
        `when`(webView.canGoForward()).thenReturn(true)

        engineSession.register(observer)
        verify(observer, never()).onNavigationStateChange(true, true)

        engineView.render(engineSession)
        engineSession.webView.webViewClient.onPageStarted(webView, "https://www.mozilla.org/", null)
        verify(observer).onNavigationStateChange(true, true)
    }

    @Test
    fun `WebView client notifies configured history delegate of url visits`() = runBlocking {
        val engineSession = SystemEngineSession(getApplicationContext())

        val engineView = SystemEngineView(getApplicationContext())
        val webView: WebView = mock()
        val historyDelegate: HistoryTrackingDelegate = mock()

        engineView.render(engineSession)

        // Nothing breaks if delegate isn't set.
        engineSession.webView.webViewClient.doUpdateVisitedHistory(webView, "https://www.mozilla.com", false)

        engineSession.settings.historyTrackingDelegate = historyDelegate

        engineSession.webView.webViewClient.doUpdateVisitedHistory(webView, "https://www.mozilla.com", false)
        verify(historyDelegate).onVisited(eq("https://www.mozilla.com"), eq(false))

        engineSession.webView.webViewClient.doUpdateVisitedHistory(webView, "https://www.mozilla.com", true)
        verify(historyDelegate).onVisited(eq("https://www.mozilla.com"), eq(true))
    }

    @Test
    fun `WebView client requests history from configured history delegate`() {
        val engineSession = SystemEngineSession(getApplicationContext())

        val engineView = SystemEngineView(getApplicationContext())
        val historyDelegate = object : HistoryTrackingDelegate {
            override suspend fun onVisited(uri: String, isReload: Boolean) {
                fail()
            }

            override suspend fun onTitleChanged(uri: String, title: String) {
                fail()
            }

            override suspend fun getVisited(uris: List<String>): List<Boolean> {
                fail()
                return emptyList()
            }

            override suspend fun getVisited(): List<String> {
                return listOf("https://www.mozilla.com")
            }
        }

        engineView.render(engineSession)

        // Nothing breaks if delegate isn't set.
        engineSession.webView.webChromeClient.getVisitedHistory(mock())

        engineSession.settings.historyTrackingDelegate = historyDelegate

        val historyValueCallback: ValueCallback<Array<String>> = mock()
        runBlocking {
            engineSession.webView.webChromeClient.getVisitedHistory(historyValueCallback)
        }
        verify(historyValueCallback).onReceiveValue(arrayOf("https://www.mozilla.com"))
    }

    @Test
    fun `WebView client notifies configured history delegate of title changes`() = runBlocking {
        val engineSession = SystemEngineSession(getApplicationContext())

        val engineView = SystemEngineView(getApplicationContext())
        val webView: WebView = mock()
        val historyDelegate: HistoryTrackingDelegate = mock()

        engineView.render(engineSession)

        // Nothing breaks if delegate isn't set.
        engineSession.webView.webChromeClient.onReceivedTitle(webView, "New title!")

        // We can now set the delegate. Were it set before the render call,
        // it'll get overwritten during settings initialization.
        engineSession.settings.historyTrackingDelegate = historyDelegate

        // Delegate not notified if, somehow, there's no currentUrl present in the view.
        engineSession.webView.webChromeClient.onReceivedTitle(webView, "New title!")
        verify(historyDelegate, never()).onTitleChanged(eq(""), eq("New title!"))

        // This sets the currentUrl.
        engineSession.webView.webViewClient.onPageStarted(webView, "https://www.mozilla.org/", null)

        engineSession.webView.webChromeClient.onReceivedTitle(webView, "New title!")
        verify(historyDelegate).onTitleChanged(eq("https://www.mozilla.org/"), eq("New title!"))

        reset(historyDelegate)

        // Empty title when none provided
        engineSession.webView.webChromeClient.onReceivedTitle(webView, null)
        verify(historyDelegate).onTitleChanged(eq("https://www.mozilla.org/"), eq(""))
    }

    @Test
    fun `WebView client notifies observers about title changes`() {
        val engineSession = SystemEngineSession(getApplicationContext())

        val engineView = SystemEngineView(getApplicationContext())
        val observer: EngineSession.Observer = mock()
        val webView: WebView = mock()
        `when`(webView.canGoBack()).thenReturn(true)
        `when`(webView.canGoForward()).thenReturn(true)

        engineSession.register(observer)
        engineView.render(engineSession)
        engineSession.webView.webChromeClient.onReceivedTitle(webView, "Hello World!")
        verify(observer).onTitleChange(eq("Hello World!"))
        verify(observer).onNavigationStateChange(true, true)

        reset(observer)

        // Empty title when none provided.
        engineSession.webView.webChromeClient.onReceivedTitle(webView, null)
        verify(observer).onTitleChange(eq(""))
        verify(observer).onNavigationStateChange(true, true)
    }

    @Test
    fun `download listener notifies observers`() {
        val engineSession = SystemEngineSession(getApplicationContext())
        val engineView = SystemEngineView(getApplicationContext())
        engineView.render(engineSession)

        var observerNotified = false

        engineSession.register(object : EngineSession.Observer {
            override fun onExternalResource(
                url: String,
                fileName: String,
                contentLength: Long?,
                contentType: String?,
                cookie: String?,
                userAgent: String?
            ) {
                assertEquals("https://download.mozilla.org", url)
                assertEquals("image.png", fileName)
                assertEquals(1337L, contentLength)
                assertNull(cookie)
                assertEquals("Components/1.0", userAgent)

                observerNotified = true
            }
        })

        val listener = engineView.createDownloadListener()
        listener.onDownloadStart(
            "https://download.mozilla.org",
            "Components/1.0",
            "attachment; filename=\"image.png\"",
            "image/png",
            1337)

        assertTrue(observerNotified)
    }

    @Test
    fun `WebView client tracking protection`() {
        SystemEngineView.URL_MATCHER = UrlMatcher(arrayOf("blocked.random"))

        val engineSession = SystemEngineSession(getApplicationContext())
        val engineView = SystemEngineView(getApplicationContext())
        engineView.render(engineSession)

        val webViewClient = engineSession.webView.webViewClient
        val invalidRequest = mock(WebResourceRequest::class.java)
        `when`(invalidRequest.isForMainFrame).thenReturn(false)
        `when`(invalidRequest.url).thenReturn(Uri.parse("market://foo.bar/"))

        var response = webViewClient.shouldInterceptRequest(engineSession.webView, invalidRequest)
        assertNull(response)

        engineSession.trackingProtectionPolicy = EngineSession.TrackingProtectionPolicy.all()
        response = webViewClient.shouldInterceptRequest(engineSession.webView, invalidRequest)
        assertNotNull(response)
        assertNull(response.data)
        assertNull(response.encoding)
        assertNull(response.mimeType)

        val faviconRequest = mock(WebResourceRequest::class.java)
        `when`(faviconRequest.isForMainFrame).thenReturn(false)
        `when`(faviconRequest.url).thenReturn(Uri.parse("http://foo/favicon.ico"))
        response = webViewClient.shouldInterceptRequest(engineSession.webView, faviconRequest)
        assertNotNull(response)
        assertNull(response.data)
        assertNull(response.encoding)
        assertNull(response.mimeType)

        val blockedRequest = mock(WebResourceRequest::class.java)
        `when`(blockedRequest.isForMainFrame).thenReturn(false)
        `when`(blockedRequest.url).thenReturn(Uri.parse("http://blocked.random"))
        response = webViewClient.shouldInterceptRequest(engineSession.webView, blockedRequest)
        assertNotNull(response)
        assertNull(response.data)
        assertNull(response.encoding)
        assertNull(response.mimeType)
    }

    @Test
    @Suppress("Deprecation")
    fun `WebViewClient calls interceptor from deprecated onReceivedError API`() {
        val engineSession = SystemEngineSession(getApplicationContext())
        val engineView = SystemEngineView(getApplicationContext())
        engineView.render(engineSession)

        val requestInterceptor: RequestInterceptor = mock()
        val webViewClient = engineSession.webView.webViewClient

        // No session or interceptor attached.
        webViewClient.onReceivedError(
            engineSession.webView,
            WebViewClient.ERROR_UNKNOWN,
            null,
            "http://failed.random"
        )
        verifyZeroInteractions(requestInterceptor)

        // Session attached, but not interceptor.
        engineView.render(engineSession)
        webViewClient.onReceivedError(
            engineSession.webView,
            WebViewClient.ERROR_UNKNOWN,
            null,
            "http://failed.random"
        )
        verifyZeroInteractions(requestInterceptor)

        // Session and interceptor.
        engineSession.settings.requestInterceptor = requestInterceptor
        webViewClient.onReceivedError(
            engineSession.webView,
            WebViewClient.ERROR_UNKNOWN,
            null,
            "http://failed.random"
        )
        verify(requestInterceptor).onErrorRequest(engineSession, ErrorType.UNKNOWN, "http://failed.random")

        val webView = mock(WebView::class.java)
        engineSession.webView = webView
        val errorResponse = RequestInterceptor.ErrorResponse("foo", url = "about:fail")
        webViewClient.onReceivedError(
            engineSession.webView,
            WebViewClient.ERROR_UNKNOWN,
            null,
            "http://failed.random"
        )
        verify(webView, never()).loadDataWithBaseURL("about:fail", "foo", "text/html", "UTF-8", null)

        `when`(requestInterceptor.onErrorRequest(engineSession, ErrorType.UNKNOWN, "http://failed.random"))
            .thenReturn(errorResponse)
        webViewClient.onReceivedError(
            engineSession.webView,
            WebViewClient.ERROR_UNKNOWN,
            null,
            "http://failed.random"
        )
        verify(webView).loadDataWithBaseURL("about:fail", "foo", "text/html", "UTF-8", null)

        val errorResponse2 = RequestInterceptor.ErrorResponse("foo")
        webViewClient.onReceivedError(
            engineSession.webView,
            WebViewClient.ERROR_UNKNOWN,
            null,
            "http://failed.random"
        )
        verify(webView, never()).loadDataWithBaseURL("http://failed.random", "foo", "text/html", "UTF-8", null)

        `when`(requestInterceptor.onErrorRequest(engineSession, ErrorType.UNKNOWN, "http://failed.random"))
            .thenReturn(errorResponse2)
        webViewClient.onReceivedError(
            engineSession.webView,
            WebViewClient.ERROR_UNKNOWN,
            null,
            "http://failed.random"
        )
        verify(webView).loadDataWithBaseURL("http://failed.random", "foo", "text/html", "UTF-8", null)
    }

    @Test
    fun `WebViewClient calls interceptor from new onReceivedError API`() {
        val engineSession = SystemEngineSession(getApplicationContext())
        val engineView = SystemEngineView(getApplicationContext())
        engineView.render(engineSession)

        val requestInterceptor: RequestInterceptor = mock()
        val webViewClient = engineSession.webView.webViewClient
        val webRequest: WebResourceRequest = mock()
        val webError: WebResourceError = mock()
        val url: Uri = mock()

        webViewClient.onReceivedError(engineSession.webView, webRequest, webError)
        verifyZeroInteractions(requestInterceptor)

        engineView.render(engineSession)
        webViewClient.onReceivedError(engineSession.webView, webRequest, webError)
        verifyZeroInteractions(requestInterceptor)

        `when`(webError.errorCode).thenReturn(WebViewClient.ERROR_UNKNOWN)
        `when`(webRequest.url).thenReturn(url)
        `when`(url.toString()).thenReturn("http://failed.random")
        engineSession.settings.requestInterceptor = requestInterceptor
        webViewClient.onReceivedError(engineSession.webView, webRequest, webError)
        verify(requestInterceptor, never()).onErrorRequest(engineSession, ErrorType.UNKNOWN, "http://failed.random")

        `when`(webRequest.isForMainFrame).thenReturn(true)
        webViewClient.onReceivedError(engineSession.webView, webRequest, webError)
        verify(requestInterceptor).onErrorRequest(engineSession, ErrorType.UNKNOWN, "http://failed.random")

        val webView = mock(WebView::class.java)
        engineSession.webView = webView
        val errorResponse = RequestInterceptor.ErrorResponse("foo", url = "about:fail")
        webViewClient.onReceivedError(engineSession.webView, webRequest, webError)
        verify(webView, never()).loadDataWithBaseURL("about:fail", "foo", "text/html", "UTF-8", null)

        `when`(requestInterceptor.onErrorRequest(engineSession, ErrorType.UNKNOWN, "http://failed.random"))
            .thenReturn(errorResponse)
        webViewClient.onReceivedError(engineSession.webView, webRequest, webError)
        verify(webView).loadDataWithBaseURL("about:fail", "foo", "text/html", "UTF-8", null)

        val errorResponse2 = RequestInterceptor.ErrorResponse("foo")
        webViewClient.onReceivedError(engineSession.webView, webRequest, webError)
        verify(webView, never()).loadDataWithBaseURL("http://failed.random", "foo", "text/html", "UTF-8", null)

        `when`(requestInterceptor.onErrorRequest(engineSession, ErrorType.UNKNOWN, "http://failed.random"))
            .thenReturn(errorResponse2)
        webViewClient.onReceivedError(engineSession.webView, webRequest, webError)
        verify(webView).loadDataWithBaseURL("http://failed.random", "foo", "text/html", "UTF-8", null)
    }

    @Test
    fun `WebViewClient calls interceptor when onReceivedSslError`() {
        val engineSession = SystemEngineSession(getApplicationContext())
        val engineView = SystemEngineView(getApplicationContext())
        engineView.render(engineSession)

        val requestInterceptor: RequestInterceptor = mock()
        val webViewClient = engineSession.webView.webViewClient
        val handler: SslErrorHandler = mock()
        val error: SslError = mock()

        webViewClient.onReceivedSslError(engineSession.webView, handler, error)
        verifyZeroInteractions(requestInterceptor)

        engineView.render(engineSession)
        webViewClient.onReceivedSslError(engineSession.webView, handler, error)
        verifyZeroInteractions(requestInterceptor)

        `when`(error.primaryError).thenReturn(SslError.SSL_EXPIRED)
        `when`(error.url).thenReturn("http://failed.random")
        engineSession.settings.requestInterceptor = requestInterceptor
        webViewClient.onReceivedSslError(engineSession.webView, handler, error)
        verify(requestInterceptor).onErrorRequest(engineSession, ErrorType.ERROR_SECURITY_SSL, "http://failed.random")
        verify(handler, times(3)).cancel()

        val webView = mock(WebView::class.java)
        engineSession.webView = webView
        val errorResponse = RequestInterceptor.ErrorResponse("foo", url = "about:fail")
        webViewClient.onReceivedSslError(engineSession.webView, handler, error)
        verify(webView, never()).loadDataWithBaseURL("about:fail", "foo", "text/html", "UTF-8", null)

        `when`(
            requestInterceptor.onErrorRequest(
                engineSession,
                ErrorType.ERROR_SECURITY_SSL,
                "http://failed.random"
            )
        ).thenReturn(errorResponse)
        webViewClient.onReceivedSslError(engineSession.webView, handler, error)
        verify(webView).loadDataWithBaseURL("about:fail", "foo", "text/html", "UTF-8", null)

        val errorResponse2 = RequestInterceptor.ErrorResponse("foo")
        webViewClient.onReceivedSslError(engineSession.webView, handler, error)
        verify(webView, never()).loadDataWithBaseURL("http://failed.random", "foo", "text/html", "UTF-8", null)

        `when`(requestInterceptor.onErrorRequest(engineSession, ErrorType.ERROR_SECURITY_SSL, "http://failed.random"))
            .thenReturn(errorResponse2)
        webViewClient.onReceivedSslError(engineSession.webView, handler, error)
        verify(webView).loadDataWithBaseURL(null, "foo", "text/html", "UTF-8", null)

        `when`(requestInterceptor.onErrorRequest(engineSession, ErrorType.ERROR_SECURITY_SSL, "http://failed.random"))
            .thenReturn(RequestInterceptor.ErrorResponse("foo", "http://failed.random"))
        webViewClient.onReceivedSslError(engineSession.webView, handler, error)
        verify(webView).loadDataWithBaseURL("http://failed.random", "foo", "text/html", "UTF-8", null)
    }

    @Test
    fun `WebViewClient blocks WebFonts`() {
        val engineSession = SystemEngineSession(getApplicationContext())
        val engineView = SystemEngineView(getApplicationContext())
        engineView.render(engineSession)

        val webViewClient = engineSession.webView.webViewClient
        val webFontRequest = mock(WebResourceRequest::class.java)
        `when`(webFontRequest.url).thenReturn(Uri.parse("/fonts/test.woff"))
        assertNull(webViewClient.shouldInterceptRequest(engineSession.webView, webFontRequest))

        engineView.render(engineSession)
        assertNull(webViewClient.shouldInterceptRequest(engineSession.webView, webFontRequest))

        engineSession.settings.webFontsEnabled = false

        val request = mock(WebResourceRequest::class.java)
        `when`(request.url).thenReturn(Uri.parse("http://mozilla.org"))
        assertNull(webViewClient.shouldInterceptRequest(engineSession.webView, request))

        val response = webViewClient.shouldInterceptRequest(engineSession.webView, webFontRequest)
        assertNotNull(response)
        assertNull(response.data)
        assertNull(response.encoding)
        assertNull(response.mimeType)
    }

    @Test
    fun `FindListener notifies observers`() {
        val engineSession = SystemEngineSession(getApplicationContext())
        val engineView = SystemEngineView(getApplicationContext())
        engineView.render(engineSession)

        var observerNotified = false

        engineSession.register(object : EngineSession.Observer {
            override fun onFindResult(activeMatchOrdinal: Int, numberOfMatches: Int, isDoneCounting: Boolean) {
                assertEquals(0, activeMatchOrdinal)
                assertEquals(1, numberOfMatches)
                assertTrue(isDoneCounting)
                observerNotified = true
            }
        })

        val listener = engineView.createFindListener()
        listener.onFindResultReceived(0, 1, true)
        assertTrue(observerNotified)
    }

    @Test
    fun `lifecycle methods are invoked`() {
        val mockWebView = mock(WebView::class.java)
        val engineSession1 = SystemEngineSession(getApplicationContext())
        val engineSession2 = SystemEngineSession(getApplicationContext())

        val engineView = SystemEngineView(getApplicationContext())
        engineView.onPause()
        engineView.onResume()
        engineView.onDestroy()

        engineSession1.webView = mockWebView
        engineView.render(engineSession1)
        engineView.onDestroy()

        engineView.render(engineSession2)
        assertNotNull(engineSession2.webView.parent)

        engineView.onDestroy()
        assertNull(engineSession2.webView.parent)

        engineView.render(engineSession1)
        engineView.onPause()
        verify(mockWebView, times(1)).onPause()
        verify(mockWebView, times(1)).pauseTimers()

        engineView.onResume()
        verify(mockWebView, times(1)).onResume()
        verify(mockWebView, times(1)).resumeTimers()

        engineView.onDestroy()
    }

    @Test
    fun `showCustomView notifies fullscreen mode observers and execs callback`() {
        val engineSession = SystemEngineSession(getApplicationContext())
        val engineView = SystemEngineView(getApplicationContext())
        engineView.render(engineSession)

        val observer: EngineSession.Observer = mock()
        engineSession.register(observer)

        val view = mock(View::class.java)
        val customViewCallback = mock(WebChromeClient.CustomViewCallback::class.java)
        engineSession.webView.webChromeClient.onShowCustomView(view, customViewCallback)

        verify(observer).onFullScreenChange(true)
    }

    @Test
    fun `addFullScreenView execs callback and removeView`() {
        val engineSession = SystemEngineSession(getApplicationContext())
        val engineView = SystemEngineView(getApplicationContext())
        engineView.render(engineSession)

        val view = View(RuntimeEnvironment.systemContext)
        val customViewCallback = mock(WebChromeClient.CustomViewCallback::class.java)

        assertNull(engineSession.fullScreenCallback)

        engineSession.webView.webChromeClient.onShowCustomView(view, customViewCallback)

        assertNotNull(engineSession.fullScreenCallback)
        assertEquals(customViewCallback, engineSession.fullScreenCallback)
        assertEquals("mozac_system_engine_fullscreen", view.tag)

        engineSession.webView.webChromeClient.onHideCustomView()
        assertEquals(View.VISIBLE, engineSession.webView.visibility)
    }

    @Test
    fun `addFullScreenView with no matching webView`() {
        val engineSession = SystemEngineSession(getApplicationContext())
        val engineView = SystemEngineView(getApplicationContext())
        engineView.render(engineSession)

        val view = View(RuntimeEnvironment.systemContext)
        val customViewCallback = mock(WebChromeClient.CustomViewCallback::class.java)

        engineSession.webView.tag = "not_webview"
        engineSession.webView.webChromeClient.onShowCustomView(view, customViewCallback)

        assertNotEquals(View.INVISIBLE, engineSession.webView.visibility)
    }

    @Test
    fun `removeFullScreenView with no matching views`() {
        val engineSession = SystemEngineSession(getApplicationContext())
        val engineView = SystemEngineView(getApplicationContext())
        engineView.render(engineSession)

        val view = View(RuntimeEnvironment.systemContext)
        val customViewCallback = mock(WebChromeClient.CustomViewCallback::class.java)

        // When the fullscreen view isn't available
        engineSession.webView.webChromeClient.onShowCustomView(view, customViewCallback)
        engineView.findViewWithTag<View>("mozac_system_engine_fullscreen").tag = "not_fullscreen"

        engineSession.webView.webChromeClient.onHideCustomView()

        assertNotNull(engineSession.fullScreenCallback)
        verify(engineSession.fullScreenCallback, never())?.onCustomViewHidden()
        assertEquals(View.INVISIBLE, engineSession.webView.visibility)

        // When fullscreen view is available, but WebView isn't.
        engineView.findViewWithTag<View>("not_fullscreen").tag = "mozac_system_engine_fullscreen"
        engineSession.webView.tag = "not_webView"

        engineSession.webView.webChromeClient.onHideCustomView()

        assertEquals(View.INVISIBLE, engineSession.webView.visibility)
    }

    @Test
    fun `fullscreenCallback is null`() {
        val engineSession = SystemEngineSession(getApplicationContext())
        val engineView = SystemEngineView(getApplicationContext())
        engineView.render(engineSession)

        engineSession.webView.webChromeClient.onHideCustomView()
        assertNull(engineSession.fullScreenCallback)
    }

    @Test
    fun `onPageFinished handles invalid URL`() {
        val engineSession = SystemEngineSession(getApplicationContext())
        val engineView = SystemEngineView(getApplicationContext())
        engineView.render(engineSession)

        var observedUrl = ""
        var observedLoadingState = true
        var observedSecurityChange: Triple<Boolean, String?, String?> = Triple(false, null, null)
        engineSession.register(object : EngineSession.Observer {
            override fun onLoadingStateChange(loading: Boolean) { observedLoadingState = loading }
            override fun onLocationChange(url: String) { observedUrl = url }
            override fun onSecurityChange(secure: Boolean, host: String?, issuer: String?) {
                observedSecurityChange = Triple(secure, host, issuer)
            }
        })

        // We need a certificate to trigger parsing the potentially invalid URL for
        // the host parameter in onSecurityChange
        val view = mock(WebView::class.java)
        val certificate = mock(SslCertificate::class.java)
        val dName = mock(SslCertificate.DName::class.java)
        doReturn("testCA").`when`(dName).oName
        doReturn(dName).`when`(certificate).issuedBy
        doReturn(certificate).`when`(view).certificate

        engineSession.webView.webViewClient.onPageFinished(view, "invalid:")
        assertEquals("invalid:", observedUrl)
        assertFalse(observedLoadingState)
        assertEquals(Triple(true, null, "testCA"), observedSecurityChange)
    }

    @Test
    fun `URL matcher categories can be changed`() {
        SystemEngineView.URL_MATCHER = null

        var urlMatcher = SystemEngineView.getOrCreateUrlMatcher(getApplicationContext(),
                TrackingProtectionPolicy.select(TrackingProtectionPolicy.AD, TrackingProtectionPolicy.ANALYTICS)
        )
        assertEquals(setOf(UrlMatcher.ADVERTISING, UrlMatcher.ANALYTICS), urlMatcher.enabledCategories)

        urlMatcher = SystemEngineView.getOrCreateUrlMatcher(getApplicationContext(),
                TrackingProtectionPolicy.select(TrackingProtectionPolicy.AD, TrackingProtectionPolicy.SOCIAL)
        )
        assertEquals(setOf(UrlMatcher.ADVERTISING, UrlMatcher.SOCIAL), urlMatcher.enabledCategories)
    }

    @Test
    fun `permission requests are forwarded to observers`() {
        val permissionRequest: android.webkit.PermissionRequest = mock()
        `when`(permissionRequest.resources).thenReturn(emptyArray())
        `when`(permissionRequest.origin).thenReturn(Uri.parse("https://mozilla.org"))

        val engineSession = SystemEngineSession(getApplicationContext())
        val engineView = SystemEngineView(getApplicationContext())
        engineView.render(engineSession)

        var observedPermissionRequest: PermissionRequest? = null
        var cancelledPermissionRequest: PermissionRequest? = null
        engineSession.register(object : EngineSession.Observer {
            override fun onContentPermissionRequest(permissionRequest: PermissionRequest) {
                observedPermissionRequest = permissionRequest
            }

            override fun onCancelContentPermissionRequest(permissionRequest: PermissionRequest) {
                cancelledPermissionRequest = permissionRequest
            }
        })

        engineSession.webView.webChromeClient.onPermissionRequest(permissionRequest)
        assertNotNull(observedPermissionRequest)

        engineSession.webView.webChromeClient.onPermissionRequestCanceled(permissionRequest)
        assertNotNull(cancelledPermissionRequest)
    }

    @Test
    fun `window requests are forwarded to observers`() {
        val permissionRequest: android.webkit.PermissionRequest = mock()
        `when`(permissionRequest.resources).thenReturn(emptyArray())
        `when`(permissionRequest.origin).thenReturn(Uri.parse("https://mozilla.org"))

        val engineSession = SystemEngineSession(getApplicationContext())
        val engineView = SystemEngineView(getApplicationContext())
        engineView.render(engineSession)

        var createWindowRequest: WindowRequest? = null
        var closeWindowRequest: WindowRequest? = null
        engineSession.register(object : EngineSession.Observer {
            override fun onOpenWindowRequest(windowRequest: WindowRequest) {
                createWindowRequest = windowRequest
            }

            override fun onCloseWindowRequest(windowRequest: WindowRequest) {
                closeWindowRequest = windowRequest
            }
        })

        engineSession.webView.webChromeClient.onCreateWindow(mock(WebView::class.java), false, false, null)
        assertNotNull(createWindowRequest)

        engineSession.webView.webChromeClient.onCloseWindow(mock(WebView::class.java))
        assertNotNull(closeWindowRequest)
    }

    @Test
    fun `Calling onShowFileChooser must provide a FilePicker PromptRequest`() {
        val engineSession = SystemEngineSession(getApplicationContext())
        val context = ApplicationProvider.getApplicationContext<Context>()
        val engineView = SystemEngineView(context)
        var onSingleFileSelectedWasCalled = false
        var onMultipleFilesSelectedWasCalled = false
        var onDismissWasCalled = false
        var request: PromptRequest? = null

        val callback = ValueCallback<Array<Uri>> {

            if (it == null) {
                onDismissWasCalled = true
            } else {
                if (it.size == 1) {
                    onSingleFileSelectedWasCalled = true
                } else {
                    onMultipleFilesSelectedWasCalled = true
                }
            }
        }

        engineSession.register(object : EngineSession.Observer {
            override fun onPromptRequest(promptRequest: PromptRequest) {
                request = promptRequest
            }
        })

        engineView.render(engineSession)

        val mockFileChooserParams = mock<WebChromeClient.FileChooserParams>()

        doReturn(MODE_OPEN_MULTIPLE).`when`(mockFileChooserParams).mode

        engineSession.webView.webChromeClient!!.onShowFileChooser(null, callback, mockFileChooserParams)

        val filePickerRequest = request as PromptRequest.File
        assertTrue(request is PromptRequest.File)

        filePickerRequest.onSingleFileSelected(mock(), mock())
        assertTrue(onSingleFileSelectedWasCalled)

        filePickerRequest.onMultipleFilesSelected(mock(), arrayOf(mock(), mock()))
        assertTrue(onMultipleFilesSelectedWasCalled)

        filePickerRequest.onDismiss()
        assertTrue(onDismissWasCalled)

        assertTrue(filePickerRequest.mimeTypes.isEmpty())
        assertTrue(filePickerRequest.isMultipleFilesSelection)

        doReturn(arrayOf("")).`when`(mockFileChooserParams).acceptTypes
        engineSession.webView.webChromeClient!!.onShowFileChooser(null, callback, mockFileChooserParams)
        assertTrue(filePickerRequest.mimeTypes.isEmpty())
    }

    @Test
    fun `canScrollVerticallyDown can be called without session`() {
        val engineView = SystemEngineView(getApplicationContext())
        assertFalse(engineView.canScrollVerticallyDown())

        engineView.render(SystemEngineSession(getApplicationContext()))
        assertFalse(engineView.canScrollVerticallyDown())
    }

    @Test
    fun `onLongClick can be called without session`() {
        val engineView = SystemEngineView(getApplicationContext())
        assertFalse(engineView.onLongClick(null))

        engineView.render(SystemEngineSession(getApplicationContext()))
        assertFalse(engineView.onLongClick(null))
    }

    @Test
    fun `Calling onJsAlert must provide an Alert PromptRequest`() {
        val context = getApplicationContext<Context>()
        val engineSession = SystemEngineSession(context)
        val engineView = SystemEngineView(context)
        var request: PromptRequest? = null

        engineSession.register(object : EngineSession.Observer {
            override fun onPromptRequest(promptRequest: PromptRequest) {
                request = promptRequest
            }
        })

        engineView.render(engineSession)

        val mockJSResult = mock<JsResult>()

        engineSession.webView.webChromeClient!!.onJsAlert(mock(), "http://www.mozilla.org", "message", mockJSResult)

        val alertRequest = request as PromptRequest.Alert
        assertTrue(request is PromptRequest.Alert)

        assertTrue(alertRequest.title.contains("mozilla.org"))
        assertEquals(alertRequest.hasShownManyDialogs, false)
        assertEquals(alertRequest.message, "message")

        alertRequest.onConfirm(true)
        verify(mockJSResult).confirm()
        assertEquals(engineView.jsAlertCount, 1)

        alertRequest.onDismiss()
        verify(mockJSResult).cancel()

        alertRequest.onConfirm(true)
        assertEquals(engineView.shouldShowMoreDialogs, false)

        engineView.lastDialogShownAt = engineView.lastDialogShownAt.add(YEAR, -1)
        engineSession.webView.webChromeClient!!.onJsAlert(mock(), "http://www.mozilla.org", "message", mockJSResult)

        assertEquals(engineView.jsAlertCount, 1)
        verify(mockJSResult, times(2)).cancel()

        engineView.lastDialogShownAt = engineView.lastDialogShownAt.add(YEAR, -1)
        engineView.jsAlertCount = 100
        engineView.shouldShowMoreDialogs = true

        engineSession.webView.webChromeClient!!.onJsAlert(mock(), "http://www.mozilla.org", null, mockJSResult)

        assertTrue((request as PromptRequest.Alert).hasShownManyDialogs)

        engineSession.currentUrl = "http://www.mozilla.org"
        engineSession.webView.webChromeClient!!.onJsAlert(mock(), null, "message", mockJSResult)
        assertTrue((request as PromptRequest.Alert).title.contains("mozilla.org"))
    }

    @Test
    fun `are dialogs by count`() {
        val context = ApplicationProvider.getApplicationContext<Context>()
        val engineView = SystemEngineView(context)

        with(engineView) {

            assertFalse(areDialogsAbusedByCount())

            jsAlertCount = MAX_SUCCESSIVE_DIALOG_COUNT + 1

            assertTrue(areDialogsAbusedByCount())

            jsAlertCount = MAX_SUCCESSIVE_DIALOG_COUNT - 1

            assertFalse(areDialogsAbusedByCount())
        }
    }

    @Test
    fun `are dialogs by time`() {
        val context = ApplicationProvider.getApplicationContext<Context>()
        val engineView = SystemEngineView(context)

        with(engineView) {

            assertFalse(areDialogsAbusedByTime())

            lastDialogShownAt = Date()

            jsAlertCount = 1

            assertTrue(areDialogsAbusedByTime())
        }
    }

    @Test
    fun `are dialogs being abused`() {
        val context = ApplicationProvider.getApplicationContext<Context>()
        val engineView = SystemEngineView(context)

        with(engineView) {

            assertFalse(areDialogsBeingAbused())

            jsAlertCount = MAX_SUCCESSIVE_DIALOG_COUNT + 1

            assertTrue(areDialogsBeingAbused())

            jsAlertCount = 0
            lastDialogShownAt = Date()

            assertFalse(areDialogsBeingAbused())

            jsAlertCount = 1
            lastDialogShownAt = Date()

            assertTrue(areDialogsBeingAbused())
        }
    }

    @Test
    fun `update JSDialog abused state`() {

        val context = ApplicationProvider.getApplicationContext<Context>()
        val engineView = SystemEngineView(context)

        with(engineView) {
            val thresholdInSeconds = MAX_SUCCESSIVE_DIALOG_SECONDS_LIMIT + 1
            lastDialogShownAt = lastDialogShownAt.add(SECOND, -thresholdInSeconds)

            val initialDate = lastDialogShownAt
            updateJSDialogAbusedState()

            assertEquals(jsAlertCount, 1)
            assertTrue(lastDialogShownAt.after(initialDate))

            lastDialogShownAt = lastDialogShownAt.add(SECOND, -thresholdInSeconds)
            updateJSDialogAbusedState()
            assertEquals(jsAlertCount, 1)
        }
    }

    @Test
    fun `js alert abuse state must be reset every time a page is started`() {

        val context = ApplicationProvider.getApplicationContext<Context>()
        val engineSession = SystemEngineSession(context)
        val engineView = SystemEngineView(context)

        with(engineView) {
            jsAlertCount = 20
            shouldShowMoreDialogs = false

            render(engineSession)
            engineSession.webView.webViewClient!!.onPageStarted(mock(), "www.mozilla.org", null)

            assertEquals(jsAlertCount, 0)
            assertTrue(shouldShowMoreDialogs)
        }
    }

    @Test
    fun `calling onJsPrompt must provide a TextPrompt PromptRequest`() {
        val context = getApplicationContext<Context>()
        val engineSession = SystemEngineSession(context)
        val engineView = SystemEngineView(context)
        var request: PromptRequest? = null

        engineSession.register(object : EngineSession.Observer {
            override fun onPromptRequest(promptRequest: PromptRequest) {
                request = promptRequest
            }
        })

        engineView.render(engineSession)

        val mockJSPromptResult = mock<JsPromptResult>()

        engineSession.webView.webChromeClient!!.onJsPrompt(
            mock(),
            "http://www.mozilla.org",
            "message",
            "defaultValue",
            mockJSPromptResult
        )

        val textPromptRequest = request as PromptRequest.TextPrompt
        assertTrue(request is PromptRequest.TextPrompt)

        assertTrue(textPromptRequest.title.contains("mozilla.org"))
        assertEquals(textPromptRequest.hasShownManyDialogs, false)
        assertEquals(textPromptRequest.inputLabel, "message")
        assertEquals(textPromptRequest.inputValue, "defaultValue")

        textPromptRequest.onConfirm(true, "value")
        verify(mockJSPromptResult).confirm("value")
        assertEquals(engineView.jsAlertCount, 1)

        textPromptRequest.onDismiss()
        verify(mockJSPromptResult).cancel()

        textPromptRequest.onConfirm(true, "value")
        assertEquals(engineView.shouldShowMoreDialogs, false)

        engineView.lastDialogShownAt = engineView.lastDialogShownAt.add(YEAR, -1)
        engineSession.webView.webChromeClient!!.onJsPrompt(
            mock(),
            "http://www.mozilla.org",
            "message", "defaultValue",
            mockJSPromptResult
        )

        assertEquals(engineView.jsAlertCount, 1)
        verify(mockJSPromptResult, times(2)).cancel()

        engineView.lastDialogShownAt = engineView.lastDialogShownAt.add(YEAR, -1)
        engineView.jsAlertCount = 100
        engineView.shouldShowMoreDialogs = true

        engineSession.webView.webChromeClient!!.onJsPrompt(
            mock(),
            "http://www.mozilla.org",
            null,
            null,
            mockJSPromptResult
        )

        assertTrue((request as PromptRequest.TextPrompt).hasShownManyDialogs)

        engineSession.currentUrl = "http://www.mozilla.org"
        engineSession.webView.webChromeClient!!.onJsPrompt(
            mock(),
            null,
            "message",
            "defaultValue",
            mockJSPromptResult
        )
        assertTrue((request as PromptRequest.TextPrompt).title.contains("mozilla.org"))
    }

    @Test
    fun `calling onJsPrompt with a null session must not provide a TextPrompt PromptRequest`() {
        val context = getApplicationContext<Context>()
        val engineSession = SystemEngineSession(context)
        val engineView = SystemEngineView(context)

        var request: PromptRequest? = null

        engineSession.register(object : EngineSession.Observer {
            override fun onPromptRequest(promptRequest: PromptRequest) {
                request = promptRequest
            }
        })

        engineView.render(engineSession)

        val mockJSPromptResult = mock<JsPromptResult>()
        engineView.session = null

        val wasTheDialogHandled = engineSession.webView.webChromeClient!!.onJsPrompt(
            mock(),
            "http://www.mozilla.org",
            "message", "defaultValue",
            mockJSPromptResult
        )

        assertTrue(wasTheDialogHandled)
        assertNull(request)
        verify(mockJSPromptResult).cancel()
    }

    @Test
    fun `calling onJsConfirm must provide a Confirm PromptRequest`() {
        val context = getApplicationContext<Context>()
        val engineSession = SystemEngineSession(context)
        val engineView = SystemEngineView(context)
        var request: PromptRequest? = null

        engineSession.register(object : EngineSession.Observer {
            override fun onPromptRequest(promptRequest: PromptRequest) {
                request = promptRequest
            }
        })

        engineView.render(engineSession)

        val mockJSPromptResult = mock<JsResult>()

        engineSession.webView.webChromeClient!!.onJsConfirm(
            mock(),
            "http://www.mozilla.org",
            "message",
            mockJSPromptResult
        )

        val confirmPromptRequest = request as PromptRequest.Confirm
        assertTrue(request is PromptRequest.Confirm)

        assertTrue(confirmPromptRequest.title.contains("mozilla.org"))
        assertEquals(confirmPromptRequest.hasShownManyDialogs, false)
        assertEquals(confirmPromptRequest.message, "message")
        assertEquals(confirmPromptRequest.positiveButtonTitle.toLowerCase(), "OK".toLowerCase())
        assertEquals(confirmPromptRequest.negativeButtonTitle.toLowerCase(), "Cancel".toLowerCase())

        confirmPromptRequest.onConfirmPositiveButton(true)
        verify(mockJSPromptResult).confirm()
        assertEquals(engineView.jsAlertCount, 1)

        confirmPromptRequest.onDismiss()
        verify(mockJSPromptResult).cancel()

        confirmPromptRequest.onConfirmNegativeButton(true)
        verify(mockJSPromptResult, times(2)).cancel()

        confirmPromptRequest.onConfirmPositiveButton(true)
        assertEquals(engineView.shouldShowMoreDialogs, false)

        engineView.lastDialogShownAt = engineView.lastDialogShownAt.add(YEAR, -1)
        engineSession.webView.webChromeClient!!.onJsConfirm(
            mock(),
            "http://www.mozilla.org",
            "message",
            mockJSPromptResult
        )

        assertEquals(engineView.jsAlertCount, 1)
        verify(mockJSPromptResult, times(3)).cancel()

        engineView.lastDialogShownAt = engineView.lastDialogShownAt.add(YEAR, -1)
        engineView.jsAlertCount = 100
        engineView.shouldShowMoreDialogs = true

        engineSession.webView.webChromeClient!!.onJsConfirm(
            mock(),
            "http://www.mozilla.org",
            null,
            mockJSPromptResult
        )

        assertTrue((request as PromptRequest.Confirm).hasShownManyDialogs)

        engineSession.currentUrl = "http://www.mozilla.org"
        engineSession.webView.webChromeClient!!.onJsConfirm(
            mock(),
            null,
            "message",
            mockJSPromptResult
        )
        assertTrue((request as PromptRequest.Confirm).title.contains("mozilla.org"))
    }

    @Test
    fun captureThumbnail() {
        val engineView = SystemEngineView(getApplicationContext())

        engineView.session = mock()

        `when`(engineView.session!!.webView).thenReturn(mock())

        `when`(engineView.session!!.webView.drawingCache)
            .thenReturn(Bitmap.createBitmap(10, 10, Bitmap.Config.RGB_565))

        var thumbnail: Bitmap? = null

        engineView.captureThumbnail {
            thumbnail = it
        }
        assertNotNull(thumbnail)

        engineView.session = null
        engineView.captureThumbnail {
            thumbnail = it
        }

        assertNull(thumbnail)
    }

    private fun Date.add(timeUnit: Int, amountOfTime: Int): Date {
        val calendar = Calendar.getInstance()
        calendar.time = this
        calendar.add(timeUnit, amountOfTime)
        return calendar.time
    }
}