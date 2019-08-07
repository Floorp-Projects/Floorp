/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

package mozilla.components.browser.engine.gecko

import android.os.Handler
import android.os.Message
import androidx.test.ext.junit.runners.AndroidJUnit4
import kotlinx.coroutines.ExperimentalCoroutinesApi
import kotlinx.coroutines.test.runBlockingTest
import mozilla.components.browser.errorpages.ErrorType
import mozilla.components.concept.engine.DefaultSettings
import mozilla.components.concept.engine.EngineSession
import mozilla.components.concept.engine.EngineSession.LoadUrlFlags
import mozilla.components.concept.engine.EngineSession.TrackingProtectionPolicy
import mozilla.components.concept.engine.HitResult
import mozilla.components.concept.engine.UnsupportedSettingException
import mozilla.components.concept.engine.content.blocking.Tracker
import mozilla.components.concept.engine.history.HistoryTrackingDelegate
import mozilla.components.concept.engine.manifest.WebAppManifest
import mozilla.components.concept.engine.permission.PermissionRequest
import mozilla.components.concept.engine.request.RequestInterceptor
import mozilla.components.concept.storage.VisitType
import mozilla.components.support.test.any
import mozilla.components.support.test.eq
import mozilla.components.support.test.expectException
import mozilla.components.support.test.mock
import mozilla.components.support.test.whenever
import mozilla.components.support.utils.ThreadUtils
import mozilla.components.test.ReflectionUtils
import org.json.JSONObject
import org.junit.Assert.assertEquals
import org.junit.Assert.assertFalse
import org.junit.Assert.assertNotNull
import org.junit.Assert.assertNull
import org.junit.Assert.assertTrue
import org.junit.Before
import org.junit.Test
import org.junit.runner.RunWith
import org.mockito.ArgumentCaptor
import org.mockito.ArgumentMatchers.anyBoolean
import org.mockito.ArgumentMatchers.anyInt
import org.mockito.ArgumentMatchers.anyList
import org.mockito.ArgumentMatchers.anyString
import org.mockito.Mockito.never
import org.mockito.Mockito.times
import org.mockito.Mockito.verify
import org.mockito.Mockito.verifyZeroInteractions
import org.mozilla.geckoview.AllowOrDeny
import org.mozilla.geckoview.ContentBlocking
import org.mozilla.geckoview.GeckoResult
import org.mozilla.geckoview.GeckoRuntime
import org.mozilla.geckoview.GeckoSession
import org.mozilla.geckoview.GeckoSession.ContentDelegate.ContextElement.TYPE_AUDIO
import org.mozilla.geckoview.GeckoSession.ContentDelegate.ContextElement.TYPE_IMAGE
import org.mozilla.geckoview.GeckoSession.ContentDelegate.ContextElement.TYPE_NONE
import org.mozilla.geckoview.GeckoSession.ContentDelegate.ContextElement.TYPE_VIDEO
import org.mozilla.geckoview.GeckoSession.ProgressDelegate.SecurityInformation
import org.mozilla.geckoview.GeckoSessionSettings
import org.mozilla.geckoview.MockWebResponseInfo
import org.mozilla.geckoview.SessionFinder
import org.mozilla.geckoview.WebRequestError
import org.mozilla.geckoview.WebRequestError.ERROR_CATEGORY_UNKNOWN
import org.mozilla.geckoview.WebRequestError.ERROR_MALFORMED_URI
import org.mozilla.geckoview.WebRequestError.ERROR_UNKNOWN

@ExperimentalCoroutinesApi
@RunWith(AndroidJUnit4::class)
class GeckoEngineSessionTest {

    private lateinit var geckoSession: GeckoSession
    private lateinit var geckoSessionProvider: () -> GeckoSession

    private lateinit var navigationDelegate: ArgumentCaptor<GeckoSession.NavigationDelegate>
    private lateinit var progressDelegate: ArgumentCaptor<GeckoSession.ProgressDelegate>
    private lateinit var contentDelegate: ArgumentCaptor<GeckoSession.ContentDelegate>
    private lateinit var permissionDelegate: ArgumentCaptor<GeckoSession.PermissionDelegate>
    private lateinit var contentBlockingDelegate: ArgumentCaptor<ContentBlocking.Delegate>
    private lateinit var historyDelegate: ArgumentCaptor<GeckoSession.HistoryDelegate>

    @Before
    fun setup() {
        ThreadUtils.setHandlerForTest(object : Handler() {
            override fun sendMessageAtTime(msg: Message?, uptimeMillis: Long): Boolean {
                val wrappedRunnable = Runnable {
                    try {
                        msg?.callback?.run()
                    } catch (t: Throwable) {
                        // We ignore this in the test as the runnable could be calling
                        // a native method (disposeNative) which won't work in Robolectric
                    }
                }
                return super.sendMessageAtTime(Message.obtain(this, wrappedRunnable), uptimeMillis)
            }
        })

        navigationDelegate = ArgumentCaptor.forClass(GeckoSession.NavigationDelegate::class.java)
        progressDelegate = ArgumentCaptor.forClass(GeckoSession.ProgressDelegate::class.java)
        contentDelegate = ArgumentCaptor.forClass(GeckoSession.ContentDelegate::class.java)
        permissionDelegate = ArgumentCaptor.forClass(GeckoSession.PermissionDelegate::class.java)
        contentBlockingDelegate = ArgumentCaptor.forClass(ContentBlocking.Delegate::class.java)
        historyDelegate = ArgumentCaptor.forClass(GeckoSession.HistoryDelegate::class.java)

        geckoSession = mockGeckoSession()
        geckoSessionProvider = { geckoSession }
    }

    private fun captureDelegates() {
        verify(geckoSession).navigationDelegate = navigationDelegate.capture()
        verify(geckoSession).progressDelegate = progressDelegate.capture()
        verify(geckoSession).contentDelegate = contentDelegate.capture()
        verify(geckoSession).permissionDelegate = permissionDelegate.capture()
        verify(geckoSession).contentBlockingDelegate = contentBlockingDelegate.capture()
        verify(geckoSession).historyDelegate = historyDelegate.capture()
    }

    @Test
    fun engineSessionInitialization() {
        val runtime = mock<GeckoRuntime>()
        GeckoEngineSession(runtime, geckoSessionProvider = geckoSessionProvider)

        verify(geckoSession).open(any())

        captureDelegates()

        assertNotNull(navigationDelegate.value)
        assertNotNull(progressDelegate.value)
    }

    @Test
    fun progressDelegateNotifiesObservers() {
        val engineSession = GeckoEngineSession(mock(),
                geckoSessionProvider = geckoSessionProvider)

        var observedProgress = 0
        var observedLoadingState = false
        var observedSecurityChange = false
        engineSession.register(object : EngineSession.Observer {
            override fun onLoadingStateChange(loading: Boolean) { observedLoadingState = loading }
            override fun onProgress(progress: Int) { observedProgress = progress }
            override fun onSecurityChange(secure: Boolean, host: String?, issuer: String?) {
                // We cannot assert on actual parameters as SecurityInfo's fields can't be set
                // from the outside and its constructor isn't accessible either.
                observedSecurityChange = true
            }
        })

        captureDelegates()

        progressDelegate.value.onPageStart(mock(), "http://mozilla.org")
        assertEquals(GeckoEngineSession.PROGRESS_START, observedProgress)
        assertEquals(true, observedLoadingState)

        progressDelegate.value.onPageStop(mock(), true)
        assertEquals(GeckoEngineSession.PROGRESS_STOP, observedProgress)
        assertEquals(false, observedLoadingState)

        // Stop will update the loading state and progress observers even when
        // we haven't completed been successful.
        progressDelegate.value.onPageStart(mock(), "http://mozilla.org")
        assertEquals(GeckoEngineSession.PROGRESS_START, observedProgress)
        assertEquals(true, observedLoadingState)

        progressDelegate.value.onPageStop(mock(), false)
        assertEquals(GeckoEngineSession.PROGRESS_STOP, observedProgress)
        assertEquals(false, observedLoadingState)

        val securityInfo = mock<SecurityInformation>()
        progressDelegate.value.onSecurityChange(mock(), securityInfo)
        assertTrue(observedSecurityChange)

        observedSecurityChange = false

        progressDelegate.value.onSecurityChange(mock(), mock())
        assertTrue(observedSecurityChange)
    }

    @Test
    fun navigationDelegateNotifiesObservers() {
        val engineSession = GeckoEngineSession(mock(),
                geckoSessionProvider = geckoSessionProvider)

        var observedUrl = ""
        var observedCanGoBack = false
        var observedCanGoForward = false
        engineSession.register(object : EngineSession.Observer {
            override fun onLocationChange(url: String) { observedUrl = url }
            override fun onNavigationStateChange(canGoBack: Boolean?, canGoForward: Boolean?) {
                canGoBack?.let { observedCanGoBack = canGoBack }
                canGoForward?.let { observedCanGoForward = canGoForward }
            }
        })

        captureDelegates()

        navigationDelegate.value.onLocationChange(mock(), "http://mozilla.org")
        assertEquals("http://mozilla.org", observedUrl)

        navigationDelegate.value.onCanGoBack(mock(), true)
        assertEquals(true, observedCanGoBack)

        navigationDelegate.value.onCanGoForward(mock(), true)
        assertEquals(true, observedCanGoForward)
    }

    @Test
    fun contentDelegateNotifiesObserverAboutDownloads() {
        val engineSession = GeckoEngineSession(mock(),
                geckoSessionProvider = geckoSessionProvider)

        val observer: EngineSession.Observer = mock()
        engineSession.register(observer)

        val info: GeckoSession.WebResponseInfo = MockWebResponseInfo(
            uri = "https://download.mozilla.org",
            contentLength = 42,
            contentType = "image/png",
            filename = "image.png"
        )

        captureDelegates()
        contentDelegate.value.onExternalResponse(mock(), info)

        verify(observer).onExternalResource(
            url = "https://download.mozilla.org",
            fileName = "image.png",
            contentLength = 42,
            contentType = "image/png",
            userAgent = null,
            cookie = null)
    }

    @Test
    fun contentDelegateNotifiesObserverAboutWebAppManifest() {
        val engineSession = GeckoEngineSession(mock(),
            geckoSessionProvider = geckoSessionProvider)

        val observer: EngineSession.Observer = mock()
        engineSession.register(observer)

        val json = JSONObject().apply {
            put("name", "Minimal")
            put("start_url", "/")
        }
        val manifest = WebAppManifest(
            name = "Minimal",
            startUrl = "/"
        )

        captureDelegates()
        contentDelegate.value.onWebAppManifest(mock(), json)

        verify(observer).onWebAppManifestLoaded(manifest)
    }

    @Test
    fun permissionDelegateNotifiesObservers() {
        val engineSession = GeckoEngineSession(mock(),
                geckoSessionProvider = geckoSessionProvider)

        val observedContentPermissionRequests: MutableList<PermissionRequest> = mutableListOf()
        val observedAppPermissionRequests: MutableList<PermissionRequest> = mutableListOf()
        engineSession.register(object : EngineSession.Observer {
            override fun onContentPermissionRequest(permissionRequest: PermissionRequest) {
                observedContentPermissionRequests.add(permissionRequest)
            }

            override fun onAppPermissionRequest(permissionRequest: PermissionRequest) {
                observedAppPermissionRequests.add(permissionRequest)
            }
        })

        captureDelegates()

        permissionDelegate.value.onContentPermissionRequest(
            geckoSession,
            "originContent",
            GeckoSession.PermissionDelegate.PERMISSION_GEOLOCATION,
            mock()
        )

        permissionDelegate.value.onContentPermissionRequest(
            geckoSession,
            null,
            GeckoSession.PermissionDelegate.PERMISSION_GEOLOCATION,
            mock()
        )

        permissionDelegate.value.onMediaPermissionRequest(
            geckoSession,
            "originMedia",
            emptyArray(),
            emptyArray(),
            mock()
        )

        permissionDelegate.value.onMediaPermissionRequest(
            geckoSession,
            "about:blank",
            null,
            null,
            mock()
        )

        permissionDelegate.value.onAndroidPermissionsRequest(
            geckoSession,
            emptyArray(),
            mock()
        )

        permissionDelegate.value.onAndroidPermissionsRequest(
            geckoSession,
            null,
            mock()
        )

        assertEquals(4, observedContentPermissionRequests.size)
        assertEquals("originContent", observedContentPermissionRequests[0].uri)
        assertEquals("", observedContentPermissionRequests[1].uri)
        assertEquals("originMedia", observedContentPermissionRequests[2].uri)
        assertEquals("about:blank", observedContentPermissionRequests[3].uri)
        assertEquals(2, observedAppPermissionRequests.size)
    }

    @Test
    fun loadUrl() {
        val engineSession = GeckoEngineSession(mock(),
                geckoSessionProvider = geckoSessionProvider)

        engineSession.loadUrl("http://mozilla.org")
        verify(geckoSession).loadUri("http://mozilla.org", GeckoSession.LOAD_FLAGS_NONE)

        engineSession.loadUrl("http://www.mozilla.org", LoadUrlFlags.select(LoadUrlFlags.EXTERNAL))
        verify(geckoSession).loadUri("http://www.mozilla.org", GeckoSession.LOAD_FLAGS_EXTERNAL)
    }

    @Test
    fun loadData() {
        val engineSession = GeckoEngineSession(mock(),
                geckoSessionProvider = geckoSessionProvider)

        engineSession.loadData("<html><body>Hello!</body></html>")
        verify(geckoSession).loadString(any(), eq("text/html"))

        engineSession.loadData("Hello!", "text/plain", "UTF-8")
        verify(geckoSession).loadString(any(), eq("text/plain"))

        engineSession.loadData("ahr0cdovl21vemlsbgeub3jn==", "text/plain", "base64")
        verify(geckoSession).loadData(any(), eq("text/plain"))

        engineSession.loadData("ahr0cdovl21vemlsbgeub3jn==", encoding = "base64")
        verify(geckoSession).loadData(any(), eq("text/html"))
    }

    @Test
    fun loadDataBase64() {
        val engineSession = GeckoEngineSession(mock(),
                geckoSessionProvider = geckoSessionProvider)

        engineSession.loadData("Hello!", "text/plain", "UTF-8")
        verify(geckoSession).loadString(eq("Hello!"), anyString())

        engineSession.loadData("ahr0cdovl21vemlsbgeub3jn==", "text/plain", "base64")
        verify(geckoSession).loadData(eq("ahr0cdovl21vemlsbgeub3jn==".toByteArray()), eq("text/plain"))

        engineSession.loadData("ahr0cdovl21vemlsbgeub3jn==", encoding = "base64")
        verify(geckoSession).loadData(eq("ahr0cdovl21vemlsbgeub3jn==".toByteArray()), eq("text/plain"))
    }

    @Test
    fun stopLoading() {
        val engineSession = GeckoEngineSession(mock(),
                geckoSessionProvider = geckoSessionProvider)

        engineSession.stopLoading()

        verify(geckoSession).stop()
    }

    @Test
    fun reload() {
        val engineSession = GeckoEngineSession(mock(),
                geckoSessionProvider = geckoSessionProvider)
        engineSession.loadUrl("http://mozilla.org")

        engineSession.reload()

        verify(geckoSession).reload()
    }

    @Test
    fun goBack() {
        val engineSession = GeckoEngineSession(mock(),
                geckoSessionProvider = geckoSessionProvider)

        engineSession.goBack()

        verify(geckoSession).goBack()
    }

    @Test
    fun goForward() {
        val engineSession = GeckoEngineSession(mock(),
                geckoSessionProvider = geckoSessionProvider)

        engineSession.goForward()

        verify(geckoSession).goForward()
    }

    @Test
    fun restoreState() {
        val engineSession = GeckoEngineSession(mock(),
                geckoSessionProvider = geckoSessionProvider)

        val actualState: GeckoSession.SessionState = mock()
        val state = GeckoEngineSessionState(actualState)

        engineSession.restoreState(state)
        verify(geckoSession).restoreState(any())
    }

    @Test
    fun `restoreState does nothing for null state`() {
        val engineSession = GeckoEngineSession(mock(),
            geckoSessionProvider = geckoSessionProvider)

        val state = GeckoEngineSessionState(null)

        engineSession.restoreState(state)
        verify(geckoSession, never()).restoreState(any())
    }

    class MockSecurityInformation(origin: String) : SecurityInformation() {
        init {
            ReflectionUtils.setField(this, "origin", origin)
        }
    }

    @Test
    fun progressDelegateIgnoresInitialLoadOfAboutBlank() {
        val engineSession = GeckoEngineSession(mock(),
                geckoSessionProvider = geckoSessionProvider)

        var observedSecurityChange = false
        engineSession.register(object : EngineSession.Observer {
            override fun onSecurityChange(secure: Boolean, host: String?, issuer: String?) {
                observedSecurityChange = true
            }
        })

        captureDelegates()

        progressDelegate.value.onSecurityChange(mock(),
                MockSecurityInformation("moz-nullprincipal:{uuid}"))
        assertFalse(observedSecurityChange)

        progressDelegate.value.onSecurityChange(mock(),
                MockSecurityInformation("https://www.mozilla.org"))
        assertTrue(observedSecurityChange)
    }

    @Test
    fun navigationDelegateIgnoresInitialLoadOfAboutBlank() {
        val engineSession = GeckoEngineSession(mock(),
                geckoSessionProvider = geckoSessionProvider)

        var observedUrl = ""
        engineSession.register(object : EngineSession.Observer {
            override fun onLocationChange(url: String) { observedUrl = url }
        })

        captureDelegates()

        navigationDelegate.value.onLocationChange(mock(), "about:blank")
        assertEquals("", observedUrl)

        navigationDelegate.value.onLocationChange(mock(), "about:blank")
        assertEquals("", observedUrl)

        navigationDelegate.value.onLocationChange(mock(), "https://www.mozilla.org")
        assertEquals("https://www.mozilla.org", observedUrl)

        navigationDelegate.value.onLocationChange(mock(), "about:blank")
        assertEquals("about:blank", observedUrl)
    }

    @Test
    fun `keeps track of current url via onPageStart events`() {
        val engineSession = GeckoEngineSession(mock(),
                geckoSessionProvider = geckoSessionProvider)

        captureDelegates()

        assertNull(engineSession.currentUrl)
        progressDelegate.value.onPageStart(geckoSession, "https://www.mozilla.org")
        assertEquals("https://www.mozilla.org", engineSession.currentUrl)

        progressDelegate.value.onPageStart(geckoSession, "https://www.firefox.com")
        assertEquals("https://www.firefox.com", engineSession.currentUrl)
    }

    @Test
    fun `notifies configured history delegate of title changes`() = runBlockingTest {
        val engineSession = GeckoEngineSession(mock(),
                geckoSessionProvider = geckoSessionProvider,
            context = coroutineContext)
        val historyTrackingDelegate: HistoryTrackingDelegate = mock()

        captureDelegates()

        // Nothing breaks if history delegate isn't configured.
        contentDelegate.value.onTitleChange(geckoSession, "Hello World!")

        engineSession.settings.historyTrackingDelegate = historyTrackingDelegate

        contentDelegate.value.onTitleChange(geckoSession, "Hello World!")
        verify(historyTrackingDelegate, never()).onTitleChanged(anyString(), anyString())

        // This sets the currentUrl.
        progressDelegate.value.onPageStart(geckoSession, "https://www.mozilla.com")

        contentDelegate.value.onTitleChange(geckoSession, "Hello World!")
        verify(historyTrackingDelegate).onTitleChanged(eq("https://www.mozilla.com"), eq("Hello World!"))
    }

    @Test
    fun `does not notify configured history delegate of title changes for private sessions`() = runBlockingTest {
        val engineSession = GeckoEngineSession(mock(),
                geckoSessionProvider = geckoSessionProvider,
            context = coroutineContext,
                privateMode = true)
        val historyTrackingDelegate: HistoryTrackingDelegate = mock()

        captureDelegates()

        // Nothing breaks if history delegate isn't configured.
        contentDelegate.value.onTitleChange(geckoSession, "Hello World!")

        engineSession.settings.historyTrackingDelegate = historyTrackingDelegate

        val observer: EngineSession.Observer = mock()
        engineSession.register(observer)

        contentDelegate.value.onTitleChange(geckoSession, "Hello World!")
        verify(historyTrackingDelegate, never()).onTitleChanged(anyString(), anyString())
        verify(observer).onTitleChange("Hello World!")

        // This sets the currentUrl.
        progressDelegate.value.onPageStart(geckoSession, "https://www.mozilla.com")

        contentDelegate.value.onTitleChange(geckoSession, "Mozilla")
        verify(historyTrackingDelegate, never()).onTitleChanged(anyString(), anyString())
        verify(observer).onTitleChange("Mozilla")
    }

    @Test
    fun `does not notify configured history delegate for redirects`() = runBlockingTest {
        val engineSession = GeckoEngineSession(mock(),
                geckoSessionProvider = geckoSessionProvider,
            context = coroutineContext)
        val historyTrackingDelegate: HistoryTrackingDelegate = mock()

        captureDelegates()

        // Nothing breaks if history delegate isn't configured.
        historyDelegate.value.onVisited(geckoSession, "https://www.mozilla.com", null, GeckoSession.HistoryDelegate.VISIT_TOP_LEVEL)
            engineSession.job.children.forEach { it.join() }

        engineSession.settings.historyTrackingDelegate = historyTrackingDelegate

        historyDelegate.value.onVisited(geckoSession, "https://www.mozilla.com", null, GeckoSession.HistoryDelegate.VISIT_REDIRECT_TEMPORARY)
            engineSession.job.children.forEach { it.join() }
            verify(historyTrackingDelegate, never()).onVisited(anyString(), any())
    }

    @Test
    fun `does not notify configured history delegate for top-level visits to error pages`() = runBlockingTest {
        val engineSession = GeckoEngineSession(mock(),
                geckoSessionProvider = geckoSessionProvider,
            context = coroutineContext)
        val historyTrackingDelegate: HistoryTrackingDelegate = mock()

        captureDelegates()

        engineSession.settings.historyTrackingDelegate = historyTrackingDelegate

        historyDelegate.value.onVisited(geckoSession, "about:neterror", null, GeckoSession.HistoryDelegate.VISIT_TOP_LEVEL or GeckoSession.HistoryDelegate.VISIT_UNRECOVERABLE_ERROR)
            engineSession.job.children.forEach { it.join() }
            verify(historyTrackingDelegate, never()).onVisited(anyString(), any())
    }

    @Test
    fun `notifies configured history delegate of visits`() = runBlockingTest {
        val engineSession = GeckoEngineSession(mock(),
                geckoSessionProvider = geckoSessionProvider,
            context = coroutineContext)
        val historyTrackingDelegate: HistoryTrackingDelegate = mock()

        captureDelegates()

        engineSession.settings.historyTrackingDelegate = historyTrackingDelegate
        whenever(historyTrackingDelegate.shouldStoreUri("https://www.mozilla.com")).thenReturn(true)

        historyDelegate.value.onVisited(geckoSession, "https://www.mozilla.com", null, GeckoSession.HistoryDelegate.VISIT_TOP_LEVEL)
            engineSession.job.children.forEach { it.join() }
            verify(historyTrackingDelegate).onVisited(eq("https://www.mozilla.com"), eq(VisitType.LINK))
    }

    @Test
    fun `notifies configured history delegate of reloads`() = runBlockingTest {
        val engineSession = GeckoEngineSession(mock(),
                geckoSessionProvider = geckoSessionProvider,
            context = coroutineContext)
        val historyTrackingDelegate: HistoryTrackingDelegate = mock()

        captureDelegates()

        engineSession.settings.historyTrackingDelegate = historyTrackingDelegate
        whenever(historyTrackingDelegate.shouldStoreUri("https://www.mozilla.com")).thenReturn(true)

        historyDelegate.value.onVisited(geckoSession, "https://www.mozilla.com", "https://www.mozilla.com", GeckoSession.HistoryDelegate.VISIT_TOP_LEVEL)
            engineSession.job.children.forEach { it.join() }
            verify(historyTrackingDelegate).onVisited(eq("https://www.mozilla.com"), eq(VisitType.RELOAD))
    }

    @Test
    fun `checks with the delegate before trying to record a visit`() = runBlockingTest {
        val engineSession = GeckoEngineSession(mock(),
                geckoSessionProvider = geckoSessionProvider,
            context = coroutineContext)
        val historyTrackingDelegate: HistoryTrackingDelegate = mock()

        captureDelegates()

        engineSession.settings.historyTrackingDelegate = historyTrackingDelegate
        whenever(historyTrackingDelegate.shouldStoreUri("https://www.mozilla.com/allowed")).thenReturn(true)
        whenever(historyTrackingDelegate.shouldStoreUri("https://www.mozilla.com/not-allowed")).thenReturn(false)

        historyDelegate.value.onVisited(geckoSession, "https://www.mozilla.com/allowed", null, GeckoSession.HistoryDelegate.VISIT_TOP_LEVEL)

            engineSession.job.children.forEach { it.join() }
            verify(historyTrackingDelegate).shouldStoreUri("https://www.mozilla.com/allowed")
            verify(historyTrackingDelegate).onVisited(eq("https://www.mozilla.com/allowed"), eq(VisitType.LINK))

        historyDelegate.value.onVisited(geckoSession, "https://www.mozilla.com/not-allowed", null, GeckoSession.HistoryDelegate.VISIT_TOP_LEVEL)

            engineSession.job.children.forEach { it.join() }
            verify(historyTrackingDelegate).shouldStoreUri("https://www.mozilla.com/not-allowed")
        verify(historyTrackingDelegate, never()).onVisited(eq("https://www.mozilla.com/not-allowed"), any())
    }

    @Test
    fun `correctly processes redirect visit flags`() = runBlockingTest {
        val engineSession = GeckoEngineSession(mock(),
                geckoSessionProvider = geckoSessionProvider,
            context = coroutineContext)
        val historyTrackingDelegate: HistoryTrackingDelegate = mock()

        captureDelegates()

        engineSession.settings.historyTrackingDelegate = historyTrackingDelegate
        whenever(historyTrackingDelegate.shouldStoreUri(any())).thenReturn(true)

        historyDelegate.value.onVisited(
                geckoSession,
                "https://www.mozilla.com/tempredirect",
                null,
                // bitwise 'or'
                GeckoSession.HistoryDelegate.VISIT_TOP_LEVEL
                        or GeckoSession.HistoryDelegate.VISIT_REDIRECT_SOURCE
        )

            engineSession.job.children.forEach { it.join() }
            verify(historyTrackingDelegate).onVisited(eq("https://www.mozilla.com/tempredirect"), eq(VisitType.REDIRECT_TEMPORARY))

        historyDelegate.value.onVisited(
                geckoSession,
                "https://www.mozilla.com/permredirect",
                null,
                GeckoSession.HistoryDelegate.VISIT_TOP_LEVEL
                        or GeckoSession.HistoryDelegate.VISIT_REDIRECT_SOURCE_PERMANENT
        )

            engineSession.job.children.forEach { it.join() }
            verify(historyTrackingDelegate).onVisited(eq("https://www.mozilla.com/permredirect"), eq(VisitType.REDIRECT_PERMANENT))

        // Visits below are targets of redirects, not redirects themselves.
        // Check that they're mapped to "link".
        historyDelegate.value.onVisited(
                geckoSession,
                "https://www.mozilla.com/targettemp",
                null,
                GeckoSession.HistoryDelegate.VISIT_TOP_LEVEL
                        or GeckoSession.HistoryDelegate.VISIT_REDIRECT_TEMPORARY
        )

            engineSession.job.children.forEach { it.join() }
            verify(historyTrackingDelegate).onVisited(eq("https://www.mozilla.com/targettemp"), eq(VisitType.LINK))

        historyDelegate.value.onVisited(
                geckoSession,
                "https://www.mozilla.com/targetperm",
                null,
                GeckoSession.HistoryDelegate.VISIT_TOP_LEVEL
                        or GeckoSession.HistoryDelegate.VISIT_REDIRECT_TEMPORARY
        )

            engineSession.job.children.forEach { it.join() }
            verify(historyTrackingDelegate).onVisited(eq("https://www.mozilla.com/targetperm"), eq(VisitType.LINK))
    }

    @Test
    fun `does not notify configured history delegate of visits for private sessions`() = runBlockingTest {
        val engineSession = GeckoEngineSession(mock(),
                geckoSessionProvider = geckoSessionProvider,
            context = coroutineContext,
                privateMode = true)
        val historyTrackingDelegate: HistoryTrackingDelegate = mock()

        captureDelegates()

        engineSession.settings.historyTrackingDelegate = historyTrackingDelegate

        historyDelegate.value.onVisited(geckoSession, "https://www.mozilla.com", "https://www.mozilla.com", GeckoSession.HistoryDelegate.VISIT_TOP_LEVEL)
            engineSession.job.children.forEach { it.join() }
            verify(historyTrackingDelegate, never()).onVisited(anyString(), any())
    }

    @Test
    fun `requests visited URLs from configured history delegate`() = runBlockingTest {
        val engineSession = GeckoEngineSession(mock(),
                geckoSessionProvider = geckoSessionProvider,
            context = coroutineContext)
        val historyTrackingDelegate: HistoryTrackingDelegate = mock()

        captureDelegates()

        // Nothing breaks if history delegate isn't configured.
        historyDelegate.value.getVisited(geckoSession, arrayOf("https://www.mozilla.com", "https://www.mozilla.org"))
            engineSession.job.children.forEach { it.join() }

        engineSession.settings.historyTrackingDelegate = historyTrackingDelegate

        historyDelegate.value.getVisited(geckoSession, arrayOf("https://www.mozilla.com", "https://www.mozilla.org"))
            engineSession.job.children.forEach { it.join() }
            verify(historyTrackingDelegate).getVisited(eq(listOf("https://www.mozilla.com", "https://www.mozilla.org")))
    }

    @Test
    fun `does not request visited URLs from configured history delegate in private sessions`() = runBlockingTest {
        val engineSession = GeckoEngineSession(mock(),
                geckoSessionProvider = geckoSessionProvider,
            context = coroutineContext,
                privateMode = true)
        val historyTrackingDelegate: HistoryTrackingDelegate = mock()

        captureDelegates()

        engineSession.settings.historyTrackingDelegate = historyTrackingDelegate

        historyDelegate.value.getVisited(geckoSession, arrayOf("https://www.mozilla.com", "https://www.mozilla.org"))
            engineSession.job.children.forEach { it.join() }
            verify(historyTrackingDelegate, never()).getVisited(anyList())
    }

    @Test
    fun websiteTitleUpdates() {
        val engineSession = GeckoEngineSession(mock(),
                geckoSessionProvider = geckoSessionProvider)

        val observer: EngineSession.Observer = mock()
        engineSession.register(observer)

        captureDelegates()

        contentDelegate.value.onTitleChange(geckoSession, "Hello World!")

        verify(observer).onTitleChange("Hello World!")
    }

    @Test
    fun trackingProtectionDelegateNotifiesObservers() {
        val engineSession = GeckoEngineSession(mock(),
                geckoSessionProvider = geckoSessionProvider)

        var trackerBlocked: Tracker? = null
        engineSession.register(object : EngineSession.Observer {
            override fun onTrackerBlocked(tracker: Tracker) {
                trackerBlocked = tracker
            }
        })

        captureDelegates()

        var geckoCatgories = 0
        geckoCatgories = geckoCatgories.or(ContentBlocking.AT_AD)
        geckoCatgories = geckoCatgories.or(ContentBlocking.AT_ANALYTIC)
        geckoCatgories = geckoCatgories.or(ContentBlocking.AT_SOCIAL)
        geckoCatgories = geckoCatgories.or(ContentBlocking.AT_CRYPTOMINING)
        geckoCatgories = geckoCatgories.or(ContentBlocking.AT_FINGERPRINTING)
        geckoCatgories = geckoCatgories.or(ContentBlocking.AT_CONTENT)

        contentBlockingDelegate.value.onContentBlocked(geckoSession,
            ContentBlocking.BlockEvent("tracker1", geckoCatgories)
        )
        assertEquals("tracker1", trackerBlocked!!.url)
        assertTrue(trackerBlocked!!.categories.containsAll(Tracker.Category.values().toList()))
    }

    @Test
    fun enableTrackingProtection() {
        val runtime = mock<GeckoRuntime>()
        whenever(runtime.settings).thenReturn(mock())
        val session = GeckoEngineSession(runtime, geckoSessionProvider = geckoSessionProvider)
        val privSession = GeckoEngineSession(runtime, geckoSessionProvider = geckoSessionProvider, privateMode = true)

        var trackerBlockingObserved = false
        session.register(object : EngineSession.Observer {
            override fun onTrackerBlockingEnabledChange(enabled: Boolean) {
                trackerBlockingObserved = enabled
            }
        })
        var privateTrackerBlockingObserved = false
        privSession.register(object : EngineSession.Observer {
            override fun onTrackerBlockingEnabledChange(enabled: Boolean) {
                privateTrackerBlockingObserved = enabled
            }
        })

        val allPolicy = TrackingProtectionPolicy.select(TrackingProtectionPolicy.AD)
        val regularOnlyPolicy = TrackingProtectionPolicy.select(TrackingProtectionPolicy.AD).forRegularSessionsOnly()
        val privateOnlyPolicy = TrackingProtectionPolicy.select(TrackingProtectionPolicy.AD).forPrivateSessionsOnly()

        session.enableTrackingProtection(allPolicy)
        assertTrue(trackerBlockingObserved)

        session.enableTrackingProtection(privateOnlyPolicy)
        assertFalse(trackerBlockingObserved)

        session.enableTrackingProtection(regularOnlyPolicy)
        assertTrue(trackerBlockingObserved)

        privSession.enableTrackingProtection(allPolicy)
        assertTrue(privateTrackerBlockingObserved)

        privSession.enableTrackingProtection(regularOnlyPolicy)
        assertFalse(privateTrackerBlockingObserved)

        privSession.enableTrackingProtection(privateOnlyPolicy)
        assertTrue(privateTrackerBlockingObserved)
    }

    @Test
    fun disableTrackingProtection() {
        val runtime = mock<GeckoRuntime>()
        whenever(runtime.settings).thenReturn(mock())
        val engineSession = GeckoEngineSession(runtime, geckoSessionProvider = geckoSessionProvider)

        var trackerBlockingDisabledObserved = false
        engineSession.register(object : EngineSession.Observer {
            override fun onTrackerBlockingEnabledChange(enabled: Boolean) {
                trackerBlockingDisabledObserved = !enabled
            }
        })

        engineSession.disableTrackingProtection()
        assertTrue(trackerBlockingDisabledObserved)
    }

    @Test
    fun trackingProtectionCategoriesAreAligned() {
        assertEquals(TrackingProtectionPolicy.AD, ContentBlocking.AT_AD)
        assertEquals(TrackingProtectionPolicy.ANALYTICS, ContentBlocking.AT_ANALYTIC)
        assertEquals(TrackingProtectionPolicy.CONTENT, ContentBlocking.AT_CONTENT)
        assertEquals(TrackingProtectionPolicy.SOCIAL, ContentBlocking.AT_SOCIAL)
        assertEquals(TrackingProtectionPolicy.TEST, ContentBlocking.AT_TEST)
        assertEquals(TrackingProtectionPolicy.CRYPTOMINING, ContentBlocking.AT_CRYPTOMINING)
        assertEquals(TrackingProtectionPolicy.FINGERPRINTING, ContentBlocking.AT_FINGERPRINTING)
        assertEquals(TrackingProtectionPolicy.SAFE_BROWSING_ALL, ContentBlocking.SB_ALL)
        assertEquals(TrackingProtectionPolicy.SAFE_BROWSING_HARMFUL, ContentBlocking.SB_HARMFUL)
        assertEquals(TrackingProtectionPolicy.SAFE_BROWSING_MALWARE, ContentBlocking.SB_MALWARE)
        assertEquals(TrackingProtectionPolicy.SAFE_BROWSING_PHISHING, ContentBlocking.SB_PHISHING)
        assertEquals(TrackingProtectionPolicy.SAFE_BROWSING_UNWANTED, ContentBlocking.SB_UNWANTED)

        assertEquals(TrackingProtectionPolicy.all().categories, ContentBlocking.CB_STRICT)
        assertEquals(TrackingProtectionPolicy.recommended().categories, ContentBlocking.CB_DEFAULT)
        assertEquals(TrackingProtectionPolicy.CookiePolicy.ACCEPT_ALL.id, ContentBlocking.COOKIE_ACCEPT_ALL)
        assertEquals(
            TrackingProtectionPolicy.CookiePolicy.ACCEPT_NON_TRACKERS.id,
            ContentBlocking.COOKIE_ACCEPT_NON_TRACKERS
        )
        assertEquals(TrackingProtectionPolicy.CookiePolicy.ACCEPT_NONE.id, ContentBlocking.COOKIE_ACCEPT_NONE)
        assertEquals(
            TrackingProtectionPolicy.CookiePolicy.ACCEPT_ONLY_FIRST_PARTY.id,
            ContentBlocking.COOKIE_ACCEPT_FIRST_PARTY
        )
        assertEquals(TrackingProtectionPolicy.CookiePolicy.ACCEPT_VISITED.id, ContentBlocking.COOKIE_ACCEPT_VISITED)
    }

    @Test
    fun settingTestingMode() {
        val runtime = mock<GeckoRuntime>()
        whenever(runtime.settings).thenReturn(mock())

        GeckoEngineSession(runtime,
                geckoSessionProvider = geckoSessionProvider,
                defaultSettings = DefaultSettings())
        verify(geckoSession.settings).fullAccessibilityTree = false

        GeckoEngineSession(runtime,
            geckoSessionProvider = geckoSessionProvider,
            defaultSettings = DefaultSettings(testingModeEnabled = true))
        verify(geckoSession.settings).fullAccessibilityTree = true
    }

    @Test
    fun settingUserAgent() {
        val runtime = mock<GeckoRuntime>()
        whenever(runtime.settings).thenReturn(mock())

        val engineSession = GeckoEngineSession(runtime, geckoSessionProvider = geckoSessionProvider)
        engineSession.settings.userAgentString

        verify(geckoSession.settings).userAgentOverride

        engineSession.settings.userAgentString = "test-ua"

        verify(geckoSession.settings).userAgentOverride = "test-ua"
    }

    @Test
    fun settingUserAgentDefault() {
        val runtime = mock<GeckoRuntime>()
        whenever(runtime.settings).thenReturn(mock())

        GeckoEngineSession(runtime,
                geckoSessionProvider = geckoSessionProvider,
                defaultSettings = DefaultSettings(userAgentString = "test-ua"))

        verify(geckoSession.settings).userAgentOverride = "test-ua"
    }

    @Test
    fun settingSuspendMediaWhenInactive() {
        val runtime = mock<GeckoRuntime>()
        whenever(runtime.settings).thenReturn(mock())

        val engineSession = GeckoEngineSession(runtime, geckoSessionProvider = geckoSessionProvider)
        verify(geckoSession.settings, never()).suspendMediaWhenInactive = anyBoolean()

        assertFalse(engineSession.settings.suspendMediaWhenInactive)
        verify(geckoSession.settings).suspendMediaWhenInactive

        engineSession.settings.suspendMediaWhenInactive = true
        verify(geckoSession.settings).suspendMediaWhenInactive = true
    }

    @Test
    fun settingSuspendMediaWhenInactiveDefault() {
        val runtime = mock<GeckoRuntime>()
        whenever(runtime.settings).thenReturn(mock())

        GeckoEngineSession(runtime, geckoSessionProvider = geckoSessionProvider)
        verify(geckoSession.settings, never()).suspendMediaWhenInactive = anyBoolean()

        GeckoEngineSession(runtime,
            geckoSessionProvider = geckoSessionProvider,
            defaultSettings = DefaultSettings())
        verify(geckoSession.settings).suspendMediaWhenInactive = false

        GeckoEngineSession(runtime,
            geckoSessionProvider = geckoSessionProvider,
            defaultSettings = DefaultSettings(suspendMediaWhenInactive = true))
        verify(geckoSession.settings).suspendMediaWhenInactive = true
    }

    @Test
    fun unsupportedSettings() {
        val settings = GeckoEngineSession(mock(),
                geckoSessionProvider = geckoSessionProvider).settings

        expectException(UnsupportedSettingException::class) {
            settings.javascriptEnabled = true
        }

        expectException(UnsupportedSettingException::class) {
            settings.domStorageEnabled = false
        }

        expectException(UnsupportedSettingException::class) {
            settings.trackingProtectionPolicy = TrackingProtectionPolicy.all()
        }
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

        GeckoEngineSession(mock(), geckoSessionProvider = geckoSessionProvider,
                defaultSettings = defaultSettings)

        captureDelegates()

        navigationDelegate.value.onLoadRequest(geckoSession, mockLoadRequest("sample:about"))

        assertEquals("sample:about", interceptorCalledWithUri)
        verify(geckoSession).loadString("<h1>Hello World</h1>", "text/html")
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

        GeckoEngineSession(mock(), geckoSessionProvider = geckoSessionProvider,
                defaultSettings = defaultSettings)

        captureDelegates()

        navigationDelegate.value.onLoadRequest(geckoSession, mockLoadRequest("sample:about"))

        assertEquals("sample:about", interceptorCalledWithUri)
        verify(geckoSession).loadUri("https://mozilla.org", GeckoSession.LOAD_FLAGS_NONE)
    }

    @Test
    fun onLoadRequestWithoutInterceptor() {
        val defaultSettings = DefaultSettings()

        GeckoEngineSession(mock(), geckoSessionProvider = geckoSessionProvider,
                defaultSettings = defaultSettings)

        captureDelegates()

        navigationDelegate.value.onLoadRequest(geckoSession, mockLoadRequest("sample:about"))

        verify(geckoSession, never()).loadString(anyString(), anyString())
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

        GeckoEngineSession(mock(), geckoSessionProvider = geckoSessionProvider,
                defaultSettings = defaultSettings)

        captureDelegates()

        navigationDelegate.value.onLoadRequest(geckoSession, mockLoadRequest("sample:about"))

        assertEquals("sample:about", interceptorCalledWithUri!!)
        verify(geckoSession, never()).loadString(anyString(), anyString())
    }

    @Test
    fun onLoadErrorCallsInterceptorWithNull() {
        var interceptedUri: String? = null
        val requestInterceptor: RequestInterceptor = mock()
        var defaultSettings = DefaultSettings()
        var engineSession = GeckoEngineSession(mock(), geckoSessionProvider = geckoSessionProvider,
                defaultSettings = defaultSettings)

        captureDelegates()

        // Interceptor is not called when there is none attached.
        var onLoadError = navigationDelegate.value.onLoadError(
            geckoSession,
            "",
            WebRequestError(
                ERROR_CATEGORY_UNKNOWN,
                ERROR_UNKNOWN)
        )
        verify(requestInterceptor, never()).onErrorRequest(engineSession, ErrorType.UNKNOWN, "")
        onLoadError!!.then { value: String? ->
            interceptedUri = value
            GeckoResult.fromValue(null)
        }
        assertNull(interceptedUri)

        // Interceptor is called correctly
        defaultSettings = DefaultSettings(requestInterceptor = requestInterceptor)
        geckoSession = mockGeckoSession()
        engineSession = GeckoEngineSession(mock(), geckoSessionProvider = geckoSessionProvider,
                defaultSettings = defaultSettings)

        captureDelegates()

        onLoadError = navigationDelegate.value.onLoadError(
            geckoSession,
            "",
            WebRequestError(
                ERROR_CATEGORY_UNKNOWN,
                ERROR_UNKNOWN)
        )

        verify(requestInterceptor).onErrorRequest(engineSession, ErrorType.UNKNOWN, "")
        onLoadError!!.then { value: String? ->
            interceptedUri = value
            GeckoResult.fromValue(null)
        }
        assertNull(interceptedUri)
    }

    @Test
    fun onLoadErrorCallsInterceptorWithErrorPage() {
        val requestInterceptor: RequestInterceptor = object : RequestInterceptor {
            override fun onErrorRequest(
                session: EngineSession,
                errorType: ErrorType,
                uri: String?
            ): RequestInterceptor.ErrorResponse? =
                RequestInterceptor.ErrorResponse("nonNullData")
        }

        val defaultSettings = DefaultSettings(requestInterceptor = requestInterceptor)
        GeckoEngineSession(mock(), geckoSessionProvider = geckoSessionProvider,
                defaultSettings = defaultSettings)

        captureDelegates()

        val onLoadError = navigationDelegate.value.onLoadError(
            geckoSession,
            "about:failed",
            WebRequestError(
                ERROR_CATEGORY_UNKNOWN,
                ERROR_UNKNOWN)
        )
        onLoadError!!.then { value: String? ->
            assertTrue(value!!.contains("data:text/html;base64,"))
            GeckoResult.fromValue(null)
        }
    }

    @Test
    fun onLoadErrorCallsInterceptorWithInvalidUri() {
        val requestInterceptor: RequestInterceptor = mock()
        val defaultSettings = DefaultSettings(requestInterceptor = requestInterceptor)
        val engineSession = GeckoEngineSession(mock(), defaultSettings = defaultSettings)

        engineSession.geckoSession.navigationDelegate!!.onLoadError(
            engineSession.geckoSession,
            null,
            WebRequestError(ERROR_MALFORMED_URI, ERROR_CATEGORY_UNKNOWN)
        )
        verify(requestInterceptor).onErrorRequest(engineSession, ErrorType.ERROR_MALFORMED_URI, null)
    }

    @Test
    fun geckoErrorMappingToErrorType() {
        assertEquals(
            ErrorType.ERROR_SECURITY_SSL,
            GeckoEngineSession.geckoErrorToErrorType(WebRequestError.ERROR_SECURITY_SSL)
        )
        assertEquals(
            ErrorType.ERROR_SECURITY_BAD_CERT,
            GeckoEngineSession.geckoErrorToErrorType(WebRequestError.ERROR_SECURITY_BAD_CERT)
        )
        assertEquals(
            ErrorType.ERROR_NET_INTERRUPT,
            GeckoEngineSession.geckoErrorToErrorType(WebRequestError.ERROR_NET_INTERRUPT)
        )
        assertEquals(
            ErrorType.ERROR_NET_TIMEOUT,
            GeckoEngineSession.geckoErrorToErrorType(WebRequestError.ERROR_NET_TIMEOUT)
        )
        assertEquals(
            ErrorType.ERROR_NET_RESET,
            GeckoEngineSession.geckoErrorToErrorType(WebRequestError.ERROR_NET_RESET)
        )
        assertEquals(
            ErrorType.ERROR_CONNECTION_REFUSED,
            GeckoEngineSession.geckoErrorToErrorType(WebRequestError.ERROR_CONNECTION_REFUSED)
        )
        assertEquals(
            ErrorType.ERROR_UNKNOWN_SOCKET_TYPE,
            GeckoEngineSession.geckoErrorToErrorType(WebRequestError.ERROR_UNKNOWN_SOCKET_TYPE)
        )
        assertEquals(
            ErrorType.ERROR_REDIRECT_LOOP,
            GeckoEngineSession.geckoErrorToErrorType(WebRequestError.ERROR_REDIRECT_LOOP)
        )
        assertEquals(
            ErrorType.ERROR_OFFLINE,
            GeckoEngineSession.geckoErrorToErrorType(WebRequestError.ERROR_OFFLINE)
        )
        assertEquals(
            ErrorType.ERROR_PORT_BLOCKED,
            GeckoEngineSession.geckoErrorToErrorType(WebRequestError.ERROR_PORT_BLOCKED)
        )
        assertEquals(
            ErrorType.ERROR_UNSAFE_CONTENT_TYPE,
            GeckoEngineSession.geckoErrorToErrorType(WebRequestError.ERROR_UNSAFE_CONTENT_TYPE)
        )
        assertEquals(
            ErrorType.ERROR_CORRUPTED_CONTENT,
            GeckoEngineSession.geckoErrorToErrorType(WebRequestError.ERROR_CORRUPTED_CONTENT)
        )
        assertEquals(
            ErrorType.ERROR_CONTENT_CRASHED,
            GeckoEngineSession.geckoErrorToErrorType(WebRequestError.ERROR_CONTENT_CRASHED)
        )
        assertEquals(
            ErrorType.ERROR_INVALID_CONTENT_ENCODING,
            GeckoEngineSession.geckoErrorToErrorType(WebRequestError.ERROR_INVALID_CONTENT_ENCODING)
        )
        assertEquals(
            ErrorType.ERROR_UNKNOWN_HOST,
            GeckoEngineSession.geckoErrorToErrorType(WebRequestError.ERROR_UNKNOWN_HOST)
        )
        assertEquals(
            ErrorType.ERROR_MALFORMED_URI,
            GeckoEngineSession.geckoErrorToErrorType(ERROR_MALFORMED_URI)
        )
        assertEquals(
            ErrorType.ERROR_UNKNOWN_PROTOCOL,
            GeckoEngineSession.geckoErrorToErrorType(WebRequestError.ERROR_UNKNOWN_PROTOCOL)
        )
        assertEquals(
            ErrorType.ERROR_FILE_NOT_FOUND,
            GeckoEngineSession.geckoErrorToErrorType(WebRequestError.ERROR_FILE_NOT_FOUND)
        )
        assertEquals(
            ErrorType.ERROR_FILE_ACCESS_DENIED,
            GeckoEngineSession.geckoErrorToErrorType(WebRequestError.ERROR_FILE_ACCESS_DENIED)
        )
        assertEquals(
            ErrorType.ERROR_PROXY_CONNECTION_REFUSED,
            GeckoEngineSession.geckoErrorToErrorType(WebRequestError.ERROR_PROXY_CONNECTION_REFUSED)
        )
        assertEquals(
            ErrorType.ERROR_UNKNOWN_PROXY_HOST,
            GeckoEngineSession.geckoErrorToErrorType(WebRequestError.ERROR_UNKNOWN_PROXY_HOST)
        )
        assertEquals(
            ErrorType.ERROR_SAFEBROWSING_MALWARE_URI,
            GeckoEngineSession.geckoErrorToErrorType(WebRequestError.ERROR_SAFEBROWSING_MALWARE_URI)
        )
        assertEquals(
            ErrorType.ERROR_SAFEBROWSING_HARMFUL_URI,
            GeckoEngineSession.geckoErrorToErrorType(WebRequestError.ERROR_SAFEBROWSING_HARMFUL_URI)
        )
        assertEquals(
            ErrorType.ERROR_SAFEBROWSING_PHISHING_URI,
            GeckoEngineSession.geckoErrorToErrorType(WebRequestError.ERROR_SAFEBROWSING_PHISHING_URI)
        )
        assertEquals(
            ErrorType.ERROR_SAFEBROWSING_UNWANTED_URI,
            GeckoEngineSession.geckoErrorToErrorType(WebRequestError.ERROR_SAFEBROWSING_UNWANTED_URI)
        )
        assertEquals(
            ErrorType.UNKNOWN,
            GeckoEngineSession.geckoErrorToErrorType(-500)
        )
    }

    @Test
    fun defaultSettings() {
        val runtime = mock<GeckoRuntime>()
        whenever(runtime.settings).thenReturn(mock())

        val defaultSettings = DefaultSettings(trackingProtectionPolicy = TrackingProtectionPolicy.all())

        GeckoEngineSession(runtime, geckoSessionProvider = geckoSessionProvider,
                privateMode = false, defaultSettings = defaultSettings)

        assertFalse(geckoSession.settings.usePrivateMode)
        verify(geckoSession.settings).useTrackingProtection = true
    }

    @Test
    fun contentDelegate() {
        val engineSession = GeckoEngineSession(mock(),
                geckoSessionProvider = geckoSessionProvider)
        val delegate = engineSession.createContentDelegate()

        var observedChanged = false
        engineSession.register(object : EngineSession.Observer {
            override fun onLongPress(hitResult: HitResult) {
                observedChanged = true
            }
        })

        class MockContextElement(
            baseUri: String?,
            linkUri: String?,
            title: String?,
            altText: String?,
            typeStr: String,
            srcUri: String?
        ) : GeckoSession.ContentDelegate.ContextElement(baseUri, linkUri, title, altText, typeStr, srcUri)

        delegate.onContextMenu(geckoSession, 0, 0,
            MockContextElement(null, null, "title", "alt", "HTMLAudioElement", "file.mp3"))
        assertTrue(observedChanged)

        observedChanged = false
        delegate.onContextMenu(geckoSession, 0, 0,
            MockContextElement(null, null, "title", "alt", "HTMLAudioElement", null))
        assertFalse(observedChanged)

        observedChanged = false
        delegate.onContextMenu(geckoSession, 0, 0,
            MockContextElement(null, null, "title", "alt", "foobar", null))
        assertFalse(observedChanged)
    }

    @Test
    fun handleLongClick() {
        val engineSession = GeckoEngineSession(mock(),
                geckoSessionProvider = geckoSessionProvider)

        var result = engineSession.handleLongClick("file.mp3", TYPE_AUDIO)
        assertNotNull(result)
        assertTrue(result is HitResult.AUDIO && result.src == "file.mp3")

        result = engineSession.handleLongClick("file.mp4", TYPE_VIDEO)
        assertNotNull(result)
        assertTrue(result is HitResult.VIDEO && result.src == "file.mp4")

        result = engineSession.handleLongClick("file.png", TYPE_IMAGE)
        assertNotNull(result)
        assertTrue(result is HitResult.IMAGE && result.src == "file.png")

        result = engineSession.handleLongClick("file.png", TYPE_IMAGE, "https://mozilla.org")
        assertNotNull(result)
        assertTrue(result is HitResult.IMAGE_SRC && result.src == "file.png" && result.uri == "https://mozilla.org")

        result = engineSession.handleLongClick(null, TYPE_IMAGE)
        assertNotNull(result)
        assertTrue(result is HitResult.UNKNOWN && result.src == "")

        result = engineSession.handleLongClick("tel:+1234567890", TYPE_NONE)
        assertNotNull(result)
        assertTrue(result is HitResult.PHONE && result.src == "tel:+1234567890")

        result = engineSession.handleLongClick("geo:1,-1", TYPE_NONE)
        assertNotNull(result)
        assertTrue(result is HitResult.GEO && result.src == "geo:1,-1")

        result = engineSession.handleLongClick("mailto:asa@mozilla.com", TYPE_NONE)
        assertNotNull(result)
        assertTrue(result is HitResult.EMAIL && result.src == "mailto:asa@mozilla.com")

        result = engineSession.handleLongClick(null, TYPE_NONE, "https://mozilla.org")
        assertNotNull(result)
        assertTrue(result is HitResult.UNKNOWN && result.src == "https://mozilla.org")

        result = engineSession.handleLongClick("data://foobar", TYPE_NONE, "https://mozilla.org")
        assertNotNull(result)
        assertTrue(result is HitResult.UNKNOWN && result.src == "data://foobar")

        result = engineSession.handleLongClick(null, TYPE_NONE, null)
        assertNull(result)
    }

    @Test
    fun setDesktopMode() {
        val runtime = mock<GeckoRuntime>()
        val engineSession = GeckoEngineSession(runtime, geckoSessionProvider = geckoSessionProvider)

        var desktopModeToggled = false
        engineSession.register(object : EngineSession.Observer {
            override fun onDesktopModeChange(enabled: Boolean) {
                desktopModeToggled = true
            }
        })
        engineSession.toggleDesktopMode(true)
        assertTrue(desktopModeToggled)

        desktopModeToggled = false
        whenever(geckoSession.settings.userAgentMode)
                .thenReturn(GeckoSessionSettings.USER_AGENT_MODE_DESKTOP)
        whenever(geckoSession.settings.viewportMode)
                .thenReturn(GeckoSessionSettings.VIEWPORT_MODE_DESKTOP)

        engineSession.toggleDesktopMode(true)
        assertFalse(desktopModeToggled)

        engineSession.toggleDesktopMode(true)
        assertFalse(desktopModeToggled)

        engineSession.toggleDesktopMode(false)
        assertTrue(desktopModeToggled)
    }

    @Test
    fun findAll() {
        val finderResult = mock<GeckoSession.FinderResult>()
        val sessionFinder = mock<SessionFinder>()
        whenever(sessionFinder.find("mozilla", 0))
                .thenReturn(GeckoResult.fromValue(finderResult))

        whenever(geckoSession.finder).thenReturn(sessionFinder)

        val engineSession = GeckoEngineSession(mock(),
                geckoSessionProvider = geckoSessionProvider)

        var findObserved: String? = null
        var findResultObserved = false
        engineSession.register(object : EngineSession.Observer {
            override fun onFind(text: String) {
                findObserved = text
            }

            override fun onFindResult(activeMatchOrdinal: Int, numberOfMatches: Int, isDoneCounting: Boolean) {
                assertEquals(0, activeMatchOrdinal)
                assertEquals(0, numberOfMatches)
                assertTrue(isDoneCounting)
                findResultObserved = true
            }
        })

        engineSession.findAll("mozilla")

        assertEquals("mozilla", findObserved)
        assertTrue(findResultObserved)
        verify(sessionFinder).find("mozilla", 0)
    }

    @Test
    fun findNext() {
        val finderResult = mock<GeckoSession.FinderResult>()
        val sessionFinder = mock<SessionFinder>()
        whenever(sessionFinder.find(eq(null), anyInt()))
                .thenReturn(GeckoResult.fromValue(finderResult))

        whenever(geckoSession.finder).thenReturn(sessionFinder)

        val engineSession = GeckoEngineSession(mock(),
                geckoSessionProvider = geckoSessionProvider)

        var findResultObserved = false
        engineSession.register(object : EngineSession.Observer {
            override fun onFindResult(activeMatchOrdinal: Int, numberOfMatches: Int, isDoneCounting: Boolean) {
                assertEquals(0, activeMatchOrdinal)
                assertEquals(0, numberOfMatches)
                assertTrue(isDoneCounting)
                findResultObserved = true
            }
        })

        engineSession.findNext(true)
        assertTrue(findResultObserved)
        verify(sessionFinder).find(null, 0)

        engineSession.findNext(false)
        assertTrue(findResultObserved)
        verify(sessionFinder).find(null, GeckoSession.FINDER_FIND_BACKWARDS)
    }

    @Test
    fun clearFindMatches() {
        val finder = mock<SessionFinder>()
        whenever(geckoSession.finder).thenReturn(finder)

        val engineSession = GeckoEngineSession(mock(),
                geckoSessionProvider = geckoSessionProvider)

        engineSession.clearFindMatches()

        verify(finder).clear()
    }

    @Test
    fun exitFullScreenModeTriggersExitEvent() {
        val engineSession = GeckoEngineSession(mock(),
                geckoSessionProvider = geckoSessionProvider)
        val observer: EngineSession.Observer = mock()

        // Verify the event is triggered for exiting fullscreen mode and GeckoView is called.
        engineSession.exitFullScreenMode()
        verify(geckoSession).exitFullScreen()

        // Verify the call to the observer.
        engineSession.register(observer)

        captureDelegates()

        contentDelegate.value.onFullScreen(geckoSession, true)

        verify(observer).onFullScreenChange(true)
    }

    @Test
    fun exitFullscreenTrueHasNoInteraction() {
        val engineSession = GeckoEngineSession(mock(),
                geckoSessionProvider = geckoSessionProvider)

        engineSession.exitFullScreenMode()
        verify(geckoSession).exitFullScreen()
    }

    @Test
    fun clearData() {
        val runtime = mock<GeckoRuntime>()
        val engineSession = GeckoEngineSession(runtime, geckoSessionProvider = geckoSessionProvider)
        val observer: EngineSession.Observer = mock()

        engineSession.register(observer)

        engineSession.clearData()

        verifyZeroInteractions(observer)
    }

    @Test
    fun `after onCrash get called geckoSession must be reset`() {
        val runtime = mock<GeckoRuntime>()
        val engineSession = GeckoEngineSession(runtime)
        val oldGeckoSession = engineSession.geckoSession

        assertTrue(engineSession.geckoSession.isOpen)

        oldGeckoSession.contentDelegate!!.onCrash(mock())

        assertFalse(oldGeckoSession.isOpen)
        assertTrue(engineSession.geckoSession != oldGeckoSession)
    }

    @Test
    fun `Closing engine session should close underlying gecko session`() {
        val geckoSession = mockGeckoSession()

        val engineSession = GeckoEngineSession(mock(), geckoSessionProvider = { geckoSession })

        engineSession.close()

        verify(geckoSession).close()
    }

    @Test
    fun `Handle new window load requests`() {
        GeckoEngineSession(mock(), geckoSessionProvider = geckoSessionProvider)
        captureDelegates()

        val result = navigationDelegate.value.onLoadRequest(geckoSession,
                mockLoadRequest("sample:about", GeckoSession.NavigationDelegate.TARGET_WINDOW_NEW))

        assertNotNull(result)
        assertEquals(result!!.poll(0), AllowOrDeny.DENY)
        verify(geckoSession).loadUri("sample:about")
    }

    @Test
    fun `State provided through delegate will be returned from saveState`() {
        val engineSession = GeckoEngineSession(mock(),
            geckoSessionProvider = geckoSessionProvider)

        captureDelegates()

        val state: GeckoSession.SessionState = mock()

        progressDelegate.value.onSessionStateChange(mock(), state)

        val savedState = engineSession.saveState()
        assertNotNull(savedState)
        assertTrue(savedState is GeckoEngineSessionState)

        val actualState = (savedState as GeckoEngineSessionState).actualState
        assertEquals(state, actualState)
    }

    @Test
    fun `onCrash notifies observers about crash`() {
        val engineSession = GeckoEngineSession(mock(),
            geckoSessionProvider = geckoSessionProvider)

        captureDelegates()

        var crashedState = false

        engineSession.register(object : EngineSession.Observer {
            override fun onCrash() {
                crashedState = true
            }
        })

        contentDelegate.value.onCrash(mock())

        assertEquals(true, crashedState)
    }

    @Test
    fun `recoverFromCrash does not restore state if no state has been saved previously`() {
        val engineSession = GeckoEngineSession(mock(),
            geckoSessionProvider = geckoSessionProvider)

        assertFalse(engineSession.recoverFromCrash())
        verify(engineSession.geckoSession, never()).restoreState(any())
    }

    @Test
    fun `recoverFromCrash restores last known state`() {
        val engineSession = GeckoEngineSession(mock(),
            geckoSessionProvider = geckoSessionProvider)

        captureDelegates()

        val state1: GeckoSession.SessionState = mock()
        val state2: GeckoSession.SessionState = mock()

        progressDelegate.value.onSessionStateChange(engineSession.geckoSession, state1)
        progressDelegate.value.onSessionStateChange(engineSession.geckoSession, state2)

        contentDelegate.value.onCrash(engineSession.geckoSession)

        assertTrue(engineSession.recoverFromCrash())

        verify(engineSession.geckoSession).restoreState(state2)
    }

    @Test
    fun `recoverFromCrash does not restore last known state if no crash occurred`() {
        val engineSession = GeckoEngineSession(mock(),
            geckoSessionProvider = geckoSessionProvider)

        captureDelegates()

        val state: GeckoSession.SessionState = mock()

        progressDelegate.value.onSessionStateChange(engineSession.geckoSession, state)

        assertFalse(engineSession.recoverFromCrash())

        verify(engineSession.geckoSession, never()).restoreState(state)
        verify(engineSession.geckoSession, never()).restoreState(any())
    }

    @Test
    fun `onLoadRequest will notify observers if request was not intercepted`() {
        val engineSession = GeckoEngineSession(mock(),
            geckoSessionProvider = geckoSessionProvider)

        captureDelegates()

        var observedUrl: String? = null
        var observedTriggeredByRedirect: Boolean? = null

        engineSession.register(object : EngineSession.Observer {
            override fun onLoadRequest(url: String, triggeredByRedirect: Boolean, triggeredByWebContent: Boolean) {
                observedTriggeredByRedirect = triggeredByRedirect
                observedUrl = url
            }
        })

        navigationDelegate.value.onLoadRequest(
            mock(), mockLoadRequest("sample:about", triggeredByRedirect = true))

        assertNotNull(observedTriggeredByRedirect)
        assertTrue(observedTriggeredByRedirect!!)
        assertEquals("sample:about", observedUrl)

        navigationDelegate.value.onLoadRequest(
            mock(), mockLoadRequest("sample:about", triggeredByRedirect = false))

        assertFalse(observedTriggeredByRedirect!!)
        assertEquals("sample:about", observedUrl)
    }

    @Test
    fun `onLoadRequest will notify observers if the url is loaded from the user interacting with chrome`() {
        val engineSession = GeckoEngineSession(mock(),
            geckoSessionProvider = geckoSessionProvider)

        captureDelegates()

        val fakeUrl = "https://example.com"
        var observedUrl: String?
        var observedTriggeredByWebContent: Boolean?

        engineSession.register(object : EngineSession.Observer {
            override fun onLoadRequest(url: String, triggeredByRedirect: Boolean, triggeredByWebContent: Boolean) {
                observedTriggeredByWebContent = triggeredByWebContent
                observedUrl = url
            }
        })

        fun fakePageLoad(expectedTriggeredByWebContent: Boolean) {
            observedTriggeredByWebContent = null
            observedUrl = null
            navigationDelegate.value.onLoadRequest(
                mock(), mockLoadRequest(fakeUrl, triggeredByRedirect = true))
            progressDelegate.value.onPageStop(mock(), true)
            assertNotNull(observedTriggeredByWebContent)
            assertEquals(expectedTriggeredByWebContent, observedTriggeredByWebContent!!)
            assertNotNull(observedUrl)
            assertEquals(fakeUrl, observedUrl)
        }

        // loadUrl(url: String)
        engineSession.loadUrl(fakeUrl)
        verify(geckoSession).loadUri(fakeUrl, GeckoSession.LOAD_FLAGS_NONE)
        fakePageLoad(false)

        // subsequent page loads _are_ from web content
        fakePageLoad(true)

        // loadData(data: String, mimeType: String, encoding: String)
        val fakeData = "data://"
        val fakeMimeType = ""
        val fakeEncoding = ""
        engineSession.loadData(data = fakeData, mimeType = fakeMimeType, encoding = fakeEncoding)
        verify(geckoSession).loadString(fakeData, fakeMimeType)
        fakePageLoad(false)

        fakePageLoad(true)

        // reload()
        engineSession.reload()
        verify(geckoSession).reload()
        fakePageLoad(false)

        fakePageLoad(true)

        // goBack()
        engineSession.goBack()
        verify(geckoSession).goBack()
        fakePageLoad(false)

        fakePageLoad(true)

        // goForward()
        engineSession.goForward()
        verify(geckoSession).goForward()
        fakePageLoad(false)

        fakePageLoad(true)

        // toggleDesktopMode()
        engineSession.toggleDesktopMode(false, reload = true)
        // This is the second time in this test, so we actually want two invocations.
        verify(geckoSession, times(2)).reload()
        fakePageLoad(false)

        fakePageLoad(true)
    }

    @Test
    fun `onLoadRequest will not notify observers if request was intercepted`() {
        val defaultSettings = DefaultSettings(requestInterceptor = object : RequestInterceptor {
            override fun onLoadRequest(session: EngineSession, uri: String): RequestInterceptor.InterceptionResponse? {
                return RequestInterceptor.InterceptionResponse.Content("<h1>Hello World</h1>")
            }
        })

        val engineSession = GeckoEngineSession(
            mock(), geckoSessionProvider = geckoSessionProvider, defaultSettings = defaultSettings)

        captureDelegates()

        val observer: EngineSession.Observer = mock()
        engineSession.register(observer)

        val fakeUri = "sample:about"
        navigationDelegate.value.onLoadRequest(
            mock(), mockLoadRequest(fakeUri, triggeredByRedirect = true))

        verify(observer, never()).onLoadRequest(eq(fakeUri), anyBoolean(), anyBoolean())
    }

    @Test
    fun loadFlagsAreAligned() {
        assertEquals(LoadUrlFlags.BYPASS_CACHE, GeckoSession.LOAD_FLAGS_BYPASS_CACHE)
        assertEquals(LoadUrlFlags.BYPASS_PROXY, GeckoSession.LOAD_FLAGS_BYPASS_PROXY)
        assertEquals(LoadUrlFlags.EXTERNAL, GeckoSession.LOAD_FLAGS_EXTERNAL)
        assertEquals(LoadUrlFlags.ALLOW_POPUPS, GeckoSession.LOAD_FLAGS_ALLOW_POPUPS)
        assertEquals(LoadUrlFlags.BYPASS_CLASSIFIER, GeckoSession.LOAD_FLAGS_BYPASS_CLASSIFIER)
    }

    @Test
    fun `onKill will recover, restore state and notify observers`() {
        val engineSession = GeckoEngineSession(mock(),
                geckoSessionProvider = geckoSessionProvider)

        captureDelegates()

        var observerNotified = false

        engineSession.register(object : EngineSession.Observer {
            override fun onProcessKilled() {
                observerNotified = true
            }
        })

        val mockedState: GeckoSession.SessionState = mock()
        progressDelegate.value.onSessionStateChange(geckoSession, mockedState)

        verify(geckoSession, never()).restoreState(mockedState)

        contentDelegate.value.onKill(geckoSession)

        verify(geckoSession).restoreState(mockedState)
        assertTrue(observerNotified)
    }

    private fun mockGeckoSession(): GeckoSession {
        val session = mock<GeckoSession>()
        whenever(session.settings).thenReturn(
            mock())
        return session
    }

    private fun mockLoadRequest(
        uri: String,
        target: Int = 0,
        triggeredByRedirect: Boolean = false
    ): GeckoSession.NavigationDelegate.LoadRequest {
        var flags = 0
        if (triggeredByRedirect) {
            flags = flags or 0x800000
        }

        val constructor = GeckoSession.NavigationDelegate.LoadRequest::class.java.getDeclaredConstructor(
            String::class.java,
            String::class.java,
            Int::class.java,
            Int::class.java)
        constructor.isAccessible = true

        return constructor.newInstance(uri, uri, target, flags)
    }
}
