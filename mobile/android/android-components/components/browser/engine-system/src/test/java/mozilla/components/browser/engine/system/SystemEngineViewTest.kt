/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

package mozilla.components.browser.engine.system

import android.app.Activity
import android.graphics.Bitmap
import android.net.Uri
import android.net.http.SslCertificate
import android.net.http.SslError
import android.os.Build
import android.os.Bundle
import android.os.Message
import android.view.PixelCopy
import android.view.View
import android.webkit.HttpAuthHandler
import android.webkit.JsPromptResult
import android.webkit.JsResult
import android.webkit.SslErrorHandler
import android.webkit.ValueCallback
import android.webkit.WebChromeClient
import android.webkit.WebChromeClient.FileChooserParams.MODE_OPEN_MULTIPLE
import android.webkit.WebResourceError
import android.webkit.WebResourceRequest
import android.webkit.WebSettings
import android.webkit.WebView
import android.webkit.WebView.HitTestResult
import android.webkit.WebViewClient
import android.webkit.WebViewDatabase
import androidx.test.ext.junit.runners.AndroidJUnit4
import kotlinx.coroutines.runBlocking
import mozilla.components.browser.engine.system.matcher.UrlMatcher
import mozilla.components.browser.errorpages.ErrorType
import mozilla.components.concept.engine.EngineSession
import mozilla.components.concept.engine.EngineSession.TrackingProtectionPolicy
import mozilla.components.concept.engine.EngineSession.TrackingProtectionPolicy.TrackingCategory
import mozilla.components.concept.engine.HitResult
import mozilla.components.concept.engine.InputResultDetail
import mozilla.components.concept.engine.content.blocking.Tracker
import mozilla.components.concept.engine.history.HistoryTrackingDelegate
import mozilla.components.concept.engine.permission.PermissionRequest
import mozilla.components.concept.engine.prompt.PromptRequest
import mozilla.components.concept.engine.request.RequestInterceptor
import mozilla.components.concept.engine.window.WindowRequest
import mozilla.components.concept.fetch.Response
import mozilla.components.concept.storage.PageVisit
import mozilla.components.concept.storage.RedirectSource
import mozilla.components.concept.storage.VisitType
import mozilla.components.support.test.any
import mozilla.components.support.test.argumentCaptor
import mozilla.components.support.test.eq
import mozilla.components.support.test.mock
import mozilla.components.support.test.robolectric.shadow.PixelCopyShadow
import mozilla.components.support.test.robolectric.testContext
import mozilla.components.support.test.whenever
import org.junit.Assert.assertEquals
import org.junit.Assert.assertFalse
import org.junit.Assert.assertNotEquals
import org.junit.Assert.assertNotNull
import org.junit.Assert.assertNull
import org.junit.Assert.assertSame
import org.junit.Assert.assertTrue
import org.junit.Assert.fail
import org.junit.Test
import org.junit.runner.RunWith
import org.mockito.ArgumentMatchers
import org.mockito.Mockito.doNothing
import org.mockito.Mockito.doReturn
import org.mockito.Mockito.never
import org.mockito.Mockito.reset
import org.mockito.Mockito.spy
import org.mockito.Mockito.times
import org.mockito.Mockito.verify
import org.mockito.Mockito.verifyNoInteractions
import org.robolectric.Robolectric
import org.robolectric.RuntimeEnvironment
import org.robolectric.annotation.Config
import java.io.StringReader

@RunWith(AndroidJUnit4::class)
class SystemEngineViewTest {

    @Test
    fun `EngineView initialization`() {
        val engineView = SystemEngineView(testContext)
        val webView = WebView(testContext)

        engineView.initWebView(webView)
        assertNotNull(webView.webChromeClient)
        assertNotNull(webView.webViewClient)
    }

    @Test
    fun `EngineView renders WebView`() {
        val engineSession = SystemEngineSession(testContext)
        val engineView = SystemEngineView(testContext)

        engineView.render(engineSession)
        assertEquals(engineSession.webView, engineView.getChildAt(0))
    }

    @Test
    fun `WebViewClient notifies observers`() {
        val engineSession = SystemEngineSession(testContext)
        val engineView = SystemEngineView(testContext)
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

        val view = mock<WebView>()
        engineSession.webView.webViewClient.onPageFinished(view, "http://mozilla.org")
        assertEquals(Triple(false, null, null), observedSecurityChange)

        val certificate = mock<SslCertificate>()
        val dName = mock<SslCertificate.DName>()
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
        val engineSession = SystemEngineSession(testContext)
        val engineView = SystemEngineView(testContext)
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
        val engineSession = SystemEngineSession(testContext)
        val engineView = SystemEngineView(testContext)
        val handler = SystemEngineView.ImageHandler(engineSession)
        val message = mock<Message>()
        val bundle = mock<Bundle>()
        var observerNotified = false

        whenever(message.data).thenReturn(bundle)
        whenever(message.data.getString("url")).thenReturn("https://mozilla.org")
        whenever(message.data.getString("src")).thenReturn("file.png")

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
        val engineSession = SystemEngineSession(testContext)
        val handler = SystemEngineView.ImageHandler(engineSession)
        val message = mock<Message>()
        val bundle = mock<Bundle>()

        whenever(message.data).thenReturn(bundle)
        whenever(message.data.getString("url")).thenReturn("https://mozilla.org")

        handler.handleMessage(message)
    }

    @Test(expected = IllegalStateException::class)
    fun `null image url`() {
        val engineSession = SystemEngineSession(testContext)
        val handler = SystemEngineView.ImageHandler(engineSession)
        val message = mock<Message>()
        val bundle = mock<Bundle>()

        whenever(message.data).thenReturn(bundle)
        whenever(message.data.getString("src")).thenReturn("file.png")

        handler.handleMessage(message)
    }

    @Test
    fun `WebChromeClient notifies observers`() {
        val engineSession = SystemEngineSession(testContext)
        val engineView = SystemEngineView(testContext)
        engineView.render(engineSession)

        var observedProgress = 0
        engineSession.register(object : EngineSession.Observer {
            override fun onProgress(progress: Int) { observedProgress = progress }
        })

        engineSession.webView.webChromeClient!!.onProgressChanged(null, 100)
        assertEquals(100, observedProgress)
    }

    @Test
    fun `SystemEngineView updates current session url via onPageStart events`() {
        val engineSession = SystemEngineSession(testContext)
        val engineView = SystemEngineView(testContext)
        engineView.render(engineSession)

        assertEquals("", engineSession.currentUrl)
        engineSession.webView.webViewClient.onPageStarted(engineSession.webView, "https://www.mozilla.org/", null)
        assertEquals("https://www.mozilla.org/", engineSession.currentUrl)

        engineSession.webView.webViewClient.onPageStarted(engineSession.webView, "https://www.firefox.com/", null)
        assertEquals("https://www.firefox.com/", engineSession.currentUrl)
    }

    @Test
    fun `WebView client notifies navigation state changes`() {
        val engineSession = SystemEngineSession(testContext)
        val engineView = SystemEngineView(testContext)

        val observer: EngineSession.Observer = mock()
        val webView: WebView = mock()
        whenever(webView.canGoBack()).thenReturn(true)
        whenever(webView.canGoForward()).thenReturn(true)

        engineSession.register(observer)
        verify(observer, never()).onNavigationStateChange(true, true)

        engineView.render(engineSession)
        engineSession.webView.webViewClient.onPageStarted(webView, "https://www.mozilla.org/", null)
        verify(observer).onNavigationStateChange(true, true)
    }

    @Test
    fun `WebView client notifies configured history delegate of url visits`() = runBlocking {
        val engineSession = SystemEngineSession(testContext)

        val engineView = SystemEngineView(testContext)
        val webView: WebView = mock()
        val historyDelegate: HistoryTrackingDelegate = mock()

        engineView.render(engineSession)

        // Nothing breaks if delegate isn't set.
        engineSession.webView.webViewClient.doUpdateVisitedHistory(webView, "https://www.mozilla.com", false)

        engineSession.settings.historyTrackingDelegate = historyDelegate
        whenever(historyDelegate.shouldStoreUri(any())).thenReturn(true)

        engineSession.webView.webViewClient.doUpdateVisitedHistory(webView, "https://www.mozilla.com", false)
        verify(historyDelegate).onVisited(eq("https://www.mozilla.com"), eq(PageVisit(VisitType.LINK, RedirectSource.NOT_A_SOURCE)))

        engineSession.webView.webViewClient.doUpdateVisitedHistory(webView, "https://www.mozilla.com", true)
        verify(historyDelegate).onVisited(eq("https://www.mozilla.com"), eq(PageVisit(VisitType.RELOAD, RedirectSource.NOT_A_SOURCE)))
    }

    @Test
    fun `WebView client checks with the delegate if the URI visit should be recorded`() = runBlocking {
        val engineSession = SystemEngineSession(testContext)
        val engineView = SystemEngineView(testContext)
        val webView: WebView = mock()
        engineView.render(engineSession)

        val historyDelegate: HistoryTrackingDelegate = mock()
        engineSession.settings.historyTrackingDelegate = historyDelegate

        whenever(historyDelegate.shouldStoreUri("https://www.mozilla.com")).thenReturn(true)

        // Verify that engine session asked delegate if uri should be stored.
        engineSession.webView.webViewClient.doUpdateVisitedHistory(webView, "https://www.mozilla.com", false)
        verify(historyDelegate).onVisited(eq("https://www.mozilla.com"), eq(PageVisit(VisitType.LINK, RedirectSource.NOT_A_SOURCE)))
        verify(historyDelegate).shouldStoreUri("https://www.mozilla.com")

        // Verify that engine won't try to store a uri that delegate doesn't want.
        engineSession.webView.webViewClient.doUpdateVisitedHistory(webView, "https://www.mozilla.com/not-allowed", false)
        verify(historyDelegate, never()).onVisited(eq("https://www.mozilla.com/not-allowed"), any())
        verify(historyDelegate).shouldStoreUri("https://www.mozilla.com/not-allowed")
        Unit
    }

    @Test
    fun `WebView client requests history from configured history delegate`() {
        val engineSession = SystemEngineSession(testContext)

        val engineView = SystemEngineView(testContext)
        val historyDelegate = object : HistoryTrackingDelegate {
            override suspend fun onVisited(uri: String, visit: PageVisit) {
                fail()
            }

            override fun shouldStoreUri(uri: String): Boolean {
                return true
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
        engineSession.webView.webChromeClient!!.getVisitedHistory(mock())

        engineSession.settings.historyTrackingDelegate = historyDelegate

        val historyValueCallback: ValueCallback<Array<String>> = mock()
        runBlocking {
            engineSession.webView.webChromeClient!!.getVisitedHistory(historyValueCallback)
        }
        verify(historyValueCallback).onReceiveValue(arrayOf("https://www.mozilla.com"))
    }

    @Test
    fun `WebView client notifies configured history delegate of title changes`() = runBlocking {
        val engineSession = SystemEngineSession(testContext)

        val engineView = SystemEngineView(testContext)
        val webView: WebView = mock()
        val historyDelegate: HistoryTrackingDelegate = mock()

        engineView.render(engineSession)

        // Nothing breaks if delegate isn't set.
        engineSession.webView.webChromeClient!!.onReceivedTitle(webView, "New title!")

        // We can now set the delegate. Were it set before the render call,
        // it'll get overwritten during settings initialization.
        engineSession.settings.historyTrackingDelegate = historyDelegate

        // Delegate not notified if, somehow, there's no currentUrl present in the view.
        engineSession.webView.webChromeClient!!.onReceivedTitle(webView, "New title!")
        verify(historyDelegate, never()).onTitleChanged(eq(""), eq("New title!"))

        // This sets the currentUrl.
        engineSession.webView.webViewClient.onPageStarted(webView, "https://www.mozilla.org/", null)

        engineSession.webView.webChromeClient!!.onReceivedTitle(webView, "New title!")
        verify(historyDelegate).onTitleChanged(eq("https://www.mozilla.org/"), eq("New title!"))

        reset(historyDelegate)

        // Empty title when none provided
        engineSession.webView.webChromeClient!!.onReceivedTitle(webView, null)
        verify(historyDelegate).onTitleChanged(eq("https://www.mozilla.org/"), eq(""))
    }

    @Test
    fun `WebView client notifies observers about title changes`() {
        val engineSession = SystemEngineSession(testContext)

        val engineView = SystemEngineView(testContext)
        val observer: EngineSession.Observer = mock()
        val webView: WebView = mock()
        whenever(webView.canGoBack()).thenReturn(true)
        whenever(webView.canGoForward()).thenReturn(true)

        engineSession.register(observer)
        engineView.render(engineSession)
        engineSession.webView.webChromeClient!!.onReceivedTitle(webView, "Hello World!")
        verify(observer).onTitleChange(eq("Hello World!"))
        verify(observer).onNavigationStateChange(true, true)

        reset(observer)

        // Empty title when none provided.
        engineSession.webView.webChromeClient!!.onReceivedTitle(webView, null)
        verify(observer).onTitleChange(eq(""))
        verify(observer).onNavigationStateChange(true, true)
    }

    @Test
    fun `download listener notifies observers`() {
        val engineSession = SystemEngineSession(testContext)
        val engineView = SystemEngineView(testContext)
        engineView.render(engineSession)

        var observerNotified = false

        engineSession.register(object : EngineSession.Observer {
            override fun onExternalResource(
                url: String,
                fileName: String?,
                contentLength: Long?,
                contentType: String?,
                cookie: String?,
                userAgent: String?,
                isPrivate: Boolean,
                response: Response?
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
            1337
        )

        assertTrue(observerNotified)
    }

    @Test
    fun `WebView client tracking protection`() {
        SystemEngineView.URL_MATCHER = UrlMatcher(arrayOf("blocked.random"))

        val engineSession = SystemEngineSession(testContext)
        val engineView = SystemEngineView(testContext)
        engineView.render(engineSession)

        val webViewClient = engineSession.webView.webViewClient
        val invalidRequest = mock<WebResourceRequest>()
        whenever(invalidRequest.isForMainFrame).thenReturn(false)
        whenever(invalidRequest.url).thenReturn(Uri.parse("market://foo.bar/"))

        var response = webViewClient.shouldInterceptRequest(engineSession.webView, invalidRequest)
        assertNull(response)

        engineSession.trackingProtectionPolicy = TrackingProtectionPolicy.strict()
        response = webViewClient.shouldInterceptRequest(engineSession.webView, invalidRequest)
        assertNotNull(response)
        assertNull(response!!.data)
        assertNull(response.encoding)
        assertNull(response.mimeType)

        val faviconRequest = mock<WebResourceRequest>()
        whenever(faviconRequest.isForMainFrame).thenReturn(false)
        whenever(faviconRequest.url).thenReturn(Uri.parse("http://foo/favicon.ico"))
        response = webViewClient.shouldInterceptRequest(engineSession.webView, faviconRequest)
        assertNotNull(response)
        assertNull(response!!.data)
        assertNull(response.encoding)
        assertNull(response.mimeType)

        val blockedRequest = mock<WebResourceRequest>()
        whenever(blockedRequest.isForMainFrame).thenReturn(false)
        whenever(blockedRequest.url).thenReturn(Uri.parse("http://blocked.random"))

        var trackerBlocked: Tracker? = null
        engineSession.register(object : EngineSession.Observer {
            override fun onTrackerBlocked(tracker: Tracker) {
                trackerBlocked = tracker
            }
        })

        response = webViewClient.shouldInterceptRequest(engineSession.webView, blockedRequest)
        assertNotNull(response)
        assertNull(response!!.data)
        assertNull(response.encoding)
        assertNull(response.mimeType)
        assertTrue(trackerBlocked!!.trackingCategories.isEmpty())
    }

    @Test
    fun `blocked trackers are reported with correct categories`() {
        val BLOCK_LIST = """{
      "license": "test-license",
      "categories": {
        "Advertising": [
          {
            "AdTest1": {
              "http://www.adtest1.com/": [
                "adtest1.com"
              ]
            }
          }
        ],
        "Analytics": [
          {
            "AnalyticsTest": {
                "http://analyticsTest1.com/": [
                  "analyticsTest1.com"
                ]
            }
          }
        ],
        "Content": [
          {
            "ContentTest1": {
              "http://contenttest1.com/": [
                "contenttest1.com"
              ]
            }
          }
        ],
        "Social": [
          {
            "SocialTest1": {
              "http://www.socialtest1.com/": [
                "socialtest1.com"
               ]
              }
            }
        ]
      }
        }
    """
        SystemEngineView.URL_MATCHER = UrlMatcher.createMatcher(
            StringReader(BLOCK_LIST),
            StringReader("{}")
        )

        val engineSession = SystemEngineSession(testContext)
        val engineView = SystemEngineView(testContext)
        var trackerBlocked: Tracker? = null

        engineView.render(engineSession)
        val webViewClient = engineSession.webView.webViewClient

        engineSession.trackingProtectionPolicy = TrackingProtectionPolicy.strict()

        engineSession.register(object : EngineSession.Observer {
            override fun onTrackerBlocked(tracker: Tracker) {
                trackerBlocked = tracker
            }
        })

        val blockedRequest = mock<WebResourceRequest>()
        whenever(blockedRequest.isForMainFrame).thenReturn(false)

        whenever(blockedRequest.url).thenReturn(Uri.parse("http://www.adtest1.com/"))
        webViewClient.shouldInterceptRequest(engineSession.webView, blockedRequest)

        assertTrue(trackerBlocked!!.trackingCategories.first() == TrackingCategory.AD)

        whenever(blockedRequest.url).thenReturn(Uri.parse("http://analyticsTest1.com/"))
        webViewClient.shouldInterceptRequest(engineSession.webView, blockedRequest)

        assertTrue(trackerBlocked!!.trackingCategories.first() == TrackingCategory.ANALYTICS)

        whenever(blockedRequest.url).thenReturn(Uri.parse("http://www.socialtest1.com/"))
        webViewClient.shouldInterceptRequest(engineSession.webView, blockedRequest)

        assertTrue(trackerBlocked!!.trackingCategories.first() == TrackingCategory.SOCIAL)

        SystemEngineView.URL_MATCHER = null
    }

    @Test
    @Suppress("Deprecation")
    fun `WebViewClient calls interceptor from deprecated onReceivedError API`() {
        val engineSession = spy(SystemEngineSession(testContext))
        val engineView = SystemEngineView(testContext)
        engineView.render(engineSession)
        doNothing().`when`(engineSession).initSettings()

        val requestInterceptor: RequestInterceptor = mock()
        val webViewClient = engineSession.webView.webViewClient

        // No session or interceptor attached.
        webViewClient.onReceivedError(
            engineSession.webView,
            WebViewClient.ERROR_UNKNOWN,
            null,
            "http://failed.random"
        )
        verifyNoInteractions(requestInterceptor)

        // Session attached, but not interceptor.
        engineView.render(engineSession)
        webViewClient.onReceivedError(
            engineSession.webView,
            WebViewClient.ERROR_UNKNOWN,
            null,
            "http://failed.random"
        )
        verifyNoInteractions(requestInterceptor)

        // Session and interceptor.
        engineSession.settings.requestInterceptor = requestInterceptor
        webViewClient.onReceivedError(
            engineSession.webView,
            WebViewClient.ERROR_UNKNOWN,
            null,
            "http://failed.random"
        )
        verify(requestInterceptor).onErrorRequest(engineSession, ErrorType.UNKNOWN, "http://failed.random")

        val webView = mock<WebView>()
        val settings = mock<WebSettings>()
        whenever(webView.settings).thenReturn(settings)

        engineSession.webView = webView
        val errorResponse = RequestInterceptor.ErrorResponse("about:fail")
        webViewClient.onReceivedError(
            engineSession.webView,
            WebViewClient.ERROR_UNKNOWN,
            null,
            "http://failed.random"
        )
        verify(webView, never()).loadUrl(ArgumentMatchers.anyString())

        whenever(requestInterceptor.onErrorRequest(engineSession, ErrorType.UNKNOWN, "http://failed.random"))
            .thenReturn(errorResponse)
        webViewClient.onReceivedError(
            engineSession.webView,
            WebViewClient.ERROR_UNKNOWN,
            null,
            "http://failed.random"
        )
        verify(webView).loadUrl("about:fail")

        val errorResponse2 = RequestInterceptor.ErrorResponse("about:fail2")
        webViewClient.onReceivedError(
            engineSession.webView,
            WebViewClient.ERROR_UNKNOWN,
            null,
            "http://failed.random"
        )
        verify(webView, never()).loadUrl("about:fail2")

        whenever(requestInterceptor.onErrorRequest(engineSession, ErrorType.UNKNOWN, "http://failed.random"))
            .thenReturn(errorResponse2)
        webViewClient.onReceivedError(
            engineSession.webView,
            WebViewClient.ERROR_UNKNOWN,
            null,
            "http://failed.random"
        )
        verify(webView).loadUrl("about:fail2")
    }

    @Test
    fun `WebViewClient calls interceptor from new onReceivedError API`() {
        val engineSession = spy(SystemEngineSession(testContext))
        val engineView = SystemEngineView(testContext)
        engineView.render(engineSession)
        doNothing().`when`(engineSession).initSettings()

        val requestInterceptor: RequestInterceptor = mock()
        val webViewClient = engineSession.webView.webViewClient
        val webRequest: WebResourceRequest = mock()
        val webError: WebResourceError = mock()
        val url: Uri = mock()

        webViewClient.onReceivedError(engineSession.webView, webRequest, webError)
        verifyNoInteractions(requestInterceptor)

        engineView.render(engineSession)
        webViewClient.onReceivedError(engineSession.webView, webRequest, webError)
        verifyNoInteractions(requestInterceptor)

        whenever(webError.errorCode).thenReturn(WebViewClient.ERROR_UNKNOWN)
        whenever(webRequest.url).thenReturn(url)
        whenever(url.toString()).thenReturn("http://failed.random")
        engineSession.settings.requestInterceptor = requestInterceptor
        webViewClient.onReceivedError(engineSession.webView, webRequest, webError)
        verify(requestInterceptor, never()).onErrorRequest(engineSession, ErrorType.UNKNOWN, "http://failed.random")

        whenever(webRequest.isForMainFrame).thenReturn(true)
        webViewClient.onReceivedError(engineSession.webView, webRequest, webError)
        verify(requestInterceptor).onErrorRequest(engineSession, ErrorType.UNKNOWN, "http://failed.random")

        val webView = mock<WebView>()
        val settings = mock<WebSettings>()
        whenever(webView.settings).thenReturn(settings)

        engineSession.webView = webView
        val errorResponse = RequestInterceptor.ErrorResponse("about:fail")
        webViewClient.onReceivedError(engineSession.webView, webRequest, webError)
        verify(webView, never()).loadUrl(ArgumentMatchers.anyString())

        whenever(requestInterceptor.onErrorRequest(engineSession, ErrorType.UNKNOWN, "http://failed.random"))
            .thenReturn(errorResponse)
        webViewClient.onReceivedError(engineSession.webView, webRequest, webError)
        verify(webView).loadUrl("about:fail")

        val errorResponse2 = RequestInterceptor.ErrorResponse("about:fail2")
        webViewClient.onReceivedError(engineSession.webView, webRequest, webError)
        verify(webView, never()).loadUrl("about:fail2")

        whenever(requestInterceptor.onErrorRequest(engineSession, ErrorType.UNKNOWN, "http://failed.random"))
            .thenReturn(errorResponse2)
        webViewClient.onReceivedError(engineSession.webView, webRequest, webError)
        verify(webView).loadUrl("about:fail2")
    }

    @Test
    fun `WebViewClient calls interceptor when onReceivedSslError`() {
        val engineSession = spy(SystemEngineSession(testContext))
        val engineView = SystemEngineView(testContext)
        engineView.render(engineSession)
        doNothing().`when`(engineSession).initSettings()

        val requestInterceptor: RequestInterceptor = mock()
        val webViewClient = engineSession.webView.webViewClient
        val handler: SslErrorHandler = mock()
        val error: SslError = mock()

        webViewClient.onReceivedSslError(engineSession.webView, handler, error)
        verifyNoInteractions(requestInterceptor)

        engineView.render(engineSession)
        webViewClient.onReceivedSslError(engineSession.webView, handler, error)
        verifyNoInteractions(requestInterceptor)

        whenever(error.primaryError).thenReturn(SslError.SSL_EXPIRED)
        whenever(error.url).thenReturn("http://failed.random")
        engineSession.settings.requestInterceptor = requestInterceptor
        webViewClient.onReceivedSslError(engineSession.webView, handler, error)
        verify(requestInterceptor).onErrorRequest(engineSession, ErrorType.ERROR_SECURITY_SSL, "http://failed.random")
        verify(handler, times(3)).cancel()

        val webView = mock<WebView>()
        val settings = mock<WebSettings>()
        whenever(webView.settings).thenReturn(settings)

        engineSession.webView = webView
        val errorResponse = RequestInterceptor.ErrorResponse("about:fail")
        webViewClient.onReceivedSslError(engineSession.webView, handler, error)
        verify(webView, never()).loadUrl(ArgumentMatchers.anyString())

        whenever(
            requestInterceptor.onErrorRequest(
                engineSession,
                ErrorType.ERROR_SECURITY_SSL,
                "http://failed.random"
            )
        ).thenReturn(errorResponse)
        webViewClient.onReceivedSslError(engineSession.webView, handler, error)
        verify(webView).loadUrl("about:fail")

        val errorResponse2 = RequestInterceptor.ErrorResponse("about:fail2")
        webViewClient.onReceivedSslError(engineSession.webView, handler, error)
        verify(webView, never()).loadUrl("about:fail2")

        whenever(requestInterceptor.onErrorRequest(engineSession, ErrorType.ERROR_SECURITY_SSL, "http://failed.random"))
            .thenReturn(errorResponse2)
        webViewClient.onReceivedSslError(engineSession.webView, handler, error)
        verify(webView).loadUrl("about:fail2")

        whenever(requestInterceptor.onErrorRequest(engineSession, ErrorType.ERROR_SECURITY_SSL, "http://failed.random"))
            .thenReturn(RequestInterceptor.ErrorResponse("http://failed.random"))
        webViewClient.onReceivedSslError(engineSession.webView, handler, error)
        verify(webView).loadUrl("http://failed.random")
    }

    @Test
    fun `WebViewClient blocks WebFonts`() {
        val engineSession = SystemEngineSession(testContext)
        val engineView = SystemEngineView(testContext)
        engineView.render(engineSession)

        val webViewClient = engineSession.webView.webViewClient
        val webFontRequest = mock<WebResourceRequest>()
        whenever(webFontRequest.url).thenReturn(Uri.parse("/fonts/test.woff"))
        assertNull(webViewClient.shouldInterceptRequest(engineSession.webView, webFontRequest))

        engineView.render(engineSession)
        assertNull(webViewClient.shouldInterceptRequest(engineSession.webView, webFontRequest))

        engineSession.settings.webFontsEnabled = false

        val request = mock<WebResourceRequest>()
        whenever(request.url).thenReturn(Uri.parse("http://mozilla.org"))
        assertNull(webViewClient.shouldInterceptRequest(engineSession.webView, request))

        val response = webViewClient.shouldInterceptRequest(engineSession.webView, webFontRequest)
        assertNotNull(response)
        assertNull(response!!.data)
        assertNull(response.encoding)
        assertNull(response.mimeType)
    }

    @Test
    fun `FindListener notifies observers`() {
        val engineSession = SystemEngineSession(testContext)
        val engineView = SystemEngineView(testContext)
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
        val mockWebView = mock<WebView>()
        val settings = mock<WebSettings>()
        whenever(mockWebView.settings).thenReturn(settings)

        val engineSession1 = SystemEngineSession(testContext)
        val engineSession2 = SystemEngineSession(testContext)

        val engineView = SystemEngineView(testContext)
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
        val engineSession = SystemEngineSession(testContext)
        val engineView = SystemEngineView(testContext)
        engineView.render(engineSession)

        val observer: EngineSession.Observer = mock()
        engineSession.register(observer)

        val view = mock<View>()
        val customViewCallback = mock<WebChromeClient.CustomViewCallback>()
        engineSession.webView.webChromeClient!!.onShowCustomView(view, customViewCallback)

        verify(observer).onFullScreenChange(true)
    }

    @Test
    fun `addFullScreenView execs callback and removeView`() {
        val engineSession = SystemEngineSession(testContext)
        val engineView = SystemEngineView(testContext)
        engineView.render(engineSession)

        val view = View(RuntimeEnvironment.systemContext)
        val customViewCallback = mock<WebChromeClient.CustomViewCallback>()

        assertNull(engineSession.fullScreenCallback)

        engineSession.webView.webChromeClient!!.onShowCustomView(view, customViewCallback)

        assertNotNull(engineSession.fullScreenCallback)
        assertEquals(customViewCallback, engineSession.fullScreenCallback)
        assertEquals("mozac_system_engine_fullscreen", view.tag)

        engineSession.webView.webChromeClient!!.onHideCustomView()
        assertEquals(View.VISIBLE, engineSession.webView.visibility)
    }

    @Test
    fun `addFullScreenView with no matching webView`() {
        val engineSession = SystemEngineSession(testContext)
        val engineView = SystemEngineView(testContext)
        engineView.render(engineSession)

        val view = View(RuntimeEnvironment.systemContext)
        val customViewCallback = mock<WebChromeClient.CustomViewCallback>()

        engineSession.webView.tag = "not_webview"
        engineSession.webView.webChromeClient!!.onShowCustomView(view, customViewCallback)

        assertNotEquals(View.INVISIBLE, engineSession.webView.visibility)
    }

    @Test
    fun `removeFullScreenView with no matching views`() {
        val engineSession = SystemEngineSession(testContext)
        val engineView = SystemEngineView(testContext)
        engineView.render(engineSession)

        val view = View(RuntimeEnvironment.systemContext)
        val customViewCallback = mock<WebChromeClient.CustomViewCallback>()

        // When the fullscreen view isn't available
        engineSession.webView.webChromeClient!!.onShowCustomView(view, customViewCallback)
        engineView.findViewWithTag<View>("mozac_system_engine_fullscreen").tag = "not_fullscreen"

        engineSession.webView.webChromeClient!!.onHideCustomView()

        assertNotNull(engineSession.fullScreenCallback)
        verify(engineSession.fullScreenCallback, never())?.onCustomViewHidden()
        assertEquals(View.INVISIBLE, engineSession.webView.visibility)

        // When fullscreen view is available, but WebView isn't.
        engineView.findViewWithTag<View>("not_fullscreen").tag = "mozac_system_engine_fullscreen"
        engineSession.webView.tag = "not_webView"

        engineSession.webView.webChromeClient!!.onHideCustomView()

        assertEquals(View.INVISIBLE, engineSession.webView.visibility)
    }

    @Test
    fun `fullscreenCallback is null`() {
        val engineSession = SystemEngineSession(testContext)
        val engineView = SystemEngineView(testContext)
        engineView.render(engineSession)

        engineSession.webView.webChromeClient!!.onHideCustomView()
        assertNull(engineSession.fullScreenCallback)
    }

    @Test
    fun `onPageFinished handles invalid URL`() {
        val engineSession = SystemEngineSession(testContext)
        val engineView = SystemEngineView(testContext)
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
        val view = mock<WebView>()
        val certificate = mock<SslCertificate>()
        val dName = mock<SslCertificate.DName>()
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
        val resources = testContext.resources

        var urlMatcher = SystemEngineView.getOrCreateUrlMatcher(
            resources,
            TrackingProtectionPolicy.select(
                arrayOf(
                    TrackingCategory.AD,
                    TrackingCategory.ANALYTICS
                )
            )
        )
        assertEquals(setOf(UrlMatcher.ADVERTISING, UrlMatcher.ANALYTICS), urlMatcher.enabledCategories)

        urlMatcher = SystemEngineView.getOrCreateUrlMatcher(
            resources,
            TrackingProtectionPolicy.select(
                arrayOf(
                    TrackingCategory.AD,
                    TrackingCategory.SOCIAL
                )
            )
        )
        assertEquals(setOf(UrlMatcher.ADVERTISING, UrlMatcher.SOCIAL), urlMatcher.enabledCategories)
    }

    @Test
    fun `URL matcher supports compounded categories`() {
        val recommendedPolicy = TrackingProtectionPolicy.recommended()
        val strictPolicy = TrackingProtectionPolicy.strict()
        val resources = testContext.resources
        val recommendedCategories = setOf(
            UrlMatcher.ADVERTISING,
            UrlMatcher.ANALYTICS,
            UrlMatcher.SOCIAL,
            UrlMatcher.FINGERPRINTING,
            UrlMatcher.CRYPTOMINING
        )
        val strictCategories = setOf(
            UrlMatcher.ADVERTISING,
            UrlMatcher.ANALYTICS,
            UrlMatcher.SOCIAL,
            UrlMatcher.FINGERPRINTING,
            UrlMatcher.CRYPTOMINING
        )

        var urlMatcher = SystemEngineView.getOrCreateUrlMatcher(resources, recommendedPolicy)

        assertEquals(recommendedCategories, urlMatcher.enabledCategories)

        urlMatcher = SystemEngineView.getOrCreateUrlMatcher(resources, strictPolicy)

        assertEquals(strictCategories, urlMatcher.enabledCategories)
    }

    @Test
    fun `permission requests are forwarded to observers`() {
        val permissionRequest: android.webkit.PermissionRequest = mock()
        whenever(permissionRequest.resources).thenReturn(emptyArray())
        whenever(permissionRequest.origin).thenReturn(Uri.parse("https://mozilla.org"))

        val engineSession = SystemEngineSession(testContext)
        val engineView = SystemEngineView(testContext)
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

        engineSession.webView.webChromeClient!!.onPermissionRequest(permissionRequest)
        assertNotNull(observedPermissionRequest)

        engineSession.webView.webChromeClient!!.onPermissionRequestCanceled(permissionRequest)
        assertNotNull(cancelledPermissionRequest)
    }

    @Test
    fun `window requests are forwarded to observers`() {
        val permissionRequest: android.webkit.PermissionRequest = mock()
        whenever(permissionRequest.resources).thenReturn(emptyArray())
        whenever(permissionRequest.origin).thenReturn(Uri.parse("https://mozilla.org"))

        val engineSession = SystemEngineSession(testContext)
        val engineView = SystemEngineView(testContext)
        engineView.render(engineSession)

        var createWindowRequest: WindowRequest? = null
        var closeWindowRequest: WindowRequest? = null
        engineSession.register(object : EngineSession.Observer {
            override fun onWindowRequest(windowRequest: WindowRequest) {
                if (windowRequest.type == WindowRequest.Type.OPEN) {
                    createWindowRequest = windowRequest
                } else {
                    closeWindowRequest = windowRequest
                }
            }
        })

        engineSession.webView.webChromeClient!!.onCreateWindow(mock(), false, false, null)
        assertNotNull(createWindowRequest)
        assertNull(closeWindowRequest)

        engineSession.webView.webChromeClient!!.onCloseWindow(mock())
        assertNotNull(closeWindowRequest)
    }

    @Test
    fun `Calling onShowFileChooser must provide a FilePicker PromptRequest`() {
        val engineSession = SystemEngineSession(testContext)
        val engineView = SystemEngineView(testContext)
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
        val engineView = SystemEngineView(testContext)
        assertFalse(engineView.canScrollVerticallyDown())

        engineView.render(SystemEngineSession(testContext))
        assertFalse(engineView.canScrollVerticallyDown())
    }

    @Test
    fun `onLongClick can be called without session`() {
        val engineView = SystemEngineView(testContext)
        assertFalse(engineView.onLongClick(null))

        engineView.render(SystemEngineSession(testContext))
        assertFalse(engineView.onLongClick(null))
    }

    @Test
    fun `Calling onJsAlert must provide an Alert PromptRequest`() {
        val engineSession = SystemEngineSession(testContext)
        val engineView = SystemEngineView(testContext)
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
        assertEquals(alertRequest.message, "message")

        alertRequest.onConfirm(true)
        verify(mockJSResult).confirm()

        alertRequest.onDismiss()
        verify(mockJSResult).cancel()
    }

    @Test
    fun `calling onJsPrompt must provide a TextPrompt PromptRequest`() {
        val engineSession = SystemEngineSession(testContext)
        val engineView = SystemEngineView(testContext)
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

        textPromptRequest.onDismiss()
        verify(mockJSPromptResult).cancel()

        textPromptRequest.onConfirm(true, "value")
    }

    @Test
    fun `calling onJsPrompt with a null session must not provide a TextPrompt PromptRequest`() {
        val engineSession = SystemEngineSession(testContext)
        val engineView = SystemEngineView(testContext)

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
        val engineSession = SystemEngineSession(testContext)
        val engineView = SystemEngineView(testContext)
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

        confirmPromptRequest.onConfirmPositiveButton(true)
        verify(mockJSPromptResult).confirm()

        confirmPromptRequest.onDismiss()
        verify(mockJSPromptResult).cancel()

        confirmPromptRequest.onConfirmNegativeButton(true)
        verify(mockJSPromptResult, times(2)).cancel()
    }

    @Test
    @Config(sdk = [Build.VERSION_CODES.N])
    fun captureThumbnailOnPreO() {
        val activity = Robolectric.setupActivity(Activity::class.java)
        val engineView = SystemEngineView(activity)
        val webView = mock<WebView>()

        whenever(webView.width).thenReturn(100)
        whenever(webView.height).thenReturn(200)

        engineView.session = mock()

        whenever(engineView.session!!.webView).thenReturn(webView)

        var thumbnail: Bitmap? = null

        engineView.captureThumbnail {
            thumbnail = it
        }
        verify(webView).draw(any())
        assertNotNull(thumbnail)

        engineView.session = null
        engineView.captureThumbnail {
            thumbnail = it
        }

        assertNull(thumbnail)
    }

    @Test
    @Config(sdk = [Build.VERSION_CODES.O], shadows = [PixelCopyShadow::class])
    fun captureThumbnailOnPostO() {
        val activity = Robolectric.setupActivity(Activity::class.java)
        val engineView = SystemEngineView(activity)
        val webView = mock<WebView>()
        whenever(webView.width).thenReturn(100)
        whenever(webView.height).thenReturn(200)

        var thumbnail: Bitmap? = null

        engineView.session = null
        engineView.captureThumbnail {
            thumbnail = it
        }
        assertNull(thumbnail)

        engineView.session = mock()
        whenever(engineView.session!!.webView).thenReturn(webView)

        PixelCopyShadow.copyResult = PixelCopy.ERROR_UNKNOWN
        engineView.captureThumbnail {
            thumbnail = it
        }
        assertNull(thumbnail)

        PixelCopyShadow.copyResult = PixelCopy.SUCCESS
        engineView.captureThumbnail {
            thumbnail = it
        }
        assertNotNull(thumbnail)
    }

    @Test
    fun `calling onReceivedHttpAuthRequest must provide an Authentication PromptRequest`() {
        val engineSession = SystemEngineSession(testContext)
        val engineView = SystemEngineView(testContext)
        var request: PromptRequest? = null

        engineSession.register(object : EngineSession.Observer {
            override fun onPromptRequest(promptRequest: PromptRequest) {
                request = promptRequest
            }
        })
        engineView.render(engineSession)

        val authHandler = mock<HttpAuthHandler>()
        val host = "mozilla.org"
        val realm = "realm"

        engineSession.webView.webViewClient.onReceivedHttpAuthRequest(engineSession.webView, authHandler, host, realm)

        val authRequest = request as PromptRequest.Authentication
        assertTrue(request is PromptRequest.Authentication)

        assertEquals(authRequest.title, "")

        authRequest.onConfirm("u", "p")
        verify(authHandler).proceed("u", "p")

        authRequest.onDismiss()
        verify(authHandler).cancel()
    }

    @Test
    fun `calling onReceivedHttpAuthRequest with a null session must not provide an Authentication PromptRequest`() {
        val engineSession = SystemEngineSession(testContext)
        val engineView = SystemEngineView(testContext)
        var request: PromptRequest? = null

        engineSession.register(object : EngineSession.Observer {
            override fun onPromptRequest(promptRequest: PromptRequest) {
                request = promptRequest
            }
        })
        engineView.render(engineSession)

        val authHandler = mock<HttpAuthHandler>()
        engineView.session = null

        engineSession.webView.webViewClient.onReceivedHttpAuthRequest(mock(), authHandler, "mozilla.org", "realm")

        assertNull(request)
        verify(authHandler).cancel()
    }

    @Test
    fun `onReceivedHttpAuthRequest correctly handles realm`() {
        val engineSession = SystemEngineSession(testContext)
        val engineView = SystemEngineView(testContext)

        var request: PromptRequest? = null

        engineSession.register(object : EngineSession.Observer {
            override fun onPromptRequest(promptRequest: PromptRequest) {
                request = promptRequest
            }
        })
        engineView.render(engineSession)

        val webView = engineSession.webView
        val authHandler = mock<HttpAuthHandler>()
        val host = "mozilla.org"

        val longRealm = "Login with a user name of httpwatch and a different password each time"
        webView.webViewClient.onReceivedHttpAuthRequest(webView, authHandler, host, longRealm)
        assertTrue((request as PromptRequest.Authentication).message.endsWith("differen…”"))

        val emptyRealm = ""
        webView.webViewClient.onReceivedHttpAuthRequest(webView, authHandler, host, emptyRealm)
        val noRealmMessageTail = testContext.getString(R.string.mozac_browser_engine_system_auth_no_realm_message).let {
            it.substring(it.length - 10)
        }
        assertTrue((request as PromptRequest.Authentication).message.endsWith(noRealmMessageTail))
    }

    @Test
    @Config(sdk = [Build.VERSION_CODES.N])
    @Suppress("Deprecation")
    fun `onReceivedHttpAuthRequest takes credentials from WebView`() {
        val engineSession = SystemEngineSession(testContext)
        val engineView = SystemEngineView(testContext)
        var request: PromptRequest? = null

        engineSession.register(object : EngineSession.Observer {
            override fun onPromptRequest(promptRequest: PromptRequest) {
                request = promptRequest
            }
        })

        engineSession.webView = spy(engineSession.webView)
        engineView.render(engineSession)

        // use captor as getWebViewClient() is available only from Oreo
        // and this test runs on N to not use WebViewDatabase
        val captor = argumentCaptor<WebViewClient>()
        verify(engineSession.webView).webViewClient = captor.capture()
        val webViewClient = captor.value

        val host = "mozilla.org"
        val realm = "realm"
        val userName = "user123"
        val password = "pass@123"

        val validCredentials = arrayOf(userName, password)
        whenever(engineSession.webView.getHttpAuthUsernamePassword(host, realm)).thenReturn(validCredentials)
        webViewClient.onReceivedHttpAuthRequest(engineSession.webView, mock(), host, realm)
        assertEquals((request as PromptRequest.Authentication).userName, userName)
        assertEquals((request as PromptRequest.Authentication).password, password)

        val nullCredentials = null
        whenever(engineSession.webView.getHttpAuthUsernamePassword(host, realm)).thenReturn(nullCredentials)
        webViewClient.onReceivedHttpAuthRequest(engineSession.webView, mock(), host, realm)
        assertEquals((request as PromptRequest.Authentication).userName, "")
        assertEquals((request as PromptRequest.Authentication).password, "")

        val credentialsWithNulls = arrayOf<String?>(null, null)
        whenever(engineSession.webView.getHttpAuthUsernamePassword(host, realm)).thenReturn(credentialsWithNulls)
        webViewClient.onReceivedHttpAuthRequest(engineSession.webView, mock(), host, realm)
        assertEquals((request as PromptRequest.Authentication).userName, "")
        assertEquals((request as PromptRequest.Authentication).password, "")
    }

    @Test
    @Config(sdk = [Build.VERSION_CODES.O])
    fun `onReceivedHttpAuthRequest uses WebViewDatabase on Oreo+`() {
        val engineSession = spy(SystemEngineSession(testContext))
        val engineView = SystemEngineView(testContext)
        var request: PromptRequest? = null

        engineSession.register(object : EngineSession.Observer {
            override fun onPromptRequest(promptRequest: PromptRequest) {
                request = promptRequest
            }
        })
        engineView.render(engineSession)

        val host = "mozilla.org"
        val realm = "realm"
        val userName = "userFromDB"
        val password = "pass@123FromDB"
        val webViewDatabase = mock<WebViewDatabase>()
        whenever(webViewDatabase.getHttpAuthUsernamePassword(host, realm)).thenReturn(arrayOf(userName, password))
        whenever(engineSession.webViewDatabase(testContext)).thenReturn(webViewDatabase)

        engineSession.webView.webViewClient.onReceivedHttpAuthRequest(engineSession.webView, mock(), host, realm)

        val authRequest = request as PromptRequest.Authentication
        assertEquals(authRequest.userName, userName)
        assertEquals(authRequest.password, password)
    }

    @Test
    fun `GIVEN SystemEngineView WHEN getInputResultDetail is called THEN it returns the instance from webView`() {
        val engineView = SystemEngineView(testContext)
        val engineSession = SystemEngineSession(testContext)
        val webView = spy(NestedWebView(testContext))
        engineSession.webView = webView
        engineView.render(engineSession)
        val inputResult = InputResultDetail.newInstance()
        doReturn(inputResult).`when`(webView).inputResultDetail

        assertSame(inputResult, engineView.getInputResultDetail())
    }

    @Test
    fun `GIVEN SystemEngineView WHEN getInputResultDetail is called THEN it returns a new default instance if not available from webView`() {
        val engineView = spy(SystemEngineView(testContext))

        val result = engineView.getInputResultDetail()

        assertNotNull(result)
        assertTrue(result.isTouchHandlingUnknown())
        assertFalse(result.canScrollToLeft())
        assertFalse(result.canScrollToTop())
        assertFalse(result.canScrollToRight())
        assertFalse(result.canScrollToBottom())
        assertFalse(result.canOverscrollLeft())
        assertFalse(result.canOverscrollTop())
        assertFalse(result.canOverscrollRight())
        assertFalse(result.canOverscrollBottom())
    }
}
