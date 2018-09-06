/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

package mozilla.components.browser.engine.system

import android.os.Bundle
import android.webkit.WebView
import mozilla.components.concept.engine.EngineSession
import mozilla.components.support.ktx.kotlin.toBundle
import java.lang.ref.WeakReference
import kotlinx.coroutines.experimental.launch
import mozilla.components.concept.engine.Settings
import mozilla.components.concept.engine.request.RequestInterceptor

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
        get() = _settings ?: run { initSettings() }

    private var _settings: Settings? = null

    internal fun initSettings(): Settings {
        currentView()?.settings?.let {
            _settings = object : Settings {
                override var javascriptEnabled: Boolean
                    get() = it.javaScriptEnabled
                    set(value) { it.javaScriptEnabled = value }

                override var domStorageEnabled: Boolean
                    get() = it.domStorageEnabled
                    set(value) { it.domStorageEnabled = value }

                override var webFontsEnabled: Boolean
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
                    this.javascriptEnabled = defaultSettings.javascriptEnabled
                    this.domStorageEnabled = defaultSettings.domStorageEnabled
                    this.webFontsEnabled = defaultSettings.webFontsEnabled
                    this.trackingProtectionPolicy = defaultSettings.trackingProtectionPolicy
                    this.requestInterceptor = defaultSettings.requestInterceptor
                }
            }
            return _settings as Settings
        } ?: throw IllegalStateException("System engine session not initialized")
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

    /**
     * Helper method to notify observers from other classes in this package. This is needed as
     * almost everything is implemented by WebView and its listeners. There is no actual concept of
     * a session when using WebView.
     */
    internal fun internalNotifyObservers(block: Observer.() -> Unit) {
        super.notifyObservers(block)
    }
}
