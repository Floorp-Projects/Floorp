/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

package mozilla.components.browser.engine.system

import android.content.Context
import android.graphics.Bitmap
import android.net.Uri
import android.util.AttributeSet
import android.webkit.CookieManager
import android.webkit.DownloadListener
import android.webkit.WebChromeClient
import android.webkit.WebResourceRequest
import android.webkit.WebResourceResponse
import android.webkit.WebView
import android.webkit.WebViewClient
import android.widget.FrameLayout
import mozilla.components.concept.engine.EngineSession
import mozilla.components.concept.engine.EngineView
import mozilla.components.support.utils.DownloadUtils
import mozilla.components.support.utils.matcher.UrlMatcher
import java.lang.ref.WeakReference
import java.net.URI

/**
 * WebView-based implementation of EngineView.
 */
class SystemEngineView @JvmOverloads constructor(
    context: Context,
    attrs: AttributeSet? = null,
    defStyleAttr: Int = 0
) : FrameLayout(context, attrs, defStyleAttr), EngineView {
    internal val currentWebView = createWebView(context)
    internal var currentUrl = ""
    private var session: SystemEngineSession? = null

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

        internalSession.scheduledLoad.data?.let {
            currentWebView.loadData(it, internalSession.scheduledLoad.mimeType, "UTF-8")
            internalSession.scheduledLoad = ScheduledLoad()
        }

        internalSession.scheduledLoad.url?.let {
            currentWebView.loadUrl(it)
            internalSession.scheduledLoad = ScheduledLoad()
        }
    }

    private fun createWebView(context: Context): WebView {
        val webView = WebView(context)
        webView.webViewClient = createWebViewClient(webView)
        webView.webChromeClient = createWebChromeClient()
        webView.setDownloadListener(createDownloadListener())
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
