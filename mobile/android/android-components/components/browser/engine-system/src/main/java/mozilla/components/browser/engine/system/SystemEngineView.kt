/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

package mozilla.components.browser.engine.system

import android.annotation.TargetApi
import android.app.Activity
import android.content.Context
import android.content.res.Resources
import android.graphics.Bitmap
import android.graphics.Canvas
import android.net.Uri
import android.net.http.SslError
import android.os.Build
import android.os.Handler
import android.os.Message
import android.util.AttributeSet
import android.view.PixelCopy
import android.view.View
import android.webkit.CookieManager
import android.webkit.DownloadListener
import android.webkit.HttpAuthHandler
import android.webkit.JsPromptResult
import android.webkit.JsResult
import android.webkit.PermissionRequest
import android.webkit.SslErrorHandler
import android.webkit.ValueCallback
import android.webkit.WebChromeClient
import android.webkit.WebChromeClient.FileChooserParams.MODE_OPEN_MULTIPLE
import android.webkit.WebResourceError
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
import androidx.annotation.VisibleForTesting
import androidx.annotation.VisibleForTesting.PRIVATE
import androidx.core.net.toUri
import kotlinx.coroutines.runBlocking
import mozilla.components.browser.engine.system.matcher.UrlMatcher
import mozilla.components.browser.engine.system.permission.SystemPermissionRequest
import mozilla.components.browser.engine.system.window.SystemWindowRequest
import mozilla.components.browser.errorpages.ErrorType
import mozilla.components.concept.engine.EngineSession
import mozilla.components.concept.engine.EngineSession.TrackingProtectionPolicy
import mozilla.components.concept.engine.EngineView
import mozilla.components.concept.engine.HitResult
import mozilla.components.concept.engine.content.blocking.Tracker
import mozilla.components.concept.engine.prompt.PromptRequest
import mozilla.components.concept.engine.request.RequestInterceptor.InterceptionResponse
import mozilla.components.concept.storage.VisitType
import mozilla.components.support.ktx.android.view.getRectWithViewLocation
import mozilla.components.support.utils.DownloadUtils

/**
 * WebView-based implementation of EngineView.
 */
@Suppress("TooManyFunctions")
class SystemEngineView @JvmOverloads constructor(
    context: Context,
    attrs: AttributeSet? = null,
    defStyleAttr: Int = 0
) : FrameLayout(context, attrs, defStyleAttr), EngineView, View.OnLongClickListener {
    @VisibleForTesting(otherwise = PRIVATE)
    internal var session: SystemEngineSession? = null

    /**
     * Render the content of the given session.
     */
    override fun render(session: EngineSession) {
        removeAllViews()

        this.session = session as SystemEngineSession
        (session.webView.parent as? SystemEngineView)?.removeView(session.webView)
        addView(initWebView(session.webView))
    }

    override fun release() {
        this.session = null

        removeAllViews()
    }

    override fun onLongClick(view: View?): Boolean {
        val result = session?.webView?.hitTestResult
        return result?.let { handleLongClick(result.type, result.extra ?: "") } ?: false
    }

    override fun onPause() {
        session?.apply {
            webView.onPause()
            webView.pauseTimers()
        }
    }

    override fun onResume() {
        session?.apply {
            webView.onResume()
            webView.resumeTimers()
        }
    }

    override fun onDestroy() {
        session?.apply {
            // The WebView instance is long-lived, as it's referenced in the
            // engine session. We can't destroy it here since the session
            // might be used with a different engine view instance later.

            // Further, when this engine view gets destroyed, we need to
            // remove/detach the WebView so that engine view's activity context
            // can properly be destroyed and gc'ed. The WebView instances are
            // created with the context provided to the engine (application
            // context) and reference their parent (this engine view). Since
            // we're keeping the engine session (and their WebView) instances
            // in the SessionManager until closed we'd otherwise prevent
            // this engine view and its context from getting gc'ed.
            (webView.parent as? SystemEngineView)?.removeView(webView)
        }
    }

    internal fun initWebView(webView: WebView): WebView {
        webView.tag = "mozac_system_engine_webview"
        webView.webViewClient = createWebViewClient()
        webView.webChromeClient = createWebChromeClient()
        webView.setDownloadListener(createDownloadListener())
        webView.setFindListener(createFindListener())
        return webView
    }

    @Suppress("ComplexMethod", "NestedBlockDepth")
    private fun createWebViewClient() = object : WebViewClient() {
        override fun doUpdateVisitedHistory(view: WebView, url: String, isReload: Boolean) {
            // TODO private browsing not supported for SystemEngine
            // https://github.com/mozilla-mobile/android-components/issues/649
            // Check if the delegate wants this type of url.
            val delegate = session?.settings?.historyTrackingDelegate ?: return

            if (!delegate.shouldStoreUri(url)) {
                return
            }

            val visitType = when (isReload) {
                true -> VisitType.RELOAD
                false -> VisitType.LINK
            }

            runBlocking {
                session?.settings?.historyTrackingDelegate?.onVisited(url, visitType)
            }
        }

        override fun onPageStarted(view: WebView, url: String?, favicon: Bitmap?) {
            url?.let {
                session?.currentUrl = url
                session?.internalNotifyObservers {
                    onLoadingStateChange(true)
                    onLocationChange(it)
                    onNavigationStateChange(view.canGoBack(), view.canGoForward())
                }
            }
        }

        override fun onPageFinished(view: WebView?, url: String?) {
            url?.let {
                val cert = view?.certificate
                session?.internalNotifyObservers {
                    onLocationChange(it)
                    onLoadingStateChange(false)
                    onSecurityChange(
                            secure = cert != null,
                            host = cert?.let { Uri.parse(url).host },
                            issuer = cert?.issuedBy?.oName)
                }
            }
        }

        @Suppress("ReturnCount", "NestedBlockDepth")
        override fun shouldInterceptRequest(view: WebView, request: WebResourceRequest): WebResourceResponse? {
            if (session?.webFontsEnabled == false && UrlMatcher.isWebFont(request.url)) {
                return WebResourceResponse(null, null, null)
            }

            session?.trackingProtectionPolicy?.let {
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
                        getOrCreateUrlMatcher(resources, it).matches(resourceUri, Uri.parse(session?.currentUrl))) {
                    session?.internalNotifyObservers {
                        onTrackerBlocked(
                            Tracker(
                                resourceUri.toString(),
                                emptyList()
                            )
                        )
                    }
                    return WebResourceResponse(null, null, null)
                }
            }

            session?.let { session ->
                session.settings.requestInterceptor?.let { interceptor ->
                    interceptor.onLoadRequest(
                        session, request.url.toString()
                    )?.apply {
                        return when (this) {
                            is InterceptionResponse.Content ->
                                WebResourceResponse(mimeType, encoding, data.byteInputStream())
                            is InterceptionResponse.Url -> {
                                view.post { view.loadUrl(url) }
                                super.shouldInterceptRequest(view, request)
                            }
                        }
                    }
                }
            }

            if (request.isForMainFrame) {
                session?.let {
                    it.notifyObservers {
                        onLoadRequest(request.url.toString(), request.hasGesture(), true)
                    }
                }
            }

            return super.shouldInterceptRequest(view, request)
        }

        override fun onReceivedSslError(view: WebView, handler: SslErrorHandler, error: SslError) {
            handler.cancel()
            session?.let { session ->
                session.settings.requestInterceptor?.onErrorRequest(
                    session,
                    ErrorType.ERROR_SECURITY_SSL,
                    error.url
                )?.apply {
                    view.loadDataWithBaseURL(url, data, mimeType, encoding, null)
                }
            }
        }

        override fun onReceivedError(view: WebView, errorCode: Int, description: String?, failingUrl: String?) {
            session?.let { session ->
                val errorType = SystemEngineSession.webViewErrorToErrorType(errorCode)
                session.settings.requestInterceptor?.onErrorRequest(
                    session,
                    errorType,
                    failingUrl
                )?.apply {
                    view.loadDataWithBaseURL(url ?: failingUrl, data, mimeType, encoding, null)
                }
            }
        }

        @TargetApi(Build.VERSION_CODES.M)
        override fun onReceivedError(view: WebView, request: WebResourceRequest, error: WebResourceError) {
            session?.let { session ->
                if (!request.isForMainFrame) {
                    return
                }
                val errorType = SystemEngineSession.webViewErrorToErrorType(error.errorCode)
                session.settings.requestInterceptor?.onErrorRequest(
                    session,
                    errorType,
                    request.url.toString()
                )?.apply {
                    view.loadDataWithBaseURL(url ?: request.url.toString(), data, mimeType, encoding, null)
                }
            }
        }

        override fun onReceivedHttpAuthRequest(view: WebView, handler: HttpAuthHandler, host: String, realm: String) {
            val session = session ?: return handler.cancel()

            val formattedUrl = session.currentUrl.toUri().let { uri ->
                "${uri.scheme ?: "http"}://${uri.host ?: host}"
            }

            // Trim obnoxiously long realms.
            val trimmedRealm = if (realm.length > MAX_REALM_LENGTH) {
                realm.substring(0, MAX_REALM_LENGTH) + "\u2026"
            } else {
                realm
            }

            val message = if (trimmedRealm.isEmpty()) {
                context.getString(R.string.mozac_browser_engine_system_auth_no_realm_message, formattedUrl)
            } else {
                context.getString(R.string.mozac_browser_engine_system_auth_message, trimmedRealm, formattedUrl)
            }

            val credentials = view.getAuthCredentials(host, realm)
            val userName = credentials.first
            val password = credentials.second

            session.notifyObservers {
                onPromptRequest(
                    PromptRequest.Authentication(
                        "",
                        message,
                        userName,
                        password,
                        PromptRequest.Authentication.Method.HOST,
                        PromptRequest.Authentication.Level.NONE,
                        onConfirm = { user, pass -> handler.proceed(user, pass) },
                        onDismiss = { handler.cancel() }
                    )
                )
            }
        }
    }

    @Suppress("ComplexMethod")
    private fun createWebChromeClient() = object : WebChromeClient() {
        override fun getVisitedHistory(callback: ValueCallback<Array<String>>) {
            // TODO private browsing not supported for SystemEngine
            // https://github.com/mozilla-mobile/android-components/issues/649
            session?.settings?.historyTrackingDelegate?.let {
                runBlocking {
                    callback.onReceiveValue(it.getVisited().toTypedArray())
                }
            }
        }

        override fun onProgressChanged(view: WebView?, newProgress: Int) {
            session?.internalNotifyObservers { onProgress(newProgress) }
        }

        override fun onReceivedTitle(view: WebView, title: String?) {
            val titleOrEmpty = title ?: ""
            // TODO private browsing not supported for SystemEngine
            // https://github.com/mozilla-mobile/android-components/issues/649
            session?.currentUrl?.takeIf { it.isNotEmpty() }?.let { url ->
                session?.settings?.historyTrackingDelegate?.let { delegate ->
                    runBlocking {
                        delegate.onTitleChanged(url, titleOrEmpty)
                    }
                }
            }
            session?.internalNotifyObservers {
                onTitleChange(titleOrEmpty)
                onNavigationStateChange(view.canGoBack(), view.canGoForward())
            }
        }

        override fun onShowCustomView(view: View, callback: CustomViewCallback) {
            addFullScreenView(view, callback)
            session?.internalNotifyObservers { onFullScreenChange(true) }
        }

        override fun onHideCustomView() {
            removeFullScreenView()
            session?.internalNotifyObservers { onFullScreenChange(false) }
        }

        override fun onPermissionRequestCanceled(request: PermissionRequest) {
            session?.internalNotifyObservers { onCancelContentPermissionRequest(SystemPermissionRequest(request)) }
        }

        override fun onPermissionRequest(request: PermissionRequest) {
            session?.internalNotifyObservers { onContentPermissionRequest(SystemPermissionRequest(request)) }
        }

        override fun onJsAlert(view: WebView, url: String?, message: String?, result: JsResult): Boolean {
            val session = session ?: return applyDefaultJsDialogBehavior(result)

            // When an alert is triggered from a iframe, url is equals to about:blank, using currentUrl as a fallback.
            val safeUrl = if (url.isNullOrBlank()) {
                session.currentUrl
            } else {
                if (url.contains("about")) session.currentUrl else url
            }

            val title = context.getString(R.string.mozac_browser_engine_system_alert_title, safeUrl)

            val onDismiss: () -> Unit = {
                result.cancel()
            }

            session.notifyObservers {
                onPromptRequest(
                    PromptRequest.Alert(
                        title,
                        message ?: "",
                        false,
                        onDismiss
                    ) { _ ->
                        result.confirm()
                    })
            }
            return true
        }

        override fun onJsPrompt(
            view: WebView?,
            url: String?,
            message: String?,
            defaultValue: String?,
            result: JsPromptResult
        ): Boolean {
            val session = session ?: return applyDefaultJsDialogBehavior(result)

            val title = context.getString(R.string.mozac_browser_engine_system_alert_title, url ?: session.currentUrl)

            val onDismiss: () -> Unit = {
                result.cancel()
            }

            val onConfirm: (Boolean, String) -> Unit = { _, valueInput ->
                result.confirm(valueInput)
            }

            session.notifyObservers {
                onPromptRequest(
                    PromptRequest.TextPrompt(
                        title,
                        message ?: "",
                        defaultValue ?: "",
                        false,
                        onDismiss,
                        onConfirm
                    )
                )
            }
            return true
        }

        override fun onJsConfirm(view: WebView?, url: String?, message: String?, result: JsResult): Boolean {
            val session = session ?: return applyDefaultJsDialogBehavior(result)
            val title = context.getString(R.string.mozac_browser_engine_system_alert_title, url ?: session.currentUrl)

            val onDismiss: () -> Unit = {
                result.cancel()
            }

            val onConfirmPositiveButton: (Boolean) -> Unit = { _ ->
                result.confirm()
            }

            val onConfirmNegativeButton: (Boolean) -> Unit = { _ ->
                result.cancel()
            }

            session.notifyObservers {
                onPromptRequest(
                    PromptRequest.Confirm(
                        title,
                        message ?: "",
                        false,
                        "",
                        "",
                        "",
                        onConfirmPositiveButton,
                        onConfirmNegativeButton,
                        {},
                        onDismiss
                    )
                )
            }
            return true
        }

        override fun onShowFileChooser(
            webView: WebView?,
            filePathCallback: ValueCallback<Array<Uri>>?,
            fileChooserParams: FileChooserParams?
        ): Boolean {

            var mimeTypes = fileChooserParams?.acceptTypes ?: arrayOf()

            if (mimeTypes.isNotEmpty() && mimeTypes.first().isNullOrEmpty()) {
                mimeTypes = arrayOf()
            }

            val isMultipleFilesSelection = fileChooserParams?.mode == MODE_OPEN_MULTIPLE

            val captureMode = if (fileChooserParams?.isCaptureEnabled == true) {
                PromptRequest.File.FacingMode.ANY
            } else {
                PromptRequest.File.FacingMode.NONE
            }

            val onSelectMultiple: (Context, Array<Uri>) -> Unit = { _, uris ->
                filePathCallback?.onReceiveValue(uris)
            }

            val onSelectSingle: (Context, Uri) -> Unit = { _, uri ->
                filePathCallback?.onReceiveValue(arrayOf(uri))
            }

            val onDismiss: () -> Unit = {
                filePathCallback?.onReceiveValue(null)
            }

            session?.notifyObservers {
                onPromptRequest(
                    PromptRequest.File(
                        mimeTypes,
                        isMultipleFilesSelection,
                        captureMode,
                        onSelectSingle,
                        onSelectMultiple,
                        onDismiss
                    )
                )
            }

            return true
        }

        override fun onCreateWindow(
            view: WebView,
            isDialog: Boolean,
            isUserGesture: Boolean,
            resultMsg: Message?
        ): Boolean {
            session?.internalNotifyObservers {
                val newEngineSession = SystemEngineSession(context, session?.settings)
                onOpenWindowRequest(SystemWindowRequest(
                    view, newEngineSession, NestedWebView(context), isDialog, isUserGesture, resultMsg
                ))
            }
            return true
        }

        override fun onCloseWindow(window: WebView) {
            session?.internalNotifyObservers { onCloseWindowRequest(SystemWindowRequest(window)) }
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
                session?.webView?.requestFocusNodeHref(message)
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
        val webView = findViewWithTag<WebView>("mozac_system_engine_webview")
        val layoutParams = FrameLayout.LayoutParams(
            FrameLayout.LayoutParams.MATCH_PARENT, FrameLayout.LayoutParams.MATCH_PARENT)
        webView?.apply { this.visibility = View.INVISIBLE }

        session?.fullScreenCallback = callback

        view.tag = "mozac_system_engine_fullscreen"
        addView(view, layoutParams)
    }

    internal fun removeFullScreenView() {
        val view = findViewWithTag<View>("mozac_system_engine_fullscreen")
        val webView = findViewWithTag<WebView>("mozac_system_engine_webview")
        view?.let {
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

    override fun setVerticalClipping(clippingHeight: Int) {
        // no-op
    }

    override fun canScrollVerticallyUp() = session?.webView?.canScrollVertically(-1) ?: false

    override fun canScrollVerticallyDown() = session?.webView?.canScrollVertically(1) ?: false

    override fun captureThumbnail(onFinish: (Bitmap?) -> Unit) {
        val webView = session?.webView
        if (webView == null) {
            onFinish(null)
            return
        }

        if (Build.VERSION.SDK_INT < Build.VERSION_CODES.O) {
            createThumbnailUsingDrawingView(webView, onFinish)
        } else {
            createThumbnailUsingPixelCopy(webView, onFinish)
        }
    }

    private fun createThumbnailUsingDrawingView(view: View, onFinish: (Bitmap?) -> Unit) {
        val outBitmap = Bitmap.createBitmap(view.width, view.height, Bitmap.Config.ARGB_8888)
        val canvas = Canvas(outBitmap)
        view.draw(canvas)
        onFinish(outBitmap)
    }

    @TargetApi(Build.VERSION_CODES.O)
    private fun createThumbnailUsingPixelCopy(view: View, onFinish: (Bitmap?) -> Unit) {
        val out = Bitmap.createBitmap(view.width, view.height, Bitmap.Config.ARGB_8888)
        val viewRect = view.getRectWithViewLocation()
        val window = (context as Activity).window

        PixelCopy.request(window, viewRect, out, { copyResult ->
            val result = if (copyResult == PixelCopy.SUCCESS) out else null
            onFinish(result)
        }, handler)
    }

    private fun applyDefaultJsDialogBehavior(result: JsResult?): Boolean {
        result?.cancel()
        return true
    }

    @Suppress("Deprecation")
    private fun WebView.getAuthCredentials(host: String, realm: String): Pair<String, String> {
        val credentials = if (Build.VERSION.SDK_INT >= Build.VERSION_CODES.O) {
            session?.webViewDatabase(context)?.getHttpAuthUsernamePassword(host, realm)
        } else {
            this.getHttpAuthUsernamePassword(host, realm)
        }

        var credentialsPair = "" to ""

        if (!credentials.isNullOrEmpty() && credentials.size == 2) {

            val user = credentials[0] ?: ""
            val pass = credentials[1] ?: ""

            credentialsPair = user to pass
        }
        return credentialsPair
    }

    companion object {

        // Maximum number of successive dialogs before we prompt users to disable dialogs.
        internal const val MAX_SUCCESSIVE_DIALOG_COUNT: Int = 2

        // Minimum time required between dialogs in seconds before enabling the stop dialog.
        internal const val MAX_SUCCESSIVE_DIALOG_SECONDS_LIMIT: Int = 3

        // Maximum realm length to be shown in authentication dialog.
        internal const val MAX_REALM_LENGTH: Int = 50

        // Number of milliseconds in 1 second.
        internal const val SECOND_MS: Int = 1000

        @Volatile
        internal var URL_MATCHER: UrlMatcher? = null

        private val urlMatcherCategoryMap = mapOf(
                UrlMatcher.ADVERTISING to TrackingProtectionPolicy.TrackingCategory.AD,
                UrlMatcher.ANALYTICS to TrackingProtectionPolicy.TrackingCategory.ANALYTICS,
                UrlMatcher.CONTENT to TrackingProtectionPolicy.TrackingCategory.CONTENT,
                UrlMatcher.SOCIAL to TrackingProtectionPolicy.TrackingCategory.SOCIAL
        )

        @Synchronized
        internal fun getOrCreateUrlMatcher(resources: Resources, policy: TrackingProtectionPolicy): UrlMatcher {
            val categories = urlMatcherCategoryMap.filterValues { policy.trackingCategories.contains(it) }.keys

            URL_MATCHER?.setCategoriesEnabled(categories) ?: run {
                URL_MATCHER = UrlMatcher.createMatcher(
                        resources,
                        R.raw.domain_blacklist,
                        intArrayOf(R.raw.domain_overrides),
                        R.raw.domain_whitelist,
                        categories)
                }

            return URL_MATCHER as UrlMatcher
        }
    }
}
