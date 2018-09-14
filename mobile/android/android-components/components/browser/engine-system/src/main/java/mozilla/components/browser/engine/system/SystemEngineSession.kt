/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

package mozilla.components.browser.engine.system

import android.content.Context
import android.graphics.Bitmap
import android.os.Bundle
import android.webkit.CookieManager
import android.webkit.WebSettings
import android.webkit.WebStorage
import android.webkit.WebView
import kotlinx.coroutines.experimental.launch
import mozilla.components.concept.engine.EngineSession
import mozilla.components.concept.engine.DefaultSettings
import android.webkit.WebViewDatabase
import mozilla.components.concept.engine.Settings
import mozilla.components.concept.engine.request.RequestInterceptor
import mozilla.components.support.ktx.kotlin.toBundle
import java.lang.ref.WeakReference
import kotlin.reflect.KProperty

internal val additionalHeaders = mapOf(
    // For every request WebView sends a "X-requested-with" header with the package name of the
    // application. We can't really prevent that but we can at least send an empty value.
    // Unfortunately the additional headers will not be propagated to subsequent requests
    // (e.g. redirects). See issue #696.
    "X-Requested-With" to ""
)

/**
 * WebView-based EngineSession implementation.
 */
@Suppress("TooManyFunctions")
class SystemEngineSession(private val defaultSettings: Settings? = null) : EngineSession() {

    internal var view: WeakReference<SystemEngineView>? = null
    internal var scheduledLoad = ScheduledLoad(null)
    @Volatile internal var trackingProtectionEnabled = false
    @Volatile internal var webFontsEnabled = true
    @Volatile internal var internalSettings: Settings? = null

    /**
     * See [EngineSession.loadUrl]
     */
    override fun loadUrl(url: String) {
        val internalView = currentView()

        if (internalView == null) {
            // We can't load a URL without a WebView. So let's just remember the URL here until
            // this session gets linked to a WebView. See: EngineView.render(session).
            scheduledLoad = ScheduledLoad(url)
        } else {
            view?.get()?.currentUrl = url
            internalView.loadUrl(url, additionalHeaders)
        }
    }

    /**
     * See [EngineSession.loadData]
     */
    override fun loadData(data: String, mimeType: String, encoding: String) {
        val internalView = currentView()

        if (internalView == null) {
            // We remember the data that we want to load and when then session gets linked
            // to a WebView we call loadData then.
            scheduledLoad = ScheduledLoad(data, mimeType)
        } else {
            internalView.loadData(data, mimeType, encoding)
        }
    }

    /**
     * See [EngineSession.stopLoading]
     */
    override fun stopLoading() {
        currentView()?.stopLoading()
    }

    /**
     * See [EngineSession.reload]
     */
    override fun reload() {
        currentView()?.reload()
    }

    /**
     * See [EngineSession.goBack]
     */
    override fun goBack() {
        currentView()?.goBack()
    }

    /**
     * See [EngineSession.goForward]
     */
    override fun goForward() {
        currentView()?.goForward()
    }

    /**
     * See [EngineSession.saveState]
     */
    override fun saveState(): Map<String, Any> {
        val state = Bundle()
        currentView()?.saveState(state)

        return mutableMapOf<String, Any>().apply {
            state.keySet().forEach { k -> put(k, state[k]) }
        }
    }

    /**
     * See [EngineSession.restoreState]
     */
    override fun restoreState(state: Map<String, Any>) {
        currentView()?.restoreState(state.toBundle())
    }

    /**
     * See [EngineSession.enableTrackingProtection]
     *
     * Note that specifying tracking protection policies at run-time is
     * not supported by [SystemEngine]. Tracking protection is always active
     * for all URLs provided in domain_blacklist.json and domain_overrides.json,
     * which both support specifying categories. See [UrlMatcher] for how to
     * enable/disable specific categories.
     */
    override fun enableTrackingProtection(policy: TrackingProtectionPolicy) {
        currentView()?.let {
            // Make sure Url matcher is preloaded now that tracking protection is enabled
            launch { SystemEngineView.getOrCreateUrlMatcher(it.context) }
        }

        trackingProtectionEnabled = true
        notifyObservers { onTrackerBlockingEnabledChange(true) }
    }

    /**
     * See [EngineSession.disableTrackingProtection]
     */
    override fun disableTrackingProtection() {
        trackingProtectionEnabled = false
        notifyObservers { onTrackerBlockingEnabledChange(false) }
    }

    /**
     * See [EngineSession.clearData]
     */
    override fun clearData() {
        currentView()?.apply {
            clearFormData()
            clearHistory()
            clearMatches()
            clearSslPreferences()
            clearCache(true)

            // We don't care about the callback - we just want to make sure cookies are gone
            CookieManager.getInstance().removeAllCookies(null)

            webStorage().deleteAllData()

            webViewDatabase(context).clearHttpAuthUsernamePassword()
        }
    }

    /**
     * See [EngineSession.findAll]
     */
    override fun findAll(text: String) {
        notifyObservers { onFind(text) }
        currentView()?.findAllAsync(text)
    }

    /**
     * See [EngineSession.findNext]
     */
    override fun findNext(forward: Boolean) {
        currentView()?.findNext(forward)
    }

    /**
     * See [EngineSession.clearFindResults]
     */
    override fun clearFindMatches() {
        currentView()?.clearMatches()
    }

    /**
     * See [EngineSession.settings]
     */
    override val settings: Settings
        // Settings are initialized when the engine view is rendered
        // as we need the WebView instance to do it. If this method is
        // called before that we can return the provided default settings,
        // or the global defaults.
        get() = internalSettings ?: defaultSettings ?: DefaultSettings()

    class WebSetting<T>(private val get: () -> T, private val set: (T) -> Unit) {
        operator fun getValue(thisRef: Any?, property: KProperty<*>): T = get()
        operator fun setValue(thisRef: Any?, property: KProperty<*>, value: T) = set(value)
    }

    internal fun initSettings(): Settings {
        currentView()?.let { webView ->
            webView.settings?.let { webSettings ->
                // Explicitly set global defaults.
                webSettings.setAppCacheEnabled(false)
                webSettings.databaseEnabled = false
                webSettings.saveFormData = false
                webSettings.savePassword = false

                // We currently don't implement the callback to support turning this on.
                webSettings.setGeolocationEnabled(false)

                // webViewSettings built-in zoom controls are the only supported ones, so they should be turned on.
                webSettings.builtInZoomControls = true

                return initSettings(webView, webSettings)
            }
        } ?: throw IllegalStateException("System engine session not initialized")
    }

    private fun initSettings(webView: WebView, s: WebSettings): Settings {
        internalSettings = object : Settings() {
            override var javascriptEnabled by WebSetting(s::getJavaScriptEnabled, s::setJavaScriptEnabled)
            override var domStorageEnabled by WebSetting(s::getDomStorageEnabled, s::setDomStorageEnabled)
            override var allowFileAccess by WebSetting(s::getAllowFileAccess, s::setAllowFileAccess)
            override var allowContentAccess by WebSetting(s::getAllowContentAccess, s::setAllowContentAccess)
            override var userAgentString by WebSetting(s::getUserAgentString, s::setUserAgentString)
            override var displayZoomControls by WebSetting(s::getDisplayZoomControls, s::setDisplayZoomControls)
            override var loadWithOverviewMode by WebSetting(s::getLoadWithOverviewMode, s::setLoadWithOverviewMode)
            override var allowFileAccessFromFileURLs by WebSetting(
                    s::getAllowFileAccessFromFileURLs, s::setAllowFileAccessFromFileURLs)
            override var allowUniversalAccessFromFileURLs by WebSetting(
                    s::getAllowUniversalAccessFromFileURLs, s::setAllowUniversalAccessFromFileURLs)
            override var mediaPlaybackRequiresUserGesture by WebSetting(
                    s::getMediaPlaybackRequiresUserGesture, s::setMediaPlaybackRequiresUserGesture)
            override var javaScriptCanOpenWindowsAutomatically by WebSetting(
                    s::getJavaScriptCanOpenWindowsAutomatically, s::setJavaScriptCanOpenWindowsAutomatically)

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
                get() = if (trackingProtectionEnabled)
                    TrackingProtectionPolicy.all()
                else
                    TrackingProtectionPolicy.none()
                set(value) = value?.let { enableTrackingProtection(it) } ?: disableTrackingProtection()

            override var requestInterceptor: RequestInterceptor? = null
        }.apply {
            defaultSettings?.let {
                javascriptEnabled = it.javascriptEnabled
                domStorageEnabled = it.domStorageEnabled
                webFontsEnabled = it.webFontsEnabled
                displayZoomControls = it.displayZoomControls
                loadWithOverviewMode = it.loadWithOverviewMode
                trackingProtectionPolicy = it.trackingProtectionPolicy
                requestInterceptor = it.requestInterceptor
                userAgentString = it.userAgentString
                mediaPlaybackRequiresUserGesture = it.mediaPlaybackRequiresUserGesture
                javaScriptCanOpenWindowsAutomatically = it.javaScriptCanOpenWindowsAutomatically
                allowFileAccess = it.allowFileAccess
                allowContentAccess = it.allowContentAccess
                allowUniversalAccessFromFileURLs = it.allowUniversalAccessFromFileURLs
                allowFileAccessFromFileURLs = it.allowFileAccessFromFileURLs
                verticalScrollBarEnabled = it.verticalScrollBarEnabled
                horizontalScrollBarEnabled = it.horizontalScrollBarEnabled
            }
        }

        return internalSettings as Settings
    }

    /**
     * See [EngineSession.toggleDesktopMode]
     */
    override fun toggleDesktopMode(enable: Boolean, reload: Boolean) {
        currentView()?.let { view ->
            val webSettings = view.settings
            webSettings.userAgentString = toggleDesktopUA(webSettings.userAgentString, enable)
            webSettings.useWideViewPort = enable

            if (reload) {
                view.reload()
            }
        }
    }

    /**
     * See [EngineSession.exitFullScreenMode]
     */
    override fun exitFullScreenMode() {
        // no-op
    }

    override fun captureThumbnail(): Bitmap? {
        val webView = currentView()

        return webView?.let {
            it.buildDrawingCache()
            val outBitmap = Bitmap.createBitmap(webView.drawingCache)
            it.destroyDrawingCache()
            outBitmap
        }
    }

    internal fun toggleDesktopUA(userAgent: String, requestDesktop: Boolean): String {
        return if (requestDesktop) {
            userAgent.replace("Mobile", "eliboM").replace("Android", "diordnA")
        } else {
            userAgent.replace("eliboM", "Mobile").replace("diordnA", "Android")
        }
    }

    internal fun currentView(): WebView? {
        return view?.get()?.currentWebView
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
}
