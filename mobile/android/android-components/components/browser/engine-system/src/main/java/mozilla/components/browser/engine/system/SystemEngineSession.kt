/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

package mozilla.components.browser.engine.system

import android.content.Context
import android.webkit.CookieManager
import android.webkit.WebChromeClient
import android.webkit.WebSettings
import android.webkit.WebStorage
import android.webkit.WebView
import android.webkit.WebViewClient
import android.webkit.WebViewDatabase
import androidx.annotation.VisibleForTesting
import kotlinx.coroutines.CoroutineScope
import kotlinx.coroutines.Dispatchers
import kotlinx.coroutines.launch
import mozilla.components.browser.errorpages.ErrorType
import mozilla.components.concept.engine.Engine.BrowsingData
import mozilla.components.concept.engine.EngineSession
import mozilla.components.concept.engine.EngineSessionState
import mozilla.components.concept.engine.Settings
import mozilla.components.concept.engine.history.HistoryTrackingDelegate
import mozilla.components.concept.engine.request.RequestInterceptor
import kotlin.reflect.KProperty

internal val xRequestHeader = mapOf(
    // For every request WebView sends a "X-requested-with" header with the package name of the
    // application. We can't really prevent that but we can at least send an empty value.
    // Unfortunately the additional headers will not be propagated to subsequent requests
    // (e.g. redirects). See issue #696.
    "X-Requested-With" to "",
)

/**
 * WebView-based EngineSession implementation.
 */
@Suppress("LargeClass", "TooManyFunctions")
class SystemEngineSession(
    context: Context,
    private val defaultSettings: Settings? = null,
) : EngineSession() {
    private val resources = context.resources

    @Volatile internal lateinit var internalSettings: Settings

    @Volatile internal var historyTrackingDelegate: HistoryTrackingDelegate? = null

    @Volatile internal var trackingProtectionPolicy: TrackingProtectionPolicy? = null

    @Volatile internal var webFontsEnabled = true

    @Volatile internal var currentUrl = ""

    @Volatile internal var useWideViewPort: Boolean? = null // See [toggleDesktopMode]
    @Volatile internal var fullScreenCallback: WebChromeClient.CustomViewCallback? = null

    // This is public for FFTV which needs access to the WebView instance. We can mark it internal once
    // https://github.com/mozilla-mobile/android-components/issues/1616 is resolved.
    @Volatile var webView: WebView = NestedWebView(context)
        set(value) {
            field = value
            initSettings()
        }

    init {
        initSettings()
    }

    /**
     * See [EngineSession.loadUrl]. Note that [LoadUrlFlags] are ignored in this engine
     * implementation.
     */
    override fun loadUrl(
        url: String,
        parent: EngineSession?,
        flags: LoadUrlFlags,
        additionalHeaders: Map<String, String>?,
    ) {
        val headers =
            if (additionalHeaders == null) {
                xRequestHeader
            } else {
                xRequestHeader + additionalHeaders
            }

        if (!url.isEmpty()) {
            currentUrl = url
            webView.loadUrl(url, headers)
        }
    }

    /**
     * See [EngineSession.loadData]
     */
    override fun loadData(data: String, mimeType: String, encoding: String) {
        webView.loadData(data, mimeType, encoding)
    }

    override fun requestPdfToDownload() {
        throw UnsupportedOperationException("PDF support is not available in this engine")
    }

    /**
     * See [EngineSession.stopLoading]
     */
    override fun stopLoading() {
        webView.stopLoading()
    }

    /**
     * See [EngineSession.reload]
     * @param flags currently not supported in `SystemEngineSession`.
     */
    override fun reload(flags: LoadUrlFlags) {
        webView.reload()
    }

    /**
     * See [EngineSession.goBack]
     */
    override fun goBack(userInteraction: Boolean) {
        webView.goBack()
        if (webView.canGoBack()) {
            notifyObservers { onNavigateBack() }
        }
    }

    /**
     * See [EngineSession.goForward]
     */
    override fun goForward(userInteraction: Boolean) {
        webView.goForward()
    }

    /**
     * See [EngineSession.goToHistoryIndex]
     */
    override fun goToHistoryIndex(index: Int) {
        val historyList = webView.copyBackForwardList()
        webView.goBackOrForward(index - historyList.currentIndex)
    }

    /**
     * See [EngineSession.restoreState]
     */
    override fun restoreState(state: EngineSessionState): Boolean {
        if (state !is SystemEngineSessionState) {
            throw IllegalArgumentException("Can only restore from SystemEngineSessionState")
        }

        return state.bundle?.let { webView.restoreState(it) } != null
    }

    /**
     * See [EngineSession.updateTrackingProtection]
     */
    override fun updateTrackingProtection(policy: TrackingProtectionPolicy) {
        // Make sure Url matcher is preloaded now that tracking protection is enabled
        CoroutineScope(Dispatchers.IO).launch {
            SystemEngineView.getOrCreateUrlMatcher(resources, policy)
        }

        // TODO check if policy should be applied for this session type
        // (regular|private) once we support private browsing in system engine:
        // https://github.com/mozilla-mobile/android-components/issues/649
        trackingProtectionPolicy = policy
        notifyObservers { onTrackerBlockingEnabledChange(true) }
    }

    @VisibleForTesting
    internal fun disableTrackingProtection() {
        trackingProtectionPolicy = null
        notifyObservers { onTrackerBlockingEnabledChange(false) }
    }

    /**
     * See [EngineSession.close]
     */
    override fun close() {
        super.close()
        // The WebView instance must remain useable for the duration of this session.
        // We can only destroy it once we're sure this session will not be used
        // again which is why destroy happens here are not part of regular (activity)
        // lifecycle event.
        webView.destroy()
    }

    /**
     * See [EngineSession.clearData]
     */
    @Suppress("TooGenericExceptionCaught")
    override fun clearData(data: BrowsingData, host: String?, onSuccess: () -> Unit, onError: (Throwable) -> Unit) {
        webView.apply {
            try {
                if (data.contains(BrowsingData.DOM_STORAGES)) {
                    webStorage().deleteAllData()
                }
                if (data.contains(BrowsingData.IMAGE_CACHE) || data.contains(BrowsingData.NETWORK_CACHE)) {
                    clearCache(true)
                }
                if (data.contains(BrowsingData.COOKIES)) {
                    CookieManager.getInstance().removeAllCookies(null)
                }
                if (data.contains(BrowsingData.AUTH_SESSIONS)) {
                    webViewDatabase(context).clearHttpAuthUsernamePassword()
                }
                if (data.contains(BrowsingData.ALL)) {
                    clearSslPreferences()
                    clearFormData()
                    clearMatches()
                    clearHistory()
                }
                onSuccess()
            } catch (e: Throwable) {
                onError(e)
            }
        }
    }

    /**
     * See [EngineSession.findAll]
     */
    override fun findAll(text: String) {
        notifyObservers { onFind(text) }
        webView.findAllAsync(text)
    }

    /**
     * See [EngineSession.findNext]
     */
    override fun findNext(forward: Boolean) {
        webView.findNext(forward)
    }

    /**
     * See [EngineSession.clearFindMatches]
     */
    override fun clearFindMatches() {
        webView.clearMatches()
    }

    /**
     * Clears the internal back/forward list.
     */
    override fun purgeHistory() {
        webView.clearHistory()
    }

    /**
     * See [EngineSession.settings]
     */
    override val settings: Settings
        get() = internalSettings

    class WebSetting<T>(private val get: () -> T, private val set: (T) -> Unit) {
        operator fun getValue(thisRef: Any?, property: KProperty<*>): T = get()
        operator fun setValue(thisRef: Any?, property: KProperty<*>, value: T) = set(value)
    }

    @VisibleForTesting
    internal fun initSettings() {
        webView.settings.apply {
            // Explicitly set global defaults.

            @Suppress("DEPRECATION")
            // Deprecation will be handled in https://github.com/mozilla-mobile/android-components/issues/8512
            setAppCacheEnabled(false)
            databaseEnabled = false

            setDeprecatedWebSettings(this)

            // We currently don't implement the callback to support turning this on.
            setGeolocationEnabled(false)

            // webViewSettings built-in zoom controls are the only supported ones,
            // so they should be turned on but hidden.
            builtInZoomControls = true
            displayZoomControls = false

            initSettings(webView, this)
        }
    }

    @Suppress("DEPRECATION")
    private fun setDeprecatedWebSettings(webSettings: WebSettings) {
        // Since API26 an autofill platform feature is used instead of WebView's form data. This
        // has no effect. Form data is supported on pre-26 API versions.
        webSettings.saveFormData = false
        // Deprecated in API18.
        webSettings.savePassword = false
    }

    private fun setUseWideViewPort(settings: WebSettings, useWideViewPort: Boolean?) {
        this.useWideViewPort = useWideViewPort
        useWideViewPort?.let { settings.useWideViewPort = it }
    }

    private fun initSettings(webView: WebView, s: WebSettings) {
        internalSettings = object : Settings() {
            override var javascriptEnabled by WebSetting(s::getJavaScriptEnabled, s::setJavaScriptEnabled)
            override var domStorageEnabled by WebSetting(s::getDomStorageEnabled, s::setDomStorageEnabled)
            override var allowFileAccess by WebSetting(s::getAllowFileAccess, s::setAllowFileAccess)
            override var allowContentAccess by WebSetting(s::getAllowContentAccess, s::setAllowContentAccess)
            override var userAgentString by WebSetting(s::getUserAgentString, s::setUserAgentString)
            override var displayZoomControls by WebSetting(s::getDisplayZoomControls, s::setDisplayZoomControls)
            override var loadWithOverviewMode by WebSetting(s::getLoadWithOverviewMode, s::setLoadWithOverviewMode)
            override var useWideViewPort: Boolean?
                get() = this@SystemEngineSession.useWideViewPort
                set(value) = setUseWideViewPort(s, value)
            override var supportMultipleWindows by WebSetting(s::supportMultipleWindows, s::setSupportMultipleWindows)

            @Suppress("DEPRECATION")
            // Deprecation will be handled in https://github.com/mozilla-mobile/android-components/issues/8513
            override var allowFileAccessFromFileURLs by WebSetting(
                s::getAllowFileAccessFromFileURLs,
                s::setAllowFileAccessFromFileURLs,
            )

            @Suppress("DEPRECATION")
            // Deprecation will be handled in https://github.com/mozilla-mobile/android-components/issues/8514
            override var allowUniversalAccessFromFileURLs by WebSetting(
                s::getAllowUniversalAccessFromFileURLs,
                s::setAllowUniversalAccessFromFileURLs,
            )

            override var mediaPlaybackRequiresUserGesture by WebSetting(
                s::getMediaPlaybackRequiresUserGesture,
                s::setMediaPlaybackRequiresUserGesture,
            )
            override var javaScriptCanOpenWindowsAutomatically by WebSetting(
                s::getJavaScriptCanOpenWindowsAutomatically,
                s::setJavaScriptCanOpenWindowsAutomatically,
            )

            override var verticalScrollBarEnabled
                get() = webView.isVerticalScrollBarEnabled
                set(value) { webView.isVerticalScrollBarEnabled = value }

            override var horizontalScrollBarEnabled
                get() = webView.isHorizontalScrollBarEnabled
                set(value) { webView.isHorizontalScrollBarEnabled = value }

            override var webFontsEnabled
                get() = this@SystemEngineSession.webFontsEnabled
                set(value) { this@SystemEngineSession.webFontsEnabled = value }

            override var trackingProtectionPolicy: TrackingProtectionPolicy?
                get() = this@SystemEngineSession.trackingProtectionPolicy
                set(value) = value?.let { updateTrackingProtection(it) } ?: disableTrackingProtection()

            override var historyTrackingDelegate: HistoryTrackingDelegate?
                get() = this@SystemEngineSession.historyTrackingDelegate
                set(value) { this@SystemEngineSession.historyTrackingDelegate = value }

            override var requestInterceptor: RequestInterceptor? = null
        }.apply {
            defaultSettings?.let {
                javascriptEnabled = it.javascriptEnabled
                domStorageEnabled = it.domStorageEnabled
                webFontsEnabled = it.webFontsEnabled
                displayZoomControls = it.displayZoomControls
                loadWithOverviewMode = it.loadWithOverviewMode
                useWideViewPort = it.useWideViewPort
                trackingProtectionPolicy = it.trackingProtectionPolicy
                historyTrackingDelegate = it.historyTrackingDelegate
                requestInterceptor = it.requestInterceptor
                mediaPlaybackRequiresUserGesture = it.mediaPlaybackRequiresUserGesture
                javaScriptCanOpenWindowsAutomatically = it.javaScriptCanOpenWindowsAutomatically
                allowFileAccess = it.allowFileAccess
                allowContentAccess = it.allowContentAccess
                allowUniversalAccessFromFileURLs = it.allowUniversalAccessFromFileURLs
                allowFileAccessFromFileURLs = it.allowFileAccessFromFileURLs
                verticalScrollBarEnabled = it.verticalScrollBarEnabled
                horizontalScrollBarEnabled = it.horizontalScrollBarEnabled
                userAgentString = it.userAgentString
                supportMultipleWindows = it.supportMultipleWindows
            }
        }
    }

    /**
     * See [EngineSession.toggleDesktopMode]
     *
     * Precondition:
     * If settings.useWideViewPort = true, then webSettings.useWideViewPort is always on
     * If settings.useWideViewPort = false or null, then webSettings.useWideViewPort can be on/off
     */
    override fun toggleDesktopMode(enable: Boolean, reload: Boolean) {
        val webSettings = webView.settings
        webSettings.userAgentString = toggleDesktopUA(webSettings.userAgentString, enable)
        webSettings.useWideViewPort = if (settings.useWideViewPort == true) true else enable

        notifyObservers { onDesktopModeChange(enable) }

        if (reload) {
            webView.reload()
        }
    }

    /**
     * See [EngineSession.exitFullScreenMode]
     */
    override fun exitFullScreenMode() {
        fullScreenCallback?.onCustomViewHidden()
    }

    internal fun toggleDesktopUA(userAgent: String, requestDesktop: Boolean): String {
        return if (requestDesktop) {
            userAgent.replace("Mobile", "eliboM").replace("Android", "diordnA")
        } else {
            userAgent.replace("eliboM", "Mobile").replace("diordnA", "Android")
        }
    }

    internal fun webStorage(): WebStorage = WebStorage.getInstance()

    internal fun webViewDatabase(context: Context) = WebViewDatabase.getInstance(context)

    /**
     * Helper method to notify observers from other classes in this package. This is needed as
     * almost everything is implemented by WebView and its listeners. There is no actual concept of
     * a session when using WebView.
     */
    internal fun internalNotifyObservers(block: Observer.() -> Unit) {
        super.notifyObservers(block)
    }

    companion object {
        /**
         * Provides an ErrorType corresponding to the error code provided.
         *
         * Chromium's mapping (internal error code, to Android WebView error code) is described at:
         * https://goo.gl/vspwct (ErrorCodeConversionHelper.java)
         */
        internal fun webViewErrorToErrorType(errorCode: Int) =
            when (errorCode) {
                WebViewClient.ERROR_UNKNOWN -> ErrorType.UNKNOWN

                // This is probably the most commonly shown error. If there's no network, we inevitably
                // show this.
                WebViewClient.ERROR_HOST_LOOKUP -> ErrorType.ERROR_UNKNOWN_HOST

                WebViewClient.ERROR_CONNECT -> ErrorType.ERROR_CONNECTION_REFUSED

                // It's unclear what this actually means - it's not well documented. Based on looking at
                // ErrorCodeConversionHelper this could happen if networking is disabled during load, in which
                // case the generic error is good enough:
                WebViewClient.ERROR_IO -> ErrorType.ERROR_CONNECTION_REFUSED

                WebViewClient.ERROR_TIMEOUT -> ErrorType.ERROR_NET_TIMEOUT

                WebViewClient.ERROR_REDIRECT_LOOP -> ErrorType.ERROR_REDIRECT_LOOP

                WebViewClient.ERROR_UNSUPPORTED_SCHEME -> ErrorType.ERROR_UNKNOWN_PROTOCOL

                WebViewClient.ERROR_FAILED_SSL_HANDSHAKE -> ErrorType.ERROR_SECURITY_SSL

                WebViewClient.ERROR_BAD_URL -> ErrorType.ERROR_MALFORMED_URI

                // Seems to be an indication of OOM, insufficient resources, or too many queued DNS queries
                WebViewClient.ERROR_TOO_MANY_REQUESTS -> ErrorType.UNKNOWN

                WebViewClient.ERROR_FILE_NOT_FOUND -> ErrorType.ERROR_FILE_NOT_FOUND

                // There's no mapping for the following errors yet. At the time this library was
                // extracted from Focus we didn't use any of those errors.
                // WebViewClient.ERROR_UNSUPPORTED_AUTH_SCHEME
                // WebViewClient.ERROR_AUTHENTICATION
                // WebViewClient.ERROR_FILE
                else -> ErrorType.UNKNOWN
            }
    }
}
