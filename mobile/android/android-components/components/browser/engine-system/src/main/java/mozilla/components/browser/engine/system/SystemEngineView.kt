/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

package mozilla.components.browser.engine.system

import android.content.Context
import android.graphics.Bitmap
import android.net.Uri
import android.os.Handler
import android.os.Message
import android.util.AttributeSet
import android.view.View
import android.webkit.CookieManager
import android.webkit.DownloadListener
import android.webkit.WebChromeClient
import android.webkit.WebResourceRequest
import android.webkit.WebResourceResponse
import android.webkit.WebView
import android.webkit.WebView.HitTestResult.EMAIL_TYPE
import android.webkit.WebView.HitTestResult.GEO_TYPE
import android.webkit.WebView.HitTestResult.IMAGE_TYPE
import android.webkit.WebView.HitTestResult.PHONE_TYPE
import android.webkit.WebView.HitTestResult.SRC_ANCHOR_TYPE
import android.webkit.WebView.HitTestResult.SRC_IMAGE_ANCHOR_TYPE
import android.webkit.WebViewClient
import android.widget.FrameLayout
import mozilla.components.browser.engine.system.matcher.UrlMatcher
import mozilla.components.concept.engine.EngineSession
import mozilla.components.concept.engine.EngineView
import mozilla.components.concept.engine.HitResult
import mozilla.components.support.utils.DownloadUtils
import java.lang.ref.WeakReference
import java.net.URI

/**
 * WebView-based implementation of EngineView.
 */
@Suppress("TooManyFunctions")
class SystemEngineView @JvmOverloads constructor(
    context: Context,
    attrs: AttributeSet? = null,
    defStyleAttr: Int = 0
) : FrameLayout(context, attrs, defStyleAttr), EngineView, View.OnLongClickListener {
    internal var currentWebView = createWebView(context)
    internal var currentUrl = ""
    private var session: SystemEngineSession? = null
    internal var fullScreenCallback: WebChromeClient.CustomViewCallback? = null

    init {
        // Currently this implementation supports only a single WebView. Eventually this
        // implementation should be able to maintain at least two WebView instances to be able to
        // animate the views when switching sessions.
        addView(currentWebView)
    }

    /**
     * Render the content of the given session.
     */
    override fun render(session: EngineSession) {
        val internalSession = session as SystemEngineSession
        this.session = internalSession

        internalSession.view = WeakReference(this)
        internalSession.initSettings()

        internalSession.scheduledLoad.data?.let {
            currentWebView.loadData(it, internalSession.scheduledLoad.mimeType, "UTF-8")
            internalSession.scheduledLoad = ScheduledLoad()
        }

        internalSession.scheduledLoad.url?.let {
            currentWebView.loadUrl(it, additionalHeaders)
            internalSession.scheduledLoad = ScheduledLoad()
        }
    }

    override fun onLongClick(view: View?): Boolean {
        val result = currentWebView.hitTestResult
        return handleLongClick(result.type, result.extra)
    }

    override fun onPause() {
        currentWebView.onPause()
        currentWebView.pauseTimers()
    }

    override fun onResume() {
        currentWebView.onResume()
        currentWebView.resumeTimers()
    }

    override fun onDestroy() {
        currentWebView.destroy()
    }

    private fun createWebView(context: Context): WebView {
        val webView = WebView(context)
        webView.tag = "mosac_system_engine_webview"
        webView.webViewClient = createWebViewClient(webView)
        webView.webChromeClient = createWebChromeClient()
        webView.setDownloadListener(createDownloadListener())
        webView.setFindListener(createFindListener())
        return webView
    }

    @Suppress("ComplexMethod")
    private fun createWebViewClient(webView: WebView) = object : WebViewClient() {
        override fun onPageStarted(view: WebView?, url: String?, favicon: Bitmap?) {
            url?.let {
                currentUrl = url
                session?.internalNotifyObservers {
                    onLoadingStateChange(true)
                }
            }
        }

        override fun onPageFinished(view: WebView?, url: String?) {
            url?.let {
                val cert = view?.certificate

                session?.internalNotifyObservers {
                    onLocationChange(it)
                    onLoadingStateChange(false)
                    onNavigationStateChange(webView.canGoBack(), webView.canGoForward())
                    onSecurityChange(cert != null, cert?.let { URI(url).host }, cert?.issuedBy?.oName)
                }
            }
        }

        @Suppress("ReturnCount")
        override fun shouldInterceptRequest(view: WebView, request: WebResourceRequest): WebResourceResponse? {
            if (session?.webFontsEnabled == false && UrlMatcher.isWebFont(request.url)) {
                return WebResourceResponse(null, null, null)
            }

            if (session?.trackingProtectionEnabled == true) {
                val resourceUri = request.url
                val scheme = resourceUri.scheme
                val path = resourceUri.path

                if (!request.isForMainFrame && scheme != "http" && scheme != "https") {
                    // Block any malformed non-http(s) URIs. WebView will already ignore things like market: URLs,
                    // but not in all cases (malformed market: URIs, such as market:://... will still end up here).
                    // (Note: data: URIs are automatically handled by WebView, and won't end up here either.)
                    // file:// URIs are disabled separately by setting WebSettings.setAllowFileAccess()
                    return WebResourceResponse(null, null, null)
                }

                // WebView always requests a favicon, even though it won't be used anywhere. This check
                // isn't able to block all favicons (some of them will be loaded using <link rel="shortcut icon">
                // with a custom URL which we can't match or detect), but reduces the amount of unnecessary
                // favicon loading that's performed.
                if (path != null && path.endsWith("/favicon.ico")) {
                    return WebResourceResponse(null, null, null)
                }

                if (!request.isForMainFrame &&
                        getOrCreateUrlMatcher(view.context).matches(resourceUri, Uri.parse(currentUrl))) {
                    session?.internalNotifyObservers { onTrackerBlocked(resourceUri.toString()) }
                    return WebResourceResponse(null, null, null)
                }
            }

            session?.let { session ->
                session.settings.requestInterceptor?.let { interceptor ->
                    interceptor.onLoadRequest(
                        session, request.url.toString()
                    )?.apply {
                        return WebResourceResponse(mimeType, encoding, data.byteInputStream())
                    }
                }
            }

            return super.shouldInterceptRequest(view, request)
        }
    }

    internal fun createWebChromeClient() = object : WebChromeClient() {
        override fun onProgressChanged(view: WebView?, newProgress: Int) {
            session?.internalNotifyObservers { onProgress(newProgress) }
        }

        override fun onReceivedTitle(view: WebView, title: String?) {
            session?.internalNotifyObservers { onTitleChange(title ?: "") }
        }

        override fun onShowCustomView(view: View, callback: CustomViewCallback) {
            addFullScreenView(view, callback)
            session?.internalNotifyObservers { onFullScreenChange(true) }
        }

        override fun onHideCustomView() {
            removeFullScreenView()
            session?.internalNotifyObservers { onFullScreenChange(false) }
        }
    }

    internal fun createDownloadListener(): DownloadListener {
        return DownloadListener { url, userAgent, contentDisposition, mimetype, contentLength ->
            session?.internalNotifyObservers {
                val fileName = DownloadUtils.guessFileName(contentDisposition, url, mimetype)
                val cookie = CookieManager.getInstance().getCookie(url)
                onExternalResource(url, fileName, contentLength, mimetype, cookie, userAgent)
            }
        }
    }

    internal fun createFindListener(): WebView.FindListener {
        return WebView.FindListener { activeMatchOrdinal: Int, numberOfMatches: Int, isDoneCounting: Boolean ->
            session?.internalNotifyObservers {
                onFindResult(activeMatchOrdinal, numberOfMatches, isDoneCounting)
            }
        }
    }

    internal fun handleLongClick(type: Int, extra: String): Boolean {
        val result: HitResult? = when (type) {
            EMAIL_TYPE -> {
                HitResult.EMAIL(extra)
            }
            GEO_TYPE -> {
                HitResult.GEO(extra)
            }
            PHONE_TYPE -> {
                HitResult.PHONE(extra)
            }
            IMAGE_TYPE -> {
                HitResult.IMAGE(extra)
            }
            SRC_ANCHOR_TYPE -> {
                HitResult.UNKNOWN(extra)
            }
            SRC_IMAGE_ANCHOR_TYPE -> {
                // HitTestResult.getExtra() contains only the image URL, and not the link
                // URL. Internally, WebView's HitTestData contains both, but they only
                // make it available via requestFocusNodeHref...
                val message = Message()
                message.target = ImageHandler(session)
                currentWebView.requestFocusNodeHref(message)
                null
            }
            else -> null
        }
        result?.let {
            session?.internalNotifyObservers { onLongPress(it) }
            return true
        }
        return false
    }

    internal fun addFullScreenView(view: View, callback: WebChromeClient.CustomViewCallback) {
        val webView = findViewWithTag<WebView>("mosac_system_engine_webview")
        webView?.apply { this.visibility = View.GONE }

        fullScreenCallback = callback

        view.tag = "mosac_system_engine_fullscreen"
        addView(view)
    }

    internal fun removeFullScreenView() {
        val view = findViewWithTag<View>("mosac_system_engine_fullscreen")
        val webView = findViewWithTag<WebView>("mosac_system_engine_webview")
        view?.let {
            fullScreenCallback?.onCustomViewHidden()
            webView?.apply { this.visibility = View.VISIBLE }
            removeView(view)
        }
    }

    class ImageHandler(val session: SystemEngineSession?) : Handler() {
        override fun handleMessage(msg: Message) {
            val url = msg.data.getString("url")
            val src = msg.data.getString("src")

            if (url == null || src == null) {
                throw IllegalStateException("WebView did not supply url or src for image link")
            }

            session?.internalNotifyObservers { onLongPress(HitResult.IMAGE_SRC(src, url)) }
        }
    }

    companion object {
        @Volatile
        internal var URL_MATCHER: UrlMatcher? = null

        @Synchronized
        internal fun getOrCreateUrlMatcher(context: Context): UrlMatcher {
            if (URL_MATCHER == null) {
                URL_MATCHER = UrlMatcher.createMatcher(
                        context,
                        R.raw.domain_blacklist,
                        intArrayOf(R.raw.domain_overrides),
                        R.raw.domain_whitelist)
            }
            return URL_MATCHER as UrlMatcher
        }
    }
}
