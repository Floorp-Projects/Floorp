/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

package mozilla.components.browser.engine.gecko

import android.os.Handler
import android.os.Message
import kotlinx.coroutines.runBlocking
import mozilla.components.browser.errorpages.ErrorType
import mozilla.components.concept.engine.DefaultSettings
import mozilla.components.concept.engine.EngineSession
import mozilla.components.concept.engine.EngineSession.TrackingProtectionPolicy
import mozilla.components.concept.engine.HitResult
import mozilla.components.concept.engine.UnsupportedSettingException
import mozilla.components.concept.engine.history.HistoryTrackingDelegate
import mozilla.components.concept.engine.permission.PermissionRequest
import mozilla.components.concept.engine.request.RequestInterceptor
import mozilla.components.support.test.eq
import mozilla.components.support.test.expectException
import mozilla.components.support.test.mock
import org.junit.Assert
import org.junit.Assert.assertEquals
import org.junit.Assert.assertFalse
import org.junit.Assert.assertNotNull
import org.junit.Assert.assertNull
import org.junit.Assert.assertTrue
import org.junit.Before
import org.junit.Test
import org.junit.runner.RunWith
import org.mockito.ArgumentMatchers.any
import org.mockito.ArgumentMatchers.anyInt
import org.mockito.ArgumentMatchers.anyString
import org.mockito.Mockito.`when`
import org.mockito.Mockito.mock
import org.mockito.Mockito.never
import org.mockito.Mockito.spy
import org.mockito.Mockito.verify
import org.mockito.Mockito.verifyZeroInteractions
import org.mozilla.gecko.util.BundleEventListener
import org.mozilla.gecko.util.GeckoBundle
import org.mozilla.gecko.util.ThreadUtils
import org.mozilla.geckoview.GeckoResult
import org.mozilla.geckoview.GeckoRuntime
import org.mozilla.geckoview.GeckoRuntimeSettings
import org.mozilla.geckoview.GeckoSession
import org.mozilla.geckoview.GeckoSession.ContentDelegate.ELEMENT_TYPE_AUDIO
import org.mozilla.geckoview.GeckoSession.ContentDelegate.ELEMENT_TYPE_IMAGE
import org.mozilla.geckoview.GeckoSession.ContentDelegate.ELEMENT_TYPE_NONE
import org.mozilla.geckoview.GeckoSession.ContentDelegate.ELEMENT_TYPE_VIDEO
import org.mozilla.geckoview.GeckoSession.NavigationDelegate.ERROR_CATEGORY_UNKNOWN
import org.mozilla.geckoview.GeckoSession.NavigationDelegate.ERROR_MALFORMED_URI
import org.mozilla.geckoview.GeckoSession.NavigationDelegate.ERROR_UNKNOWN
import org.mozilla.geckoview.GeckoSession.PermissionDelegate.PERMISSION_GEOLOCATION
import org.mozilla.geckoview.GeckoSession.ProgressDelegate.SecurityInformation
import org.mozilla.geckoview.GeckoSessionSettings
import org.mozilla.geckoview.SessionFinder
import org.mozilla.geckoview.createMockedWebResponseInfo
import org.robolectric.RobolectricTestRunner

@RunWith(RobolectricTestRunner::class)
class GeckoEngineSessionTest {

    @Before
    fun setup() {
        ThreadUtils.sGeckoHandler = null
    }

    @Test
    fun engineSessionInitialization() {
        val runtime = mock(GeckoRuntime::class.java)
        val engineSession = GeckoEngineSession(runtime)

        assertTrue(engineSession.geckoSession.isOpen)
        assertNotNull(engineSession.geckoSession.navigationDelegate)
        assertNotNull(engineSession.geckoSession.progressDelegate)
    }

    @Test
    fun progressDelegateNotifiesObservers() {
        val engineSession = GeckoEngineSession(mock(GeckoRuntime::class.java))

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

        engineSession.geckoSession.progressDelegate.onPageStart(null, "http://mozilla.org")
        assertEquals(GeckoEngineSession.PROGRESS_START, observedProgress)
        assertEquals(true, observedLoadingState)

        engineSession.geckoSession.progressDelegate.onPageStop(null, true)
        assertEquals(GeckoEngineSession.PROGRESS_STOP, observedProgress)
        assertEquals(false, observedLoadingState)

        val securityInfo = mock(GeckoSession.ProgressDelegate.SecurityInformation::class.java)
        engineSession.geckoSession.progressDelegate.onSecurityChange(null, securityInfo)
        assertTrue(observedSecurityChange)

        observedSecurityChange = false

        engineSession.geckoSession.progressDelegate.onSecurityChange(null, null)
        assertTrue(observedSecurityChange)
    }

    @Test
    fun navigationDelegateNotifiesObservers() {
        val engineSession = GeckoEngineSession(mock(GeckoRuntime::class.java))

        var observedUrl = ""
        var observedCanGoBack: Boolean = false
        var observedCanGoForward: Boolean = false
        engineSession.register(object : EngineSession.Observer {
            override fun onLocationChange(url: String) { observedUrl = url }
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
    fun contentDelegateNotifiesObserverAboutDownloads() {
        val engineSession = GeckoEngineSession(mock(GeckoRuntime::class.java))

        val observer: EngineSession.Observer = mock()
        engineSession.register(observer)

        val info: GeckoSession.WebResponseInfo = createMockedWebResponseInfo(
            uri = "https://download.mozilla.org",
            contentLength = 42,
            contentType = "image/png",
            filename = "image.png"
        )

        engineSession.geckoSession.contentDelegate.onExternalResponse(mock(), info)

        verify(observer).onExternalResource(
            url = "https://download.mozilla.org",
            fileName = "image.png",
            contentLength = 42,
            contentType = "image/png",
            userAgent = null,
            cookie = null)
    }

    @Test
    fun permissionDelegateNotifiesObservers() {
        val engineSession = GeckoEngineSession(mock(GeckoRuntime::class.java))

        var observedContentPermissionRequests: MutableList<PermissionRequest> = mutableListOf()
        var observedAppPermissionRequests: MutableList<PermissionRequest> = mutableListOf()
        engineSession.register(object : EngineSession.Observer {
            override fun onContentPermissionRequest(permissionRequest: PermissionRequest) {
                observedContentPermissionRequests.add(permissionRequest)
            }

            override fun onAppPermissionRequest(permissionRequest: PermissionRequest) {
                observedAppPermissionRequests.add(permissionRequest)
            }
        })

        engineSession.geckoSession.permissionDelegate.onContentPermissionRequest(
            engineSession.geckoSession,
            "originContent",
            PERMISSION_GEOLOCATION,
            null,
            mock(GeckoSession.PermissionDelegate.Callback::class.java)
        )

        engineSession.geckoSession.permissionDelegate.onContentPermissionRequest(
            engineSession.geckoSession,
            null,
            PERMISSION_GEOLOCATION,
            null,
            mock(GeckoSession.PermissionDelegate.Callback::class.java)
        )

        engineSession.geckoSession.permissionDelegate.onMediaPermissionRequest(
            engineSession.geckoSession,
            "originMedia",
            emptyArray(),
            emptyArray(),
            mock(GeckoSession.PermissionDelegate.MediaCallback::class.java)
        )

        engineSession.geckoSession.permissionDelegate.onMediaPermissionRequest(
            engineSession.geckoSession,
            null,
            null,
            null,
            mock(GeckoSession.PermissionDelegate.MediaCallback::class.java)
        )

        engineSession.geckoSession.permissionDelegate.onAndroidPermissionsRequest(
            engineSession.geckoSession,
            emptyArray(),
            mock(GeckoSession.PermissionDelegate.Callback::class.java)
        )

        engineSession.geckoSession.permissionDelegate.onAndroidPermissionsRequest(
            engineSession.geckoSession,
            null,
            mock(GeckoSession.PermissionDelegate.Callback::class.java)
        )

        assertEquals(4, observedContentPermissionRequests.size)
        assertEquals("originContent", observedContentPermissionRequests[0].uri)
        assertEquals("", observedContentPermissionRequests[1].uri)
        assertEquals("originMedia", observedContentPermissionRequests[2].uri)
        assertEquals("", observedContentPermissionRequests[3].uri)
        assertEquals(2, observedAppPermissionRequests.size)
    }

    @Test
    fun loadUrl() {
        val engineSession = GeckoEngineSession(mock(GeckoRuntime::class.java))
        var loadUriReceived = false
        engineSession.geckoSession.eventDispatcher.registerUiThreadListener(
                BundleEventListener { _, _, _ -> loadUriReceived = true },
                "GeckoView:LoadUri"
        )

        engineSession.loadUrl("http://mozilla.org")
        assertTrue(loadUriReceived)
    }

    @Test
    fun loadData() {
        val engineSession = GeckoEngineSession(mock(GeckoRuntime::class.java))

        var loadUriReceived = false
        engineSession.geckoSession.eventDispatcher.registerUiThreadListener(
            BundleEventListener { _, _, _ -> loadUriReceived = true },
            "GeckoView:LoadUri"
        )

        engineSession.loadData("<html><body>Hello!</body></html>")
        assertTrue(loadUriReceived)

        loadUriReceived = false
        engineSession.loadData("Hello!", "text/plain", "UTF-8")
        assertTrue(loadUriReceived)

        loadUriReceived = false
        engineSession.loadData("ahr0cdovl21vemlsbgeub3jn==", "text/plain", "base64")
        assertTrue(loadUriReceived)

        loadUriReceived = false
        engineSession.loadData("ahr0cdovl21vemlsbgeub3jn==", encoding = "base64")
        assertTrue(loadUriReceived)
    }

    @Test
    fun loadDataBase64() {
        val engineSession = GeckoEngineSession(mock(GeckoRuntime::class.java))
        val geckoSession = mock(GeckoSession::class.java)

        engineSession.geckoSession = geckoSession

        engineSession.loadData("Hello!", "text/plain", "UTF-8")
        verify(geckoSession).loadString(eq("Hello!"), anyString())

        engineSession.loadData("ahr0cdovl21vemlsbgeub3jn==", "text/plain", "base64")
        verify(geckoSession).loadData(eq("ahr0cdovl21vemlsbgeub3jn==".toByteArray()), eq("text/plain"))

        engineSession.loadData("ahr0cdovl21vemlsbgeub3jn==", encoding = "base64")
        verify(geckoSession).loadData(eq("ahr0cdovl21vemlsbgeub3jn==".toByteArray()), eq("text/plain"))
    }

    @Test
    fun stopLoading() {
        val engineSession = GeckoEngineSession(mock(GeckoRuntime::class.java))
        var stopLoadingReceived = false
        engineSession.geckoSession.eventDispatcher.registerUiThreadListener(
                BundleEventListener { _, _, _ -> stopLoadingReceived = true },
                "GeckoView:Stop"
        )

        engineSession.stopLoading()
        assertTrue(stopLoadingReceived)
    }

    @Test
    fun reload() {
        val engineSession = GeckoEngineSession(mock(GeckoRuntime::class.java))
        engineSession.loadUrl("http://mozilla.org")

        var reloadReceived = false
        engineSession.geckoSession.eventDispatcher.registerUiThreadListener(
                BundleEventListener { _, _, _ -> reloadReceived = true },
                "GeckoView:Reload"
        )

        engineSession.reload()
        assertTrue(reloadReceived)
    }

    @Test
    fun goBack() {
        val engineSession = GeckoEngineSession(mock(GeckoRuntime::class.java))
        var eventReceived = false
        engineSession.geckoSession.eventDispatcher.registerUiThreadListener(
                BundleEventListener { _, _, _ -> eventReceived = true },
                "GeckoView:GoBack"
        )

        engineSession.goBack()
        assertTrue(eventReceived)
    }

    @Test
    fun goForward() {
        val engineSession = GeckoEngineSession(mock(GeckoRuntime::class.java))
        var eventReceived = false
        engineSession.geckoSession.eventDispatcher.registerUiThreadListener(
                BundleEventListener { _, _, _ -> eventReceived = true },
                "GeckoView:GoForward"
        )

        engineSession.goForward()
        assertTrue(eventReceived)
    }

    @Test
    fun saveState() {
        val engineSession = GeckoEngineSession(mock(GeckoRuntime::class.java))
        engineSession.geckoSession = mock(GeckoSession::class.java)
        val currentState = GeckoSession.SessionState("")
        val stateMap = mapOf(GeckoEngineSession.GECKO_STATE_KEY to currentState.toString())

        ThreadUtils.sGeckoHandler = object : Handler() {
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
        }
        `when`(engineSession.geckoSession.saveState()).thenReturn(GeckoResult.fromValue(currentState))
        assertEquals(stateMap, engineSession.saveState())
    }

    @Test
    fun restoreState() {
        val engineSession = GeckoEngineSession(mock(GeckoRuntime::class.java))
        var eventReceived = false
        engineSession.geckoSession.eventDispatcher.registerUiThreadListener(
                BundleEventListener { _, _, _ -> eventReceived = true },
                "GeckoView:RestoreState"
        )

        engineSession.restoreState(mapOf(GeckoEngineSession.GECKO_STATE_KEY to ""))
        assertTrue(eventReceived)
    }

    @Test
    fun progressDelegateIgnoresInitialLoadOfAboutBlank() {
        val engineSession = GeckoEngineSession(mock(GeckoRuntime::class.java))

        var observedSecurityChange = false
        engineSession.register(object : EngineSession.Observer {
            override fun onSecurityChange(secure: Boolean, host: String?, issuer: String?) {
                observedSecurityChange = true
            }
        })

        // We need to make the constructor accessible in order to test this behaviour
        val constructor = SecurityInformation::class.java.getDeclaredConstructor(GeckoBundle::class.java)
        constructor.isAccessible = true

        val bundle = mock(GeckoBundle::class.java)
        `when`(bundle.getBundle(any())).thenReturn(mock(GeckoBundle::class.java))
        `when`(bundle.getString("origin")).thenReturn("moz-nullprincipal:{uuid}")
        engineSession.geckoSession.progressDelegate.onSecurityChange(null, constructor.newInstance(bundle))
        assertFalse(observedSecurityChange)

        `when`(bundle.getBundle(any())).thenReturn(mock(GeckoBundle::class.java))
        `when`(bundle.getString("origin")).thenReturn("https://www.mozilla.org")
        engineSession.geckoSession.progressDelegate.onSecurityChange(null, constructor.newInstance(bundle))
        assertTrue(observedSecurityChange)
    }

    @Test
    fun navigationDelegateIgnoresInitialLoadOfAboutBlank() {
        val engineSession = GeckoEngineSession(mock(GeckoRuntime::class.java))

        var observedUrl = ""
        engineSession.register(object : EngineSession.Observer {
            override fun onLocationChange(url: String) { observedUrl = url }
        })

        engineSession.geckoSession.navigationDelegate.onLocationChange(null, "about:blank")
        assertEquals("", observedUrl)

        engineSession.geckoSession.navigationDelegate.onLocationChange(null, "about:blank")
        assertEquals("", observedUrl)

        engineSession.geckoSession.navigationDelegate.onLocationChange(null, "https://www.mozilla.org")
        assertEquals("https://www.mozilla.org", observedUrl)

        engineSession.geckoSession.navigationDelegate.onLocationChange(null, "about:blank")
        assertEquals("about:blank", observedUrl)
    }

    @Test
    fun `keeps track of current url via onPageStart events`() {
        val engineSession = GeckoEngineSession(mock(GeckoRuntime::class.java))

        assertNull(engineSession.currentUrl)
        engineSession.geckoSession.progressDelegate.onPageStart(engineSession.geckoSession, "https://www.mozilla.org")
        assertEquals("https://www.mozilla.org", engineSession.currentUrl)

        engineSession.geckoSession.progressDelegate.onPageStart(engineSession.geckoSession, "https://www.firefox.com")
        assertEquals("https://www.firefox.com", engineSession.currentUrl)
    }

    @Test
    fun `notifies configured history delegate of title changes`() = runBlocking {
        val engineSession = GeckoEngineSession(mock(GeckoRuntime::class.java))
        val historyTrackingDelegate: HistoryTrackingDelegate = mock()

        // Nothing breaks if history delegate isn't configured.
        engineSession.geckoSession.contentDelegate.onTitleChange(engineSession.geckoSession, "Hello World!")

        engineSession.settings.historyTrackingDelegate = historyTrackingDelegate

        engineSession.geckoSession.contentDelegate.onTitleChange(engineSession.geckoSession, "Hello World!")
        verify(historyTrackingDelegate, never()).onTitleChanged(anyString(), anyString())

        // This sets the currentUrl.
        engineSession.geckoSession.progressDelegate.onPageStart(engineSession.geckoSession, "https://www.mozilla.com")

        engineSession.geckoSession.contentDelegate.onTitleChange(engineSession.geckoSession, "Hello World!")
        verify(historyTrackingDelegate).onTitleChanged(eq("https://www.mozilla.com"), eq("Hello World!"))
    }

    @Test
    fun `notifies configured history delegate of title changes for private sessions`() = runBlocking {
        val engineSession = GeckoEngineSession(mock(GeckoRuntime::class.java), privateMode = true)
        val historyTrackingDelegate: HistoryTrackingDelegate = mock()

        // Nothing breaks if history delegate isn't configured.
        engineSession.geckoSession.contentDelegate.onTitleChange(engineSession.geckoSession, "Hello World!")

        engineSession.settings.historyTrackingDelegate = historyTrackingDelegate

        engineSession.geckoSession.contentDelegate.onTitleChange(engineSession.geckoSession, "Hello World!")
        verify(historyTrackingDelegate, never()).onTitleChanged(anyString(), anyString())

        // This sets the currentUrl.
        engineSession.geckoSession.progressDelegate.onPageStart(engineSession.geckoSession, "https://www.mozilla.com")

        engineSession.geckoSession.contentDelegate.onTitleChange(engineSession.geckoSession, "Hello World!")
        verify(historyTrackingDelegate).onTitleChanged(eq("https://www.mozilla.com"), eq("Hello World!"))
    }

    @Test
    fun websiteTitleUpdates() {
        val engineSession = GeckoEngineSession(mock(GeckoRuntime::class.java))

        val observer: EngineSession.Observer = mock()
        engineSession.register(observer)

        engineSession.geckoSession.contentDelegate.onTitleChange(engineSession.geckoSession, "Hello World!")

        verify(observer).onTitleChange("Hello World!")
    }

    @Test
    fun trackingProtectionDelegateNotifiesObservers() {
        val engineSession = GeckoEngineSession(mock(GeckoRuntime::class.java))

        var trackerBlocked = ""
        engineSession.register(object : EngineSession.Observer {
            override fun onTrackerBlocked(url: String) {
                trackerBlocked = url
            }
        })

        engineSession.geckoSession.trackingProtectionDelegate.onTrackerBlocked(engineSession.geckoSession, "tracker1", 0)
        assertEquals("tracker1", trackerBlocked)
    }

    @Test
    fun enableTrackingProtection() {
        val runtime = mock(GeckoRuntime::class.java)
        `when`(runtime.settings).thenReturn(mock(GeckoRuntimeSettings::class.java))
        val engineSession = GeckoEngineSession(runtime)

        var trackerBlockingEnabledObserved = false
        engineSession.register(object : EngineSession.Observer {
            override fun onTrackerBlockingEnabledChange(enabled: Boolean) {
                trackerBlockingEnabledObserved = enabled
            }
        })

        engineSession.enableTrackingProtection(TrackingProtectionPolicy.select(
                TrackingProtectionPolicy.ANALYTICS, TrackingProtectionPolicy.AD))
        assertTrue(trackerBlockingEnabledObserved)
        assertTrue(engineSession.geckoSession.settings.getBoolean(GeckoSessionSettings.USE_TRACKING_PROTECTION))
    }

    @Test
    fun disableTrackingProtection() {
        val runtime = mock(GeckoRuntime::class.java)
        `when`(runtime.settings).thenReturn(mock(GeckoRuntimeSettings::class.java))
        val engineSession = GeckoEngineSession(runtime)

        var trackerBlockingDisabledObserved = false
        engineSession.register(object : EngineSession.Observer {
            override fun onTrackerBlockingEnabledChange(enabled: Boolean) {
                trackerBlockingDisabledObserved = !enabled
            }
        })

        engineSession.disableTrackingProtection()
        assertTrue(trackerBlockingDisabledObserved)
        assertFalse(engineSession.geckoSession.settings.getBoolean(GeckoSessionSettings.USE_TRACKING_PROTECTION))
    }

    @Test
    fun settingPrivateMode() {
        val runtime = mock(GeckoRuntime::class.java)
        `when`(runtime.settings).thenReturn(mock(GeckoRuntimeSettings::class.java))

        val engineSession = GeckoEngineSession(runtime)
        assertFalse(engineSession.geckoSession.settings.getBoolean(GeckoSessionSettings.USE_PRIVATE_MODE))

        val privateEngineSession = GeckoEngineSession(runtime, true)
        assertTrue(privateEngineSession.geckoSession.settings.getBoolean(GeckoSessionSettings.USE_PRIVATE_MODE))
    }

    @Test
    fun unsupportedSettings() {
        val settings = GeckoEngineSession(mock(GeckoRuntime::class.java)).settings

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

        val engineSession = GeckoEngineSession(mock(), defaultSettings = defaultSettings)
        engineSession.geckoSession = spy(engineSession.geckoSession)

        engineSession.geckoSession.navigationDelegate.onLoadRequest(
            engineSession.geckoSession, mockLoadRequest("sample:about"))

        assertEquals("sample:about", interceptorCalledWithUri)
        verify(engineSession.geckoSession).loadString("<h1>Hello World</h1>", "text/html")
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

        val engineSession = GeckoEngineSession(mock(), defaultSettings = defaultSettings)
        engineSession.geckoSession = spy(engineSession.geckoSession)

        engineSession.geckoSession.navigationDelegate.onLoadRequest(
                engineSession.geckoSession, mockLoadRequest("sample:about"))

        assertEquals("sample:about", interceptorCalledWithUri)
        verify(engineSession.geckoSession).loadUri("https://mozilla.org")
    }

    @Test
    fun onLoadRequestWithoutInterceptor() {
        val defaultSettings = DefaultSettings()

        val engineSession = GeckoEngineSession(mock(), defaultSettings = defaultSettings)
        engineSession.geckoSession = spy(engineSession.geckoSession)

        engineSession.geckoSession.navigationDelegate.onLoadRequest(
            engineSession.geckoSession, mockLoadRequest("sample:about"))

        verify(engineSession.geckoSession, never()).loadString(anyString(), anyString())
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

        val engineSession = GeckoEngineSession(mock(), defaultSettings = defaultSettings)
        engineSession.geckoSession = spy(engineSession.geckoSession)

        engineSession.geckoSession.navigationDelegate.onLoadRequest(
            engineSession.geckoSession, mockLoadRequest("sample:about"))

        assertEquals("sample:about", interceptorCalledWithUri!!)
        verify(engineSession.geckoSession, never()).loadString(anyString(), anyString())
    }

    @Test
    fun onLoadErrorCallsInterceptorWithNull() {
        var interceptedUri: String? = null
        val requestInterceptor: RequestInterceptor = mock()
        var defaultSettings = DefaultSettings()
        var engineSession = GeckoEngineSession(mock(), defaultSettings = defaultSettings)

        // Interceptor is not called when there is none attached.
        var onLoadError = engineSession.geckoSession.navigationDelegate.onLoadError(
            engineSession.geckoSession,
            "",
            ERROR_CATEGORY_UNKNOWN,
            ERROR_UNKNOWN
        )
        verify(requestInterceptor, never()).onErrorRequest(engineSession, ErrorType.UNKNOWN, "")
        onLoadError.then { value: String? ->
            interceptedUri = value
            GeckoResult.fromValue(null)
        }
        assertNull(interceptedUri)

        // Interceptor is called correctly
        defaultSettings = DefaultSettings(requestInterceptor = requestInterceptor)
        engineSession = GeckoEngineSession(mock(), defaultSettings = defaultSettings)
        onLoadError = engineSession.geckoSession.navigationDelegate.onLoadError(
            engineSession.geckoSession,
            "",
            ERROR_CATEGORY_UNKNOWN,
            ERROR_UNKNOWN
        )
        verify(requestInterceptor).onErrorRequest(engineSession, ErrorType.UNKNOWN, "")
        onLoadError.then { value: String? ->
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
        val engineSession = GeckoEngineSession(mock(), defaultSettings = defaultSettings)

        val onLoadError = engineSession.geckoSession.navigationDelegate.onLoadError(
            engineSession.geckoSession,
            "about:failed",
            ERROR_CATEGORY_UNKNOWN,
            ERROR_UNKNOWN
        )
        onLoadError.then { value: String? ->
            assertTrue(value!!.contains("data:text/html;base64,"))
            GeckoResult.fromValue(null)
        }
    }

    @Test
    fun onLoadErrorCallsInterceptorWithInvalidUri() {
        val requestInterceptor: RequestInterceptor = mock()
        val defaultSettings = DefaultSettings(requestInterceptor = requestInterceptor)
        val engineSession = GeckoEngineSession(mock(), defaultSettings = defaultSettings)

        engineSession.geckoSession.navigationDelegate.onLoadError(
            engineSession.geckoSession,
            null,
            ERROR_CATEGORY_UNKNOWN,
            ERROR_MALFORMED_URI
        )
        verify(requestInterceptor).onErrorRequest(engineSession, ErrorType.ERROR_MALFORMED_URI, null)
    }

    @Test
    fun geckoErrorMappingToErrorType() {
        Assert.assertEquals(
            ErrorType.ERROR_SECURITY_SSL,
            GeckoEngineSession.geckoErrorToErrorType(GeckoSession.NavigationDelegate.ERROR_SECURITY_SSL)
        )
        Assert.assertEquals(
            ErrorType.ERROR_SECURITY_BAD_CERT,
            GeckoEngineSession.geckoErrorToErrorType(GeckoSession.NavigationDelegate.ERROR_SECURITY_BAD_CERT)
        )
        Assert.assertEquals(
            ErrorType.ERROR_NET_INTERRUPT,
            GeckoEngineSession.geckoErrorToErrorType(GeckoSession.NavigationDelegate.ERROR_NET_INTERRUPT)
        )
        Assert.assertEquals(
            ErrorType.ERROR_NET_TIMEOUT,
            GeckoEngineSession.geckoErrorToErrorType(GeckoSession.NavigationDelegate.ERROR_NET_TIMEOUT)
        )
        Assert.assertEquals(
            ErrorType.ERROR_NET_RESET,
            GeckoEngineSession.geckoErrorToErrorType(GeckoSession.NavigationDelegate.ERROR_NET_RESET)
        )
        Assert.assertEquals(
            ErrorType.ERROR_CONNECTION_REFUSED,
            GeckoEngineSession.geckoErrorToErrorType(GeckoSession.NavigationDelegate.ERROR_CONNECTION_REFUSED)
        )
        Assert.assertEquals(
            ErrorType.ERROR_UNKNOWN_SOCKET_TYPE,
            GeckoEngineSession.geckoErrorToErrorType(GeckoSession.NavigationDelegate.ERROR_UNKNOWN_SOCKET_TYPE)
        )
        Assert.assertEquals(
            ErrorType.ERROR_REDIRECT_LOOP,
            GeckoEngineSession.geckoErrorToErrorType(GeckoSession.NavigationDelegate.ERROR_REDIRECT_LOOP)
        )
        Assert.assertEquals(
            ErrorType.ERROR_OFFLINE,
            GeckoEngineSession.geckoErrorToErrorType(GeckoSession.NavigationDelegate.ERROR_OFFLINE)
        )
        Assert.assertEquals(
            ErrorType.ERROR_PORT_BLOCKED,
            GeckoEngineSession.geckoErrorToErrorType(GeckoSession.NavigationDelegate.ERROR_PORT_BLOCKED)
        )
        Assert.assertEquals(
            ErrorType.ERROR_UNSAFE_CONTENT_TYPE,
            GeckoEngineSession.geckoErrorToErrorType(GeckoSession.NavigationDelegate.ERROR_UNSAFE_CONTENT_TYPE)
        )
        Assert.assertEquals(
            ErrorType.ERROR_CORRUPTED_CONTENT,
            GeckoEngineSession.geckoErrorToErrorType(GeckoSession.NavigationDelegate.ERROR_CORRUPTED_CONTENT)
        )
        Assert.assertEquals(
            ErrorType.ERROR_CONTENT_CRASHED,
            GeckoEngineSession.geckoErrorToErrorType(GeckoSession.NavigationDelegate.ERROR_CONTENT_CRASHED)
        )
        Assert.assertEquals(
            ErrorType.ERROR_INVALID_CONTENT_ENCODING,
            GeckoEngineSession.geckoErrorToErrorType(GeckoSession.NavigationDelegate.ERROR_INVALID_CONTENT_ENCODING)
        )
        Assert.assertEquals(
            ErrorType.ERROR_UNKNOWN_HOST,
            GeckoEngineSession.geckoErrorToErrorType(GeckoSession.NavigationDelegate.ERROR_UNKNOWN_HOST)
        )
        Assert.assertEquals(
            ErrorType.ERROR_MALFORMED_URI,
            GeckoEngineSession.geckoErrorToErrorType(GeckoSession.NavigationDelegate.ERROR_MALFORMED_URI)
        )
        Assert.assertEquals(
            ErrorType.ERROR_UNKNOWN_PROTOCOL,
            GeckoEngineSession.geckoErrorToErrorType(GeckoSession.NavigationDelegate.ERROR_UNKNOWN_PROTOCOL)
        )
        Assert.assertEquals(
            ErrorType.ERROR_FILE_NOT_FOUND,
            GeckoEngineSession.geckoErrorToErrorType(GeckoSession.NavigationDelegate.ERROR_FILE_NOT_FOUND)
        )
        Assert.assertEquals(
            ErrorType.ERROR_FILE_ACCESS_DENIED,
            GeckoEngineSession.geckoErrorToErrorType(GeckoSession.NavigationDelegate.ERROR_FILE_ACCESS_DENIED)
        )
        Assert.assertEquals(
            ErrorType.ERROR_PROXY_CONNECTION_REFUSED,
            GeckoEngineSession.geckoErrorToErrorType(GeckoSession.NavigationDelegate.ERROR_PROXY_CONNECTION_REFUSED)
        )
        Assert.assertEquals(
            ErrorType.ERROR_UNKNOWN_PROXY_HOST,
            GeckoEngineSession.geckoErrorToErrorType(GeckoSession.NavigationDelegate.ERROR_UNKNOWN_PROXY_HOST)
        )
        Assert.assertEquals(
            ErrorType.ERROR_SAFEBROWSING_MALWARE_URI,
            GeckoEngineSession.geckoErrorToErrorType(GeckoSession.NavigationDelegate.ERROR_SAFEBROWSING_MALWARE_URI)
        )
        Assert.assertEquals(
            ErrorType.ERROR_SAFEBROWSING_HARMFUL_URI,
            GeckoEngineSession.geckoErrorToErrorType(GeckoSession.NavigationDelegate.ERROR_SAFEBROWSING_HARMFUL_URI)
        )
        Assert.assertEquals(
            ErrorType.ERROR_SAFEBROWSING_PHISHING_URI,
            GeckoEngineSession.geckoErrorToErrorType(GeckoSession.NavigationDelegate.ERROR_SAFEBROWSING_PHISHING_URI)
        )
        Assert.assertEquals(
            ErrorType.ERROR_SAFEBROWSING_UNWANTED_URI,
            GeckoEngineSession.geckoErrorToErrorType(GeckoSession.NavigationDelegate.ERROR_SAFEBROWSING_UNWANTED_URI)
        )
        Assert.assertEquals(
            ErrorType.UNKNOWN,
            GeckoEngineSession.geckoErrorToErrorType(-500)
        )
    }

    @Test
    fun defaultSettings() {
        val runtime = mock(GeckoRuntime::class.java)
        `when`(runtime.settings).thenReturn(mock(GeckoRuntimeSettings::class.java))

        val defaultSettings = DefaultSettings(trackingProtectionPolicy = TrackingProtectionPolicy.all())
        val session = GeckoEngineSession(runtime, false, defaultSettings)
        assertTrue(session.geckoSession.settings.getBoolean(GeckoSessionSettings.USE_TRACKING_PROTECTION))
    }

    @Test
    fun contentDelegate() {
        val engineSession = GeckoEngineSession(mock(GeckoRuntime::class.java))
        val geckoSession = mock(GeckoSession::class.java)
        val delegate = engineSession.createContentDelegate()

        var observedChanged = false
        engineSession.register(object : EngineSession.Observer {
            override fun onLongPress(hitResult: HitResult) {
                observedChanged = true
            }
        })

        delegate.onContextMenu(geckoSession, 0, 0, null, ELEMENT_TYPE_AUDIO, "file.mp3")
        assertTrue(observedChanged)

        observedChanged = false
        delegate.onContextMenu(geckoSession, 0, 0, null, ELEMENT_TYPE_AUDIO, null)
        assertFalse(observedChanged)

        observedChanged = false
        delegate.onContextMenu(geckoSession, 0, 0, null, ELEMENT_TYPE_NONE, null)
        assertFalse(observedChanged)
    }

    @Test
    fun handleLongClick() {
        val engineSession = GeckoEngineSession(mock(GeckoRuntime::class.java))

        var result = engineSession.handleLongClick("file.mp3", ELEMENT_TYPE_AUDIO)
        assertNotNull(result)
        assertTrue(result is HitResult.AUDIO && result.src == "file.mp3")

        result = engineSession.handleLongClick("file.mp4", ELEMENT_TYPE_VIDEO)
        assertNotNull(result)
        assertTrue(result is HitResult.VIDEO && result.src == "file.mp4")

        result = engineSession.handleLongClick("file.png", ELEMENT_TYPE_IMAGE)
        assertNotNull(result)
        assertTrue(result is HitResult.IMAGE && result.src == "file.png")

        result = engineSession.handleLongClick("file.png", ELEMENT_TYPE_IMAGE, "https://mozilla.org")
        assertNotNull(result)
        assertTrue(result is HitResult.IMAGE_SRC && result.src == "file.png" && result.uri == "https://mozilla.org")

        result = engineSession.handleLongClick(null, ELEMENT_TYPE_IMAGE)
        assertNotNull(result)
        assertTrue(result is HitResult.UNKNOWN && result.src == "")

        result = engineSession.handleLongClick("tel:+1234567890", ELEMENT_TYPE_NONE)
        assertNotNull(result)
        assertTrue(result is HitResult.PHONE && result.src == "tel:+1234567890")

        result = engineSession.handleLongClick("geo:1,-1", ELEMENT_TYPE_NONE)
        assertNotNull(result)
        assertTrue(result is HitResult.GEO && result.src == "geo:1,-1")

        result = engineSession.handleLongClick("mailto:asa@mozilla.com", ELEMENT_TYPE_NONE)
        assertNotNull(result)
        assertTrue(result is HitResult.EMAIL && result.src == "mailto:asa@mozilla.com")

        result = engineSession.handleLongClick(null, ELEMENT_TYPE_NONE, "https://mozilla.org")
        assertNotNull(result)
        assertTrue(result is HitResult.UNKNOWN && result.src == "https://mozilla.org")

        result = engineSession.handleLongClick("data://foobar", ELEMENT_TYPE_NONE, "https://mozilla.org")
        assertNotNull(result)
        assertTrue(result is HitResult.UNKNOWN && result.src == "data://foobar")

        result = engineSession.handleLongClick(null, ELEMENT_TYPE_NONE, null)
        assertNull(result)
    }

    @Test
    fun setDesktopMode() {
        val runtime = mock(GeckoRuntime::class.java)
        val engineSession = GeckoEngineSession(runtime)

        var desktopModeEnabled = false
        engineSession.register(object : EngineSession.Observer {
            override fun onDesktopModeChange(enabled: Boolean) {
                desktopModeEnabled = true
            }
        })
        engineSession.toggleDesktopMode(true)
        assertTrue(desktopModeEnabled)

        desktopModeEnabled = false
        engineSession.toggleDesktopMode(true)
        assertFalse(desktopModeEnabled)

        engineSession.geckoSession.settings.setInt(GeckoSessionSettings.USER_AGENT_MODE, GeckoSessionSettings.USER_AGENT_MODE_DESKTOP)
        engineSession.toggleDesktopMode(true)
        assertFalse(desktopModeEnabled)

        engineSession.toggleDesktopMode(false)
        assertTrue(desktopModeEnabled)
    }

    @Test
    fun findAll() {
        val finderResult = mock(GeckoSession.FinderResult::class.java)
        val sessionFinder = mock(SessionFinder::class.java)
        `when`(sessionFinder.find("mozilla", 0)).thenReturn(GeckoResult.fromValue(finderResult))

        val geckoSession = mock(GeckoSession::class.java)
        `when`(geckoSession.finder).thenReturn(sessionFinder)

        val engineSession = GeckoEngineSession(mock(GeckoRuntime::class.java))
        engineSession.geckoSession = geckoSession

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
        val finderResult = mock(GeckoSession.FinderResult::class.java)
        val sessionFinder = mock(SessionFinder::class.java)
        `when`(sessionFinder.find(eq(null), anyInt())).thenReturn(GeckoResult.fromValue(finderResult))

        val geckoSession = mock(GeckoSession::class.java)
        `when`(geckoSession.finder).thenReturn(sessionFinder)

        val engineSession = GeckoEngineSession(mock(GeckoRuntime::class.java))
        engineSession.geckoSession = geckoSession

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
        val engineSession = GeckoEngineSession(mock(GeckoRuntime::class.java))
        var clearMatchesReceived = false
        engineSession.geckoSession.eventDispatcher.registerUiThreadListener(
                BundleEventListener { _, _, _ -> clearMatchesReceived = true },
                "GeckoView:ClearMatches"
        )

        engineSession.clearFindMatches()
        assertTrue(clearMatchesReceived)
    }

    @Test
    fun exitFullScreenModeTriggersExitEvent() {
        val engineSession = GeckoEngineSession(mock(GeckoRuntime::class.java))
        val geckoSession = spy(engineSession.geckoSession)
        engineSession.geckoSession = geckoSession
        val observer: EngineSession.Observer = mock()

        // Verify the event is triggered for exiting fullscreen mode and GeckoView is called.
        var fullScreenExitReceived = false
        engineSession.geckoSession.eventDispatcher.registerUiThreadListener(
            BundleEventListener { _, _, _ -> fullScreenExitReceived = true },
            "GeckoViewContent:ExitFullScreen"
        )
        engineSession.exitFullScreenMode()
        assertTrue(fullScreenExitReceived)
        verify(geckoSession).exitFullScreen()

        // Verify the call to the observer.
        engineSession.register(observer)
        engineSession.geckoSession.contentDelegate.onFullScreen(geckoSession, true)
        verify(observer).onFullScreenChange(true)
    }

    @Test
    fun exitFullscreenTrueHasNoInteraction() {
        val engineSession = GeckoEngineSession(mock(GeckoRuntime::class.java))
        val geckoSession = spy(engineSession.geckoSession)
        engineSession.geckoSession = geckoSession

        engineSession.exitFullScreenMode()
        verify(geckoSession).exitFullScreen()
    }

    @Test
    fun clearData() {
        val runtime = mock(GeckoRuntime::class.java)
        val engineSession = GeckoEngineSession(runtime)
        val observer: EngineSession.Observer = mock()

        engineSession.register(observer)

        engineSession.clearData()

        verifyZeroInteractions(observer)
    }

    private fun mockLoadRequest(uri: String): GeckoSession.NavigationDelegate.LoadRequest {
        val constructor = GeckoSession.NavigationDelegate.LoadRequest::class.java.getDeclaredConstructor(
            String::class.java,
            String::class.java,
            Int::class.java,
            Int::class.java)
        constructor.isAccessible = true

        return constructor.newInstance(uri, uri, 0, 0)
    }

    @Test
    fun `after onCrash get called geckoSession must be reset`() {
        val runtime = mock(GeckoRuntime::class.java)
        val engineSession = GeckoEngineSession(runtime)
        val oldGeckoSession = engineSession.geckoSession

        assertTrue(engineSession.geckoSession.isOpen)

        oldGeckoSession.contentDelegate.onCrash(mock())

        assertFalse(oldGeckoSession.isOpen)
        assertTrue(engineSession.geckoSession != oldGeckoSession)
    }

    @Test
    fun whenOnExternalResponseDoNotProvideAFileNameMustProvideMeaningFulFileNameToTheSessionObserver() {
        val engineSession = GeckoEngineSession(mock(GeckoRuntime::class.java))
        var meaningFulFileName = ""

        val observer = object : EngineSession.Observer {
            override fun onExternalResource(
                url: String,
                fileName: String,
                contentLength: Long?,
                contentType: String?,
                cookie: String?,
                userAgent: String?
            ) {
                meaningFulFileName = fileName
            }
        }
        engineSession.register(observer)

        val info: GeckoSession.WebResponseInfo = createMockedWebResponseInfo(
            uri = "http://ipv4.download.thinkbroadband.com/1MB.zip",
            contentLength = 0,
            contentType = "",
            filename = null
        )

        engineSession.geckoSession.contentDelegate.onExternalResponse(mock(), info)

        assertEquals("1MB.zip", meaningFulFileName)
    }

    @Test
    fun `Closing engine session should close underlying gecko session`() {
        val geckoSession = mockGeckoSession()

        val engineSession = GeckoEngineSession(mock(GeckoRuntime::class.java))
        engineSession.geckoSession = geckoSession

        engineSession.close()

        verify(geckoSession).close()
    }

    private fun mockGeckoSession(): GeckoSession {
        val session = mock(GeckoSession::class.java)
        `when`(session.settings).thenReturn(
            mock(GeckoSessionSettings::class.java))
        return session
    }
}
