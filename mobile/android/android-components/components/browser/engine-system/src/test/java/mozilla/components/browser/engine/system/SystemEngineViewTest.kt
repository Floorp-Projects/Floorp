/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

package mozilla.components.browser.engine.system

import android.net.Uri
import android.net.http.SslCertificate
import android.os.Bundle
import android.os.Message
import android.view.View
import android.webkit.WebChromeClient
import android.webkit.WebResourceRequest
import android.webkit.WebView
import android.webkit.WebView.HitTestResult
import mozilla.components.browser.engine.system.matcher.UrlMatcher
import mozilla.components.concept.engine.EngineSession
import mozilla.components.concept.engine.HitResult
import mozilla.components.support.test.eq
import mozilla.components.support.test.mock
import org.junit.Assert.assertEquals
import org.junit.Assert.assertFalse
import org.junit.Assert.assertNotEquals
import org.junit.Assert.assertNotNull
import org.junit.Assert.assertNull
import org.junit.Assert.assertTrue
import org.junit.Test
import org.junit.runner.RunWith
import org.mockito.Mockito.`when`
import org.mockito.Mockito.doReturn
import org.mockito.Mockito.mock
import org.mockito.Mockito.never
import org.mockito.Mockito.verify
import org.robolectric.RobolectricTestRunner
import org.robolectric.RuntimeEnvironment

@RunWith(RobolectricTestRunner::class)
class SystemEngineViewTest {

    @Test
    fun `EngineView initialization`() {
        val engineView = SystemEngineView(RuntimeEnvironment.application)

        assertNotNull(engineView.currentWebView.webChromeClient)
        assertNotNull(engineView.currentWebView.webViewClient)
        assertEquals(engineView.currentWebView, engineView.getChildAt(0))
    }

    @Test
    fun `WebViewClient notifies observers`() {
        val engineSession = SystemEngineSession()
        val engineView = SystemEngineView(RuntimeEnvironment.application)
        engineView.render(engineSession)

        var observedUrl = ""
        var observedLoadingState = false
        var observedCanGoBack = false
        var observedCanGoForward = false
        var observedSecurityChange: Triple<Boolean, String?, String?> = Triple(false, null, null)
        engineSession.register(object : EngineSession.Observer {
            override fun onLoadingStateChange(loading: Boolean) { observedLoadingState = loading }
            override fun onLocationChange(url: String) { observedUrl = url }
            override fun onNavigationStateChange(canGoBack: Boolean?, canGoForward: Boolean?) {
                observedCanGoBack = true
                observedCanGoForward = true
            }
            override fun onSecurityChange(secure: Boolean, host: String?, issuer: String?) {
                observedSecurityChange = Triple(secure, host, issuer)
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
        assertEquals(Triple(false, null, null), observedSecurityChange)

        val view = mock(WebView::class.java)
        engineView.currentWebView.webViewClient.onPageFinished(view, "http://mozilla.org")
        assertEquals(Triple(false, null, null), observedSecurityChange)

        val certificate = mock(SslCertificate::class.java)
        val dName = mock(SslCertificate.DName::class.java)
        doReturn("testCA").`when`(dName).oName
        doReturn(dName).`when`(certificate).issuedBy
        doReturn(certificate).`when`(view).certificate
        engineView.currentWebView.webViewClient.onPageFinished(view, "http://mozilla.org")
        assertEquals(Triple(true, "mozilla.org", "testCA"), observedSecurityChange)
    }

    @Test
    fun `HitResult type handling`() {
        val engineSession = SystemEngineSession()
        val engineView = SystemEngineView(RuntimeEnvironment.application)
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
        val engineSession = SystemEngineSession()
        val engineView = SystemEngineView(RuntimeEnvironment.application)
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
        val engineSession = SystemEngineSession()
        val handler = SystemEngineView.ImageHandler(engineSession)
        val message = mock(Message::class.java)
        val bundle = mock(Bundle::class.java)

        `when`(message.data).thenReturn(bundle)
        `when`(message.data.getString("url")).thenReturn("https://mozilla.org")

        handler.handleMessage(message)
    }

    @Test(expected = IllegalStateException::class)
    fun `null image url`() {
        val engineSession = SystemEngineSession()
        val handler = SystemEngineView.ImageHandler(engineSession)
        val message = mock(Message::class.java)
        val bundle = mock(Bundle::class.java)

        `when`(message.data).thenReturn(bundle)
        `when`(message.data.getString("src")).thenReturn("file.png")

        handler.handleMessage(message)
    }

    @Test
    fun `WebChromeClient notifies observers`() {
        val engineSession = SystemEngineSession()
        val engineView = SystemEngineView(RuntimeEnvironment.application)
        engineView.render(engineSession)

        var observedProgress = 0
        engineSession.register(object : EngineSession.Observer {
            override fun onProgress(progress: Int) { observedProgress = progress }
        })

        engineView.currentWebView.webChromeClient.onProgressChanged(null, 100)
        assertEquals(100, observedProgress)
    }

    @Test
    fun `WebView client notifies observers about title changes`() {
        val engineSession = SystemEngineSession()

        val engineView = SystemEngineView(RuntimeEnvironment.application)
        engineView.render(engineSession)

        val observer: EngineSession.Observer = mock()
        engineSession.register(observer)

        engineView.currentWebView.webChromeClient.onReceivedTitle(engineView.currentWebView, "Hello World!")

        verify(observer).onTitleChange(eq("Hello World!"))
    }

    @Test
    fun `download listener notifies observers`() {
        val engineSession = SystemEngineSession()
        val engineView = SystemEngineView(RuntimeEnvironment.application)
        engineView.render(engineSession)

        var observerNotified = false

        engineSession.register(object : EngineSession.Observer {
            override fun onExternalResource(
                url: String,
                fileName: String?,
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

        val engineSession = SystemEngineSession()
        val engineView = SystemEngineView(RuntimeEnvironment.application)
        engineView.render(engineSession)

        val webViewClient = engineView.currentWebView.webViewClient
        val invalidRequest = mock(WebResourceRequest::class.java)
        `when`(invalidRequest.isForMainFrame).thenReturn(false)
        `when`(invalidRequest.url).thenReturn(Uri.parse("market://foo.bar/"))

        var response = webViewClient.shouldInterceptRequest(engineView.currentWebView, invalidRequest)
        assertNull(response)

        engineSession.trackingProtectionEnabled = true
        response = webViewClient.shouldInterceptRequest(engineView.currentWebView, invalidRequest)
        assertNotNull(response)
        assertNull(response.data)
        assertNull(response.encoding)
        assertNull(response.mimeType)

        val faviconRequest = mock(WebResourceRequest::class.java)
        `when`(faviconRequest.isForMainFrame).thenReturn(false)
        `when`(faviconRequest.url).thenReturn(Uri.parse("http://foo/favicon.ico"))
        response = webViewClient.shouldInterceptRequest(engineView.currentWebView, faviconRequest)
        assertNotNull(response)
        assertNull(response.data)
        assertNull(response.encoding)
        assertNull(response.mimeType)

        val blockedRequest = mock(WebResourceRequest::class.java)
        `when`(blockedRequest.isForMainFrame).thenReturn(false)
        `when`(blockedRequest.url).thenReturn(Uri.parse("http://blocked.random"))
        response = webViewClient.shouldInterceptRequest(engineView.currentWebView, blockedRequest)
        assertNotNull(response)
        assertNull(response.data)
        assertNull(response.encoding)
        assertNull(response.mimeType)
    }

    @Test
    fun testWebViewClientBlocksWebFonts() {
        val engineSession = SystemEngineSession()
        val engineView = SystemEngineView(RuntimeEnvironment.application)
        val webViewClient = engineView.currentWebView.webViewClient
        val webFontRequest = mock(WebResourceRequest::class.java)
        `when`(webFontRequest.url).thenReturn(Uri.parse("/fonts/test.woff"))
        assertNull(webViewClient.shouldInterceptRequest(engineView.currentWebView, webFontRequest))

        engineView.render(engineSession)
        assertNull(webViewClient.shouldInterceptRequest(engineView.currentWebView, webFontRequest))

        engineSession.settings.webFontsEnabled = false

        val request = mock(WebResourceRequest::class.java)
        `when`(request.url).thenReturn(Uri.parse("http://mozilla.org"))
        assertNull(webViewClient.shouldInterceptRequest(engineView.currentWebView, request))

        val response = webViewClient.shouldInterceptRequest(engineView.currentWebView, webFontRequest)
        assertNotNull(response)
        assertNull(response.data)
        assertNull(response.encoding)
        assertNull(response.mimeType)
    }

    @Test
    fun `FindListener notifies observers`() {
        val engineSession = SystemEngineSession()
        val engineView = SystemEngineView(RuntimeEnvironment.application)
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
        val webView = mock(WebView::class.java)
        val engineView = SystemEngineView(RuntimeEnvironment.application)
        engineView.currentWebView = webView

        engineView.onPause()
        verify(webView).onPause()
        verify(webView).pauseTimers()

        engineView.onResume()
        verify(webView).onResume()
        verify(webView).resumeTimers()

        engineView.onDestroy()
        verify(webView).destroy()
    }

    @Test
    fun `showCustomView notifies fullscreen mode observers and execs callback`() {
        val engineSession = SystemEngineSession()
        val engineView = SystemEngineView(RuntimeEnvironment.application)
        engineView.render(engineSession)

        val observer: EngineSession.Observer = mock()
        engineSession.register(observer)

        val view = mock(View::class.java)
        val customViewCallback = mock(WebChromeClient.CustomViewCallback::class.java)

        engineView.currentWebView.webChromeClient.onShowCustomView(view, customViewCallback)

        verify(observer).onFullScreenChange(true)
    }

    @Test
    fun `addFullScreenView execs callback and removeView`() {
        val engineSession = SystemEngineSession()
        val engineView = SystemEngineView(RuntimeEnvironment.application)
        engineView.render(engineSession)

        val view = View(RuntimeEnvironment.systemContext)
        val customViewCallback = mock(WebChromeClient.CustomViewCallback::class.java)

        assertNull(engineView.fullScreenCallback)

        engineView.currentWebView.webChromeClient.onShowCustomView(view, customViewCallback)

        assertNotNull(engineView.fullScreenCallback)
        assertEquals(customViewCallback, engineView.fullScreenCallback)
        assertEquals("mosac_system_engine_fullscreen", view.tag)

        engineView.currentWebView.webChromeClient.onHideCustomView()
        assertEquals(View.VISIBLE, engineView.currentWebView.visibility)
    }

    @Test
    fun `addFullScreenView with no matching webView`() {
        val engineSession = SystemEngineSession()
        val engineView = SystemEngineView(RuntimeEnvironment.application)
        engineView.render(engineSession)

        val view = View(RuntimeEnvironment.systemContext)
        val customViewCallback = mock(WebChromeClient.CustomViewCallback::class.java)

        engineView.currentWebView.tag = "not_webview"
        engineView.currentWebView.webChromeClient.onShowCustomView(view, customViewCallback)

        assertNotEquals(View.GONE, engineView.currentWebView.visibility)
    }

    @Test
    fun `removeFullScreenView with no matching views`() {
        val engineSession = SystemEngineSession()
        val engineView = SystemEngineView(RuntimeEnvironment.application)
        engineView.render(engineSession)

        val view = View(RuntimeEnvironment.systemContext)
        val customViewCallback = mock(WebChromeClient.CustomViewCallback::class.java)

        // When the fullscreen view isn't available
        engineView.currentWebView.webChromeClient.onShowCustomView(view, customViewCallback)
        engineView.findViewWithTag<View>("mosac_system_engine_fullscreen").tag = "not_fullscreen"

        engineView.currentWebView.webChromeClient.onHideCustomView()

        assertNotNull(engineView.fullScreenCallback)
        verify(engineView.fullScreenCallback, never())?.onCustomViewHidden()
        assertEquals(View.GONE, engineView.currentWebView.visibility)

        // When fullscreen view is available, but WebView isn't.
        engineView.findViewWithTag<View>("not_fullscreen").tag = "mosac_system_engine_fullscreen"
        engineView.currentWebView.tag = "not_webView"

        engineView.currentWebView.webChromeClient.onHideCustomView()

        assertEquals(View.GONE, engineView.currentWebView.visibility)
    }

    @Test
    fun `fullscreenCallback is null`() {
        val engineSession = SystemEngineSession()
        val engineView = SystemEngineView(RuntimeEnvironment.application)
        engineView.render(engineSession)

        engineView.currentWebView.webChromeClient.onHideCustomView()
        assertNull(engineView.fullScreenCallback)
    }
}