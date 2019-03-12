/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

package mozilla.components.browser.engine.gecko

import android.annotation.SuppressLint
import kotlinx.coroutines.CompletableDeferred
import kotlinx.coroutines.CoroutineScope
import kotlinx.coroutines.Dispatchers
import kotlinx.coroutines.Job
import kotlinx.coroutines.launch
import kotlinx.coroutines.runBlocking
import mozilla.components.browser.engine.gecko.permission.GeckoPermissionRequest
import mozilla.components.browser.engine.gecko.prompt.GeckoPromptDelegate
import mozilla.components.browser.errorpages.ErrorType
import mozilla.components.concept.engine.EngineSession
import mozilla.components.concept.engine.EngineSessionState
import mozilla.components.concept.engine.HitResult
import mozilla.components.concept.engine.Settings
import mozilla.components.concept.engine.history.HistoryTrackingDelegate
import mozilla.components.concept.engine.request.RequestInterceptor
import mozilla.components.concept.engine.request.RequestInterceptor.InterceptionResponse
import mozilla.components.support.ktx.android.util.Base64
import mozilla.components.support.ktx.kotlin.isEmail
import mozilla.components.support.ktx.kotlin.isGeoLocation
import mozilla.components.support.ktx.kotlin.isPhone
import mozilla.components.support.utils.DownloadUtils
import mozilla.components.support.utils.ThreadUtils
import org.mozilla.geckoview.AllowOrDeny
import org.mozilla.geckoview.GeckoResult
import org.mozilla.geckoview.GeckoRuntime
import org.mozilla.geckoview.GeckoSession
import org.mozilla.geckoview.GeckoSession.NavigationDelegate
import org.mozilla.geckoview.GeckoSessionSettings
import org.mozilla.geckoview.WebRequestError
import kotlin.coroutines.CoroutineContext

/**
 * Gecko-based EngineSession implementation.
 */
@Suppress("TooManyFunctions")
class GeckoEngineSession(
    private val runtime: GeckoRuntime,
    private val privateMode: Boolean = false,
    private val defaultSettings: Settings? = null,
    private val geckoSessionProvider: () -> GeckoSession = { GeckoSession() },
    private val context: CoroutineContext = Dispatchers.IO
) : CoroutineScope, EngineSession() {

    internal var geckoSession: GeckoSession = geckoSessionProvider()
    internal var currentUrl: String? = null
    internal var job: Job = Job()

    /**
     * See [EngineSession.settings]
     */
    override val settings: Settings = object : Settings() {
        override var requestInterceptor: RequestInterceptor? = null
        override var historyTrackingDelegate: HistoryTrackingDelegate? = null
        override var userAgentString: String?
            get() = geckoSession.settings.getString(GeckoSessionSettings.USER_AGENT_OVERRIDE)
            set(value) = geckoSession.settings.setString(GeckoSessionSettings.USER_AGENT_OVERRIDE, value)
    }

    private var initialLoad = true

    override val coroutineContext: CoroutineContext
        get() = context + job

    init {
        initGeckoSession()
    }

    /**
     * See [EngineSession.loadUrl]
     */
    override fun loadUrl(url: String) {
        geckoSession.loadUri(url)
    }

    /**
     * See [EngineSession.loadData]
     */
    override fun loadData(data: String, mimeType: String, encoding: String) {
        when (encoding) {
            "base64" -> geckoSession.loadData(data.toByteArray(), mimeType)
            else -> geckoSession.loadString(data, mimeType)
        }
    }

    /**
     * See [EngineSession.stopLoading]
     */
    override fun stopLoading() {
        geckoSession.stop()
    }

    /**
     * See [EngineSession.reload]
     */
    override fun reload() {
        geckoSession.reload()
    }

    /**
     * See [EngineSession.goBack]
     */
    override fun goBack() {
        geckoSession.goBack()
    }

    /**
     * See [EngineSession.goForward]
     */
    override fun goForward() {
        geckoSession.goForward()
    }

    /**
     * See [EngineSession.saveState]
     *
     * See https://bugzilla.mozilla.org/show_bug.cgi?id=1441810 for
     * discussion on sync vs. async, where a decision was made that
     * callers should provide synchronous wrappers, if needed. In case we're
     * asking for the state when persisting, a separate (independent) thread
     * is used so we're not blocking anything else. In case of calling this
     * method from onPause or similar, we also want a synchronous response.
     */
    override fun saveState(): EngineSessionState = runBlocking {
        val state = CompletableDeferred<GeckoEngineSessionState>()

        ThreadUtils.postToBackgroundThread {
            geckoSession.saveState().then({ sessionState ->
                state.complete(GeckoEngineSessionState(sessionState))
                GeckoResult<Void>()
            }, { throwable ->
                state.completeExceptionally(throwable)
                GeckoResult<Void>()
            })
        }

        state.await()
    }

    /**
     * See [EngineSession.restoreState]
     */
    override fun restoreState(state: EngineSessionState) {
        if (state !is GeckoEngineSessionState) {
            throw IllegalStateException("Can only restore from GeckoEngineSessionState")
        }

        if (state.actualState == null) {
            return
        }

        geckoSession.restoreState(state.actualState)
    }

    /**
     * See [EngineSession.enableTrackingProtection]
     */
    override fun enableTrackingProtection(policy: TrackingProtectionPolicy) {
        val enabled = if (privateMode) {
            policy.useForPrivateSessions
        } else {
            policy.useForRegularSessions
        }
        geckoSession.settings.setBoolean(GeckoSessionSettings.USE_TRACKING_PROTECTION, enabled)
        notifyObservers { onTrackerBlockingEnabledChange(enabled) }
    }

    /**
     * See [EngineSession.disableTrackingProtection]
     */
    override fun disableTrackingProtection() {
        geckoSession.settings.setBoolean(GeckoSessionSettings.USE_TRACKING_PROTECTION, false)
        notifyObservers { onTrackerBlockingEnabledChange(false) }
    }

    /**
     * See [EngineSession.settings]
     */
    override fun toggleDesktopMode(enable: Boolean, reload: Boolean) {
        val currentMode = geckoSession.settings.getInt(GeckoSessionSettings.USER_AGENT_MODE)
        val newMode = if (enable) {
            GeckoSessionSettings.USER_AGENT_MODE_DESKTOP
        } else {
            GeckoSessionSettings.USER_AGENT_MODE_MOBILE
        }

        if (newMode != currentMode) {
            geckoSession.settings.setInt(GeckoSessionSettings.USER_AGENT_MODE, newMode)
            notifyObservers { onDesktopModeChange(enable) }
        }

        if (reload) {
            geckoSession.reload()
        }
    }

    /**
     * See [EngineSession.clearData]
     */
    override fun clearData() {
        // API not available yet.
    }

    /**
     * See [EngineSession.findAll]
     */
    override fun findAll(text: String) {
        notifyObservers { onFind(text) }
        geckoSession.finder.find(text, 0).then { result: GeckoSession.FinderResult? ->
            result?.let {
                val activeMatchOrdinal = if (it.current > 0) it.current - 1 else it.current
                notifyObservers { onFindResult(activeMatchOrdinal, it.total, true) }
            }
            GeckoResult<Void>()
        }
    }

    /**
     * See [EngineSession.findNext]
     */
    @SuppressLint("WrongConstant") // FinderFindFlags annotation doesn't include a 0 value.
    override fun findNext(forward: Boolean) {
        val findFlags = if (forward) 0 else GeckoSession.FINDER_FIND_BACKWARDS
        geckoSession.finder.find(null, findFlags).then { result: GeckoSession.FinderResult? ->
            result?.let {
                val activeMatchOrdinal = if (it.current > 0) it.current - 1 else it.current
                notifyObservers { onFindResult(activeMatchOrdinal, it.total, true) }
            }
            GeckoResult<Void>()
        }
    }

    /**
     * See [EngineSession.clearFindMatches]
     */
    override fun clearFindMatches() {
        geckoSession.finder.clear()
    }

    /**
     * See [EngineSession.exitFullScreenMode]
     */
    override fun exitFullScreenMode() {
        geckoSession.exitFullScreen()
    }

    /**
     * See [EngineSession.close].
     */
    override fun close() {
        super.close()
        job.cancel()
        geckoSession.close()
    }

    /**
     * NavigationDelegate implementation for forwarding callbacks to observers of the session.
     */
    @Suppress("ComplexMethod")
    private fun createNavigationDelegate() = object : GeckoSession.NavigationDelegate {
        override fun onLocationChange(session: GeckoSession?, url: String) {
            // Ignore initial load of about:blank (see https://github.com/mozilla-mobile/android-components/issues/403)
            if (initialLoad && url == ABOUT_BLANK) {
                return
            }
            initialLoad = false

            notifyObservers { onLocationChange(url) }
        }

        override fun onLoadRequest(
            session: GeckoSession,
            request: NavigationDelegate.LoadRequest
        ): GeckoResult<AllowOrDeny> {
            // TODO use onNewSession and create window request:
            // https://github.com/mozilla-mobile/android-components/issues/1503
            if (request.target == GeckoSession.NavigationDelegate.TARGET_WINDOW_NEW) {
                geckoSession.loadUri(request.uri)
                return GeckoResult.fromValue(AllowOrDeny.DENY)
            }

            val response = settings.requestInterceptor?.onLoadRequest(
                this@GeckoEngineSession,
                request.uri
            )?.apply {
                when (this) {
                    is InterceptionResponse.Content -> loadData(data, mimeType, encoding)
                    is InterceptionResponse.Url -> loadUrl(url)
                }
            }
            return GeckoResult.fromValue(if (response != null) AllowOrDeny.DENY else AllowOrDeny.ALLOW)
        }

        override fun onCanGoForward(session: GeckoSession?, canGoForward: Boolean) {
            notifyObservers { onNavigationStateChange(canGoForward = canGoForward) }
        }

        override fun onCanGoBack(session: GeckoSession?, canGoBack: Boolean) {
            notifyObservers { onNavigationStateChange(canGoBack = canGoBack) }
        }

        override fun onNewSession(
            session: GeckoSession,
            uri: String
        ): GeckoResult<GeckoSession> = GeckoResult.fromValue(null)

        override fun onLoadError(
            session: GeckoSession?,
            uri: String?,
            error: WebRequestError
        ): GeckoResult<String> {
            settings.requestInterceptor?.onErrorRequest(
                this@GeckoEngineSession,
                geckoErrorToErrorType(error.code),
                uri
            )?.apply {
                return GeckoResult.fromValue(Base64.encodeToUriString(data))
            }
            return GeckoResult.fromValue(null)
        }
    }

    /**
     * ProgressDelegate implementation for forwarding callbacks to observers of the session.
     */
    private fun createProgressDelegate() = object : GeckoSession.ProgressDelegate {
        override fun onProgressChange(session: GeckoSession?, progress: Int) {
            notifyObservers { onProgress(progress) }
        }

        override fun onSecurityChange(
            session: GeckoSession?,
            securityInfo: GeckoSession.ProgressDelegate.SecurityInformation?
        ) {
            // Ignore initial load of about:blank (see https://github.com/mozilla-mobile/android-components/issues/403)
            if (initialLoad && securityInfo?.origin?.startsWith(MOZ_NULL_PRINCIPAL) == true) {
                return
            }

            notifyObservers {
                if (securityInfo == null) {
                    onSecurityChange(false)
                    return@notifyObservers
                }
                onSecurityChange(securityInfo.isSecure, securityInfo.host, securityInfo.issuerOrganization)
            }
        }

        override fun onPageStart(session: GeckoSession?, url: String?) {
            url?.let { currentUrl = it }

            notifyObservers {
                onProgress(PROGRESS_START)
                onLoadingStateChange(true)
            }
        }

        override fun onPageStop(session: GeckoSession?, success: Boolean) {
            if (success) {
                notifyObservers {
                    onProgress(PROGRESS_STOP)
                    onLoadingStateChange(false)
                }
            }
        }
    }

    @Suppress("ComplexMethod")
    internal fun createHistoryDelegate() = object : GeckoSession.HistoryDelegate {
        override fun onVisited(
            session: GeckoSession,
            url: String,
            lastVisitedURL: String?,
            flags: Int
        ): GeckoResult<Boolean>? {
            if (privateMode ||
                (flags and GeckoSession.HistoryDelegate.VISIT_TOP_LEVEL) == 0 ||
                (flags and GeckoSession.HistoryDelegate.VISIT_UNRECOVERABLE_ERROR) != 0) {

                // Don't track visits in private mode, redirects, or error
                // pages, even if they're top-level visits.
                return GeckoResult.fromValue(false)
            }

            val delegate = settings.historyTrackingDelegate ?: return GeckoResult.fromValue(false)

            val isReload = lastVisitedURL?.let { it == url } ?: false
            val result = GeckoResult<Boolean>()
            launch {
                delegate.onVisited(url, isReload)
                result.complete(true)
            }
            return result
        }

        override fun getVisited(
            session: GeckoSession,
            urls: Array<out String>
        ): GeckoResult<BooleanArray>? {
            if (privateMode) {
                return GeckoResult.fromValue(null)
            }

            val delegate = settings.historyTrackingDelegate ?: return GeckoResult.fromValue(null)

            val result = GeckoResult<BooleanArray>()
            launch {
                val visits: List<Boolean>? = delegate.getVisited(urls.toList())
                result.complete(visits?.toBooleanArray())
            }
            return result
        }
    }

    @Suppress("ComplexMethod")
    internal fun createContentDelegate() = object : GeckoSession.ContentDelegate {
        override fun onFirstComposite(session: GeckoSession?) = Unit

        override fun onContextMenu(
            session: GeckoSession,
            screenX: Int,
            screenY: Int,
            element: GeckoSession.ContentDelegate.ContextElement
        ) {
            val hitResult = handleLongClick(element.srcUri, element.type, element.linkUri)
            hitResult?.let {
                notifyObservers { onLongPress(it) }
            }
        }

        override fun onCrash(session: GeckoSession?) {
            geckoSession.close()
            initGeckoSession()
        }

        override fun onFullScreen(session: GeckoSession, fullScreen: Boolean) {
            notifyObservers { onFullScreenChange(fullScreen) }
        }

        override fun onExternalResponse(session: GeckoSession, response: GeckoSession.WebResponseInfo) {
            notifyObservers {
                val fileName = response.filename
                    ?: DownloadUtils.guessFileName("", response.uri, response.contentType)
                onExternalResource(
                        url = response.uri,
                        contentLength = response.contentLength,
                        contentType = response.contentType,
                        fileName = fileName)
            }
        }

        override fun onCloseRequest(session: GeckoSession) = Unit

        override fun onTitleChange(session: GeckoSession, title: String) {
            if (!privateMode) {
                currentUrl?.let { url ->
                    settings.historyTrackingDelegate?.let { delegate ->
                        runBlocking {
                            delegate.onTitleChanged(url, title)
                        }
                    }
                }
            }
            notifyObservers { onTitleChange(title) }
        }

        override fun onFocusRequest(session: GeckoSession) = Unit
    }

    private fun createTrackingProtectionDelegate() = GeckoSession.TrackingProtectionDelegate {
        session, uri, _ ->
            session?.let { uri?.let { notifyObservers { onTrackerBlocked(it) } } }
    }

    private fun createPermissionDelegate() = object : GeckoSession.PermissionDelegate {
        override fun onContentPermissionRequest(
            session: GeckoSession?,
            uri: String?,
            type: Int,
            callback: GeckoSession.PermissionDelegate.Callback
        ) {
            val request = GeckoPermissionRequest.Content(uri ?: "", type, callback)
            notifyObservers { onContentPermissionRequest(request) }
        }

        override fun onMediaPermissionRequest(
            session: GeckoSession?,
            uri: String?,
            video: Array<out GeckoSession.PermissionDelegate.MediaSource>?,
            audio: Array<out GeckoSession.PermissionDelegate.MediaSource>?,
            callback: GeckoSession.PermissionDelegate.MediaCallback
        ) {
            val request = GeckoPermissionRequest.Media(
                    uri ?: "",
                    video?.toList() ?: emptyList(),
                    audio?.toList() ?: emptyList(),
                    callback)
            notifyObservers { onContentPermissionRequest(request) }
        }

        override fun onAndroidPermissionsRequest(
            session: GeckoSession?,
            permissions: Array<out String>?,
            callback: GeckoSession.PermissionDelegate.Callback
        ) {
            val request = GeckoPermissionRequest.App(
                    permissions?.toList() ?: emptyList(),
                    callback)
            notifyObservers { onAppPermissionRequest(request) }
        }
    }

    @Suppress("ComplexMethod")
    fun handleLongClick(elementSrc: String?, elementType: Int, uri: String? = null): HitResult? {
        return when (elementType) {
            GeckoSession.ContentDelegate.ContextElement.TYPE_AUDIO ->
                elementSrc?.let {
                    HitResult.AUDIO(it)
                }
            GeckoSession.ContentDelegate.ContextElement.TYPE_VIDEO ->
                elementSrc?.let {
                    HitResult.VIDEO(it)
                }
            GeckoSession.ContentDelegate.ContextElement.TYPE_IMAGE -> {
                when {
                    elementSrc != null && uri != null ->
                        HitResult.IMAGE_SRC(elementSrc, uri)
                    elementSrc != null ->
                        HitResult.IMAGE(elementSrc)
                    else -> HitResult.UNKNOWN("")
                }
            }
            GeckoSession.ContentDelegate.ContextElement.TYPE_NONE -> {
                elementSrc?.let {
                    when {
                        it.isPhone() -> HitResult.PHONE(it)
                        it.isEmail() -> HitResult.EMAIL(it)
                        it.isGeoLocation() -> HitResult.GEO(it)
                        else -> HitResult.UNKNOWN(it)
                    }
                } ?: uri?.let {
                    HitResult.UNKNOWN(it)
                }
            }
            else -> HitResult.UNKNOWN("")
        }
    }

    private fun initGeckoSession() {
        this.geckoSession = geckoSessionProvider()
        defaultSettings?.trackingProtectionPolicy?.let { enableTrackingProtection(it) }
        defaultSettings?.requestInterceptor?.let { settings.requestInterceptor = it }
        defaultSettings?.historyTrackingDelegate?.let { settings.historyTrackingDelegate = it }
        defaultSettings?.testingModeEnabled?.let {
            geckoSession.settings.setBoolean(GeckoSessionSettings.FULL_ACCESSIBILITY_TREE, it)
        }
        defaultSettings?.userAgentString?.let {
            geckoSession.settings.setString(GeckoSessionSettings.USER_AGENT_OVERRIDE, it)
        }

        geckoSession.settings.setBoolean(GeckoSessionSettings.USE_PRIVATE_MODE, privateMode)
        geckoSession.open(runtime)

        geckoSession.navigationDelegate = createNavigationDelegate()
        geckoSession.progressDelegate = createProgressDelegate()
        geckoSession.contentDelegate = createContentDelegate()
        geckoSession.trackingProtectionDelegate = createTrackingProtectionDelegate()
        geckoSession.permissionDelegate = createPermissionDelegate()
        geckoSession.promptDelegate = GeckoPromptDelegate(this)
        geckoSession.historyDelegate = createHistoryDelegate()
    }

    companion object {
        internal const val PROGRESS_START = 25
        internal const val PROGRESS_STOP = 100
        internal const val GECKO_STATE_KEY = "GECKO_STATE"
        internal const val MOZ_NULL_PRINCIPAL = "moz-nullprincipal:"
        internal const val ABOUT_BLANK = "about:blank"

        /**
         * Provides an ErrorType corresponding to the error code provided.
         */
        @Suppress("ComplexMethod")
        internal fun geckoErrorToErrorType(@WebRequestError.Error errorCode: Int) =
            when (errorCode) {
                WebRequestError.ERROR_UNKNOWN -> ErrorType.UNKNOWN
                WebRequestError.ERROR_SECURITY_SSL -> ErrorType.ERROR_SECURITY_SSL
                WebRequestError.ERROR_SECURITY_BAD_CERT -> ErrorType.ERROR_SECURITY_BAD_CERT
                WebRequestError.ERROR_NET_INTERRUPT -> ErrorType.ERROR_NET_INTERRUPT
                WebRequestError.ERROR_NET_TIMEOUT -> ErrorType.ERROR_NET_TIMEOUT
                WebRequestError.ERROR_CONNECTION_REFUSED -> ErrorType.ERROR_CONNECTION_REFUSED
                WebRequestError.ERROR_UNKNOWN_SOCKET_TYPE -> ErrorType.ERROR_UNKNOWN_SOCKET_TYPE
                WebRequestError.ERROR_REDIRECT_LOOP -> ErrorType.ERROR_REDIRECT_LOOP
                WebRequestError.ERROR_OFFLINE -> ErrorType.ERROR_OFFLINE
                WebRequestError.ERROR_PORT_BLOCKED -> ErrorType.ERROR_PORT_BLOCKED
                WebRequestError.ERROR_NET_RESET -> ErrorType.ERROR_NET_RESET
                WebRequestError.ERROR_UNSAFE_CONTENT_TYPE -> ErrorType.ERROR_UNSAFE_CONTENT_TYPE
                WebRequestError.ERROR_CORRUPTED_CONTENT -> ErrorType.ERROR_CORRUPTED_CONTENT
                WebRequestError.ERROR_CONTENT_CRASHED -> ErrorType.ERROR_CONTENT_CRASHED
                WebRequestError.ERROR_INVALID_CONTENT_ENCODING -> ErrorType.ERROR_INVALID_CONTENT_ENCODING
                WebRequestError.ERROR_UNKNOWN_HOST -> ErrorType.ERROR_UNKNOWN_HOST
                WebRequestError.ERROR_MALFORMED_URI -> ErrorType.ERROR_MALFORMED_URI
                WebRequestError.ERROR_UNKNOWN_PROTOCOL -> ErrorType.ERROR_UNKNOWN_PROTOCOL
                WebRequestError.ERROR_FILE_NOT_FOUND -> ErrorType.ERROR_FILE_NOT_FOUND
                WebRequestError.ERROR_FILE_ACCESS_DENIED -> ErrorType.ERROR_FILE_ACCESS_DENIED
                WebRequestError.ERROR_PROXY_CONNECTION_REFUSED -> ErrorType.ERROR_PROXY_CONNECTION_REFUSED
                WebRequestError.ERROR_UNKNOWN_PROXY_HOST -> ErrorType.ERROR_UNKNOWN_PROXY_HOST
                WebRequestError.ERROR_SAFEBROWSING_MALWARE_URI -> ErrorType.ERROR_SAFEBROWSING_MALWARE_URI
                WebRequestError.ERROR_SAFEBROWSING_UNWANTED_URI -> ErrorType.ERROR_SAFEBROWSING_UNWANTED_URI
                WebRequestError.ERROR_SAFEBROWSING_HARMFUL_URI -> ErrorType.ERROR_SAFEBROWSING_HARMFUL_URI
                WebRequestError.ERROR_SAFEBROWSING_PHISHING_URI -> ErrorType.ERROR_SAFEBROWSING_PHISHING_URI
                else -> ErrorType.UNKNOWN
            }
    }
}
