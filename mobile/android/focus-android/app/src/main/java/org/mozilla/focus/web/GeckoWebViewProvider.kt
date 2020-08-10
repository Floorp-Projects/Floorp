/* -*- Mode: Java; c-basic-offset: 4; tab-width: 4; indent-tabs-mode: nil; -*-
 * This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

package org.mozilla.focus.web

import android.app.Activity
import android.content.Context
import android.content.SharedPreferences
import android.net.Uri
import android.os.Bundle
import android.os.Environment
import android.os.Parcelable
import android.preference.PreferenceManager
import android.text.TextUtils
import android.util.AttributeSet
import android.util.Log
import android.view.View
import android.webkit.WebSettings
import kotlinx.coroutines.CoroutineScope
import kotlinx.coroutines.Dispatchers
import kotlinx.coroutines.Job
import mozilla.components.browser.errorpages.ErrorPages
import mozilla.components.browser.errorpages.ErrorType
import mozilla.components.browser.session.Session
import mozilla.components.lib.crash.handler.CrashHandlerService
import mozilla.components.support.ktx.android.util.Base64
import org.mozilla.focus.R
import org.mozilla.focus.browser.LocalizedContent
import org.mozilla.focus.ext.savedWebViewState
import org.mozilla.focus.gecko.GeckoViewPrompt
import org.mozilla.focus.gecko.NestedGeckoView
import org.mozilla.focus.telemetry.TelemetryWrapper
import org.mozilla.focus.utils.AppConstants
import org.mozilla.focus.utils.IntentUtils
import org.mozilla.focus.utils.Settings
import org.mozilla.focus.utils.UrlUtils
import org.mozilla.focus.webview.SystemWebView
import org.mozilla.geckoview.AllowOrDeny
import org.mozilla.geckoview.ContentBlocking
import org.mozilla.geckoview.GeckoResult
import org.mozilla.geckoview.GeckoRuntime
import org.mozilla.geckoview.GeckoRuntimeSettings
import org.mozilla.geckoview.GeckoSession
import org.mozilla.geckoview.GeckoSession.NavigationDelegate
import org.mozilla.geckoview.GeckoSessionSettings
import org.mozilla.geckoview.SessionFinder
import org.mozilla.geckoview.WebRequestError
import kotlin.coroutines.CoroutineContext

/**
 * WebViewProvider implementation for creating a Gecko based implementation of IWebView.
 */
@Suppress("TooManyFunctions")
class GeckoWebViewProvider : IWebViewProvider {
    override fun preload(context: Context) {
        sendTelemetryEventOnSwitchToGecko(context)
        createGeckoRuntime(context)
    }

    private fun sendTelemetryEventOnSwitchToGecko(context: Context) {
        val settings = Settings.getInstance(context)
        if (!settings.shouldShowFirstrun() && settings.isFirstGeckoRun()) {
            PreferenceManager.getDefaultSharedPreferences(context)
                .edit().putBoolean(PREF_FIRST_GECKO_RUN, false).apply()
            Log.d(javaClass.simpleName, "Sending change to Gecko ping")
            TelemetryWrapper.changeToGeckoEngineEvent()
        }
    }

    override fun create(context: Context, attributeSet: AttributeSet?): View {
        return GeckoWebView(context, attributeSet)
    }

    override fun performCleanup(context: Context) {
        // Nothing: a WebKit work-around.
    }

    override fun performNewBrowserSessionCleanup() {
        // Nothing: a WebKit work-around.
    }

    private fun createGeckoRuntime(context: Context) {
        if (geckoRuntime == null) {
            val runtimeSettingsBuilder = GeckoRuntimeSettings.Builder()
            runtimeSettingsBuilder.contentBlocking(ContentBlocking.Settings.Builder()
                    .safeBrowsing(ContentBlocking.SafeBrowsing.MALWARE or ContentBlocking.SafeBrowsing.PHISHING)
                    .build())
            val contentBlockingBuilder = ContentBlocking.Settings.Builder()
            if (Settings.getInstance(context).shouldUseSafeBrowsing()) {
                contentBlockingBuilder.safeBrowsing(ContentBlocking.SafeBrowsing.MALWARE or
                        ContentBlocking.SafeBrowsing.PHISHING)
            }
            runtimeSettingsBuilder.contentBlocking(contentBlockingBuilder.build())
            runtimeSettingsBuilder.crashHandler(CrashHandlerService::class.java)

            geckoRuntime =
                    GeckoRuntime.create(context.applicationContext, runtimeSettingsBuilder.build())
        }
    }

    override fun requestMobileSite(context: Context, webSettings: WebSettings) {
    }

    override fun requestDesktopSite(webSettings: WebSettings) {
    }

    override fun applyAppSettings(
        context: Context,
        webSettings: WebSettings,
        systemWebView: SystemWebView
    ) {
    }

    override fun disableBlocking(webSettings: WebSettings, systemWebView: SystemWebView) {
    }

    override fun getUABrowserString(existingUAString: String, focusToken: String): String {
        return ""
    }

    @Suppress("LargeClass", "TooManyFunctions")
    class GeckoWebView(context: Context, attrs: AttributeSet?) :
        NestedGeckoView(context, attrs),
        IWebView,
        SharedPreferences.OnSharedPreferenceChangeListener,
        CoroutineScope {
        private var callback: IWebView.Callback? = null
        private var findListener: IFindListener? = null
        private var currentUrl: String = ABOUT_BLANK
        private var canGoBack: Boolean = false
        private var canGoForward: Boolean = false
        private var isSecure: Boolean = false
        private var geckoSession: GeckoSession
        private var webViewTitle: String? = null
        private var isLoadingInternalUrl = false
        private lateinit var finder: SessionFinder
        private var restored = false
        private var job = Job()
        override val coroutineContext: CoroutineContext
            get() = job + Dispatchers.Main

        init {
            PreferenceManager.getDefaultSharedPreferences(context)
                .registerOnSharedPreferenceChangeListener(this)
            geckoSession = createGeckoSession()
            applySettingsAndSetDelegates()
            geckoSession.open(geckoRuntime!!)
            setSession(geckoSession)
        }

        private fun applySettingsAndSetDelegates() {
            applyAppSettings()
            updateBlocking()

            geckoSession.contentDelegate = createContentDelegate()
            geckoSession.progressDelegate = createProgressDelegate()
            geckoSession.navigationDelegate = createNavigationDelegate()
            geckoSession.contentBlockingDelegate = createTrackingProtectionDelegate()
            geckoSession.promptDelegate = createPromptDelegate()
            finder = geckoSession.finder
            finder.displayFlags = GeckoSession.FINDER_DISPLAY_HIGHLIGHT_ALL
        }

        private fun createGeckoSession(): GeckoSession {
            val builder = GeckoSessionSettings.Builder()
            builder.usePrivateMode(true)
            builder.suspendMediaWhenInactive(true)
            return GeckoSession(builder.build())
        }

        override fun setCallback(callback: IWebView.Callback?) {
            this.callback = callback
        }

        override fun onPause() {
            job.cancel()
        }

        override fun goBack() {
            geckoSession.goBack()
        }

        override fun goForward() {
            geckoSession.goForward()
        }

        override fun reload() {
            geckoSession.reload()
        }

        override fun destroy() {
        }

        override fun onResume() {
            if (job.isCancelled) {
                job = Job()
            }
        }

        override fun stopLoading() {
            geckoSession.stop()
            callback?.onPageFinished(isSecure)
        }

        override fun getUrl(): String? {
            return currentUrl
        }

        override fun loadUrl(url: String) {
            currentUrl = url
            geckoSession.loadUri(currentUrl)
        }

        override fun cleanup() {
            if (geckoSession.isOpen) {
                geckoSession.close()
            }
        }

        override fun setBlockingEnabled(enabled: Boolean) {
            geckoSession.settings.useTrackingProtection = enabled
            if (enabled) {
                updateBlocking()
                applyAppSettings()
            } else {
                geckoRuntime!!.settings.javaScriptEnabled = true
                geckoRuntime!!.settings.webFontsEnabled = true
                geckoRuntime!!.settings.contentBlocking.cookieBehavior = ContentBlocking.CookieBehavior.ACCEPT_ALL
            }
            callback?.onBlockingStateChanged(enabled)
        }

        override fun setRequestDesktop(shouldRequestDesktop: Boolean) {
            geckoSession.settings.userAgentMode =
                    if (shouldRequestDesktop) GeckoSessionSettings.USER_AGENT_MODE_DESKTOP
                    else GeckoSessionSettings.USER_AGENT_MODE_MOBILE
            geckoSession.settings.viewportMode =
                    if (shouldRequestDesktop) GeckoSessionSettings.VIEWPORT_MODE_DESKTOP
                    else GeckoSessionSettings.VIEWPORT_MODE_MOBILE
            callback?.onRequestDesktopStateChanged(shouldRequestDesktop)
        }

        @Suppress("ComplexMethod")
        override fun onSharedPreferenceChanged(
            sharedPreferences: SharedPreferences,
            key: String
        ) {
            when (key) {
                context.getString(R.string.pref_key_privacy_block_social),
                context.getString(R.string.pref_key_privacy_block_ads),
                context.getString(R.string.pref_key_privacy_block_analytics),
                context.getString(R.string.pref_key_privacy_block_other) -> updateBlocking()
                context.getString(R.string.pref_key_performance_block_javascript)
                -> geckoRuntime!!.settings.javaScriptEnabled =
                        !Settings.getInstance(context).shouldBlockJavaScript()
                context.getString(R.string.pref_key_performance_block_webfonts)
                -> geckoRuntime!!.settings.webFontsEnabled =
                        !Settings.getInstance(context).shouldBlockWebFonts()
                context.getString(R.string.pref_key_remote_debugging) ->
                    geckoRuntime!!.settings.remoteDebuggingEnabled =
                            Settings.getInstance(context).shouldEnableRemoteDebugging()
                context.getString(R.string.pref_key_performance_enable_cookies) -> {
                    updateCookieSettings()
                }
                context.getString(R.string.pref_key_safe_browsing) -> {
                    val shouldUseSafeBrowsing =
                        Settings.getInstance(context).shouldUseSafeBrowsing()
                    var cats = geckoRuntime!!.settings.contentBlocking.safeBrowsingCategories
                    if (shouldUseSafeBrowsing) {
                        cats = cats or ContentBlocking.SafeBrowsing.MALWARE or
                                ContentBlocking.SafeBrowsing.PHISHING
                    } else {
                        cats = cats and ContentBlocking.SafeBrowsing.MALWARE.inv() and
                                ContentBlocking.SafeBrowsing.PHISHING.inv()
                    }
                    geckoRuntime!!.settings.contentBlocking.setSafeBrowsing(cats)
                }
                else -> return
            }
            reload()
        }

        private fun applyAppSettings() {
            geckoRuntime!!.settings.javaScriptEnabled =
                    !Settings.getInstance(context).shouldBlockJavaScript()
            geckoRuntime!!.settings.webFontsEnabled =
                    !Settings.getInstance(context).shouldBlockWebFonts()
            geckoRuntime!!.settings.remoteDebuggingEnabled = false
            updateCookieSettings()
        }

        private fun updateCookieSettings() {
            geckoRuntime!!.settings.contentBlocking.cookieBehavior =
                    when (Settings.getInstance(context).shouldBlockCookiesValue()) {
                        context.getString(
                            R.string.preference_privacy_should_block_cookies_yes_option
                        ) ->
                            ContentBlocking.CookieBehavior.ACCEPT_NONE
                        context.getString(
                            R.string.preference_privacy_should_block_cookies_third_party_tracker_cookies_option
                        ) ->
                            ContentBlocking.CookieBehavior.ACCEPT_NON_TRACKERS
                        context.getString(
                            R.string.preference_privacy_should_block_cookies_third_party_only_option
                        ) ->
                            ContentBlocking.CookieBehavior.ACCEPT_FIRST_PARTY
                        else -> ContentBlocking.CookieBehavior.ACCEPT_ALL
                    }
        }

        private fun updateBlocking() {
            val settings = Settings.getInstance(context)

            var categories = geckoRuntime!!.settings.contentBlocking.antiTrackingCategories
            if (settings.shouldBlockSocialTrackers()) {
                categories = categories or ContentBlocking.AntiTracking.SOCIAL
            } else {
                categories = categories and ContentBlocking.AntiTracking.SOCIAL.inv()
            }
            if (settings.shouldBlockAdTrackers()) {
                categories = categories or ContentBlocking.AntiTracking.AD
            } else {
                categories = categories and ContentBlocking.AntiTracking.AD.inv()
            }
            if (settings.shouldBlockAnalyticTrackers()) {
                categories = categories or ContentBlocking.AntiTracking.ANALYTIC
            } else {
                categories = categories and ContentBlocking.AntiTracking.ANALYTIC.inv()
            }
            if (settings.shouldBlockOtherTrackers()) {
                categories = categories or ContentBlocking.AntiTracking.CONTENT
            } else {
                categories = categories and ContentBlocking.AntiTracking.CONTENT.inv()
            }

            geckoRuntime!!.settings.contentBlocking.setAntiTracking(categories)
        }

        @Suppress("ComplexMethod", "ReturnCount")
        private fun createContentDelegate(): GeckoSession.ContentDelegate {
            return object : GeckoSession.ContentDelegate {
                override fun onTitleChange(session: GeckoSession, title: String?) {
                    webViewTitle = title
                    callback?.onTitleChanged(title)
                }

                override fun onFullScreen(session: GeckoSession, fullScreen: Boolean) {
                    if (fullScreen) {
                        callback?.onEnterFullScreen({ geckoSession.exitFullScreen() }, null)
                    } else {
                        callback?.onExitFullScreen()
                    }
                }

                override fun onContextMenu(
                    session: GeckoSession,
                    screenX: Int,
                    screenY: Int,
                    contextElement: GeckoSession.ContentDelegate.ContextElement
                ) {
                    val elementSrc = contextElement.srcUri
                    val uri = contextElement.linkUri
                    val elementType = contextElement.type
                    if (elementSrc != null && uri != null &&
                        elementType == GeckoSession.ContentDelegate.ContextElement.TYPE_IMAGE
                    ) {
                        callback?.onLongPress(IWebView.HitTarget(true, uri, true, elementSrc))
                    } else if (elementSrc != null &&
                        elementType == GeckoSession.ContentDelegate.ContextElement.TYPE_IMAGE) {
                        callback?.onLongPress(IWebView.HitTarget(false, null, true, elementSrc))
                    } else if (uri != null) {
                        callback?.onLongPress(IWebView.HitTarget(true, uri, false, null))
                    }
                }

                override fun onExternalResponse(
                    session: GeckoSession,
                    response: GeckoSession.WebResponseInfo
                ) {
                    if (!AppConstants.supportsDownloadingFiles()) {
                        return
                    }

                    val scheme = Uri.parse(response.uri).scheme
                    if (scheme == null || scheme != "http" && scheme != "https") {
                        // We are ignoring everything that is not http or https. This is a limitation of
                        // Android's download manager. There's no reason to show a download dialog for
                        // something we can't download anyways.
                        Log.w(TAG, "Ignoring download from non http(s) URL: " + response.uri)
                        return
                    }

                    val download = Download(
                        response.uri, USER_AGENT,
                        response.filename, response.contentType, response.contentLength,
                        Environment.DIRECTORY_DOWNLOADS, response.filename
                    )
                    callback?.onDownloadStart(download)
                }

                override fun onCrash(session: GeckoSession) {
                    geckoSession.close()
                    geckoSession = createGeckoSession()
                    applySettingsAndSetDelegates()
                    geckoSession.open(geckoRuntime!!)
                    setSession(geckoSession)
                    currentUrl = ABOUT_BLANK
                    geckoSession.loadUri(currentUrl)
                }

                override fun onFocusRequest(geckoSession: GeckoSession) {}

                override fun onCloseRequest(geckoSession: GeckoSession) {
                    // Ignore this callback
                }

                override fun onFirstComposite(geckoSession: GeckoSession) {}
            }
        }

        @Suppress("ComplexMethod")
        private fun createProgressDelegate(): GeckoSession.ProgressDelegate {
            return object : GeckoSession.ProgressDelegate {
                override fun onProgressChange(session: GeckoSession, progress: Int) {
                    if (progress == PROGRESS_100) {
                        if (UrlUtils.isLocalizedContent(url)) {
                            // When the url is a localized content, then the page is secure
                            isSecure = true
                        }
                        callback?.onPageFinished(isSecure)
                    } else {
                        callback?.onProgress(progress)
                    }
                }

                override fun onPageStart(session: GeckoSession, url: String) {
                    callback?.onPageStarted(url)
                    callback?.resetBlockedTrackers()
                    isSecure = false
                }

                override fun onPageStop(session: GeckoSession, success: Boolean) {
                    if (success) {
                        if (UrlUtils.isLocalizedContent(url)) {
                            // When the url is a localized content, then the page is secure
                            isSecure = true
                        }

                        callback?.onPageFinished(isSecure)
                    }
                }

                override fun onSecurityChange(
                    session: GeckoSession,
                    securityInfo: GeckoSession.ProgressDelegate.SecurityInformation
                ) {
                    isSecure = securityInfo.isSecure

                    if (UrlUtils.isLocalizedContent(url)) {
                        // When the url is a localized content, then the page is secure
                        isSecure = true
                    }

                    callback?.onSecurityChanged(
                        isSecure,
                        securityInfo.host,
                        securityInfo.certificate?.issuerDN?.name
                    )
                }
            }
        }

        @Suppress("ComplexMethod")
        private fun createNavigationDelegate(): GeckoSession.NavigationDelegate {
            return object : GeckoSession.NavigationDelegate {
                override fun onLoadRequest(
                    session: GeckoSession,
                    request: NavigationDelegate.LoadRequest
                ): GeckoResult<AllowOrDeny>? {
                    val uri = Uri.parse(request.uri)

                    val complete = when {
                        request.target == GeckoSession.NavigationDelegate.TARGET_WINDOW_NEW -> {
                            geckoSession.loadUri(request.uri)
                            AllowOrDeny.DENY
                        }
                        LocalizedContent.handleInternalContent(
                            request.uri,
                            this@GeckoWebView,
                            context
                        ) -> {
                            AllowOrDeny.DENY
                        }
                        !UrlUtils.isSupportedProtocol(uri.scheme) && callback != null &&
                                IntentUtils.handleExternalUri(
                                    context,
                                    this@GeckoWebView,
                                    request.uri
                                ) -> {
                            AllowOrDeny.DENY
                        }
                        else -> {
                            callback?.onRequest(!request.isRedirect)
                            AllowOrDeny.ALLOW
                        }
                    }

                    return GeckoResult.fromValue(complete)
                }

                override fun onLoadError(
                    session: GeckoSession,
                    uri: String?,
                    webRequestError: WebRequestError
                ): GeckoResult<String> {
                    ErrorPages.createErrorPage(
                        context,
                        geckoErrorToErrorType(webRequestError.code),
                        uri
                    ).apply {
                        return GeckoResult.fromValue(Base64.encodeToUriString(this))
                    }
                }

                override fun onNewSession(
                    session: GeckoSession,
                    uri: String
                ): GeckoResult<GeckoSession>? {
                    // Prevent new sessions to be created from onLoadRequest
                    throw IllegalStateException()
                }

                override fun onLocationChange(session: GeckoSession, url: String?) {
                    var desiredUrl = url
                    // Save internal data: urls we should override to present focus:about, focus:rights
                    if (isLoadingInternalUrl) {
                        if (currentUrl == LocalizedContent.URL_ABOUT) {
                            internalAboutData = desiredUrl
                        } else if (currentUrl == LocalizedContent.URL_RIGHTS) {
                            internalRightsData = desiredUrl
                        }
                        isLoadingInternalUrl = false
                        desiredUrl = currentUrl
                    }

                    // Check for internal data: urls to instead present focus:rights, focus:about
                    if (!TextUtils.isEmpty(internalAboutData) && internalAboutData == desiredUrl) {
                        desiredUrl = LocalizedContent.URL_ABOUT
                    } else if (!TextUtils.isEmpty(internalRightsData) && internalRightsData == desiredUrl) {
                        desiredUrl = LocalizedContent.URL_RIGHTS
                    }

                    currentUrl = desiredUrl!!
                    callback?.onURLChanged(desiredUrl)
                }

                override fun onCanGoBack(session: GeckoSession, canGoBack: Boolean) {
                    this@GeckoWebView.canGoBack = canGoBack
                }

                override fun onCanGoForward(session: GeckoSession, canGoForward: Boolean) {
                    this@GeckoWebView.canGoForward = canGoForward
                }
            }
        }

        private fun createTrackingProtectionDelegate(): ContentBlocking.Delegate {
            return object : ContentBlocking.Delegate {
                override fun onContentBlocked(session: GeckoSession, event: ContentBlocking.BlockEvent) {
                    callback?.countBlockedTracker()
                }
            }
        }

        private fun createPromptDelegate(): GeckoSession.PromptDelegate {
            return GeckoViewPrompt(context as Activity)
        }

        override fun canGoForward(): Boolean {
            return canGoForward
        }

        override fun canGoBack(): Boolean {
            return canGoBack
        }

        override fun restoreWebViewState(session: Session) {
            val stateData = session.savedWebViewState!!
            val savedSession = stateData.getParcelable<GeckoSession>(GECKO_SESSION)!!

            if (geckoSession != savedSession && !restored) {
                // Tab changed, we need to close the default session and restore our saved session
                geckoSession.close()

                geckoSession = savedSession
                canGoBack = stateData.getBoolean(CAN_GO_BACK, false)
                canGoForward = stateData.getBoolean(CAN_GO_FORWARD, false)
                isSecure = stateData.getBoolean(IS_SECURE, false)
                webViewTitle = stateData.getString(WEBVIEW_TITLE, null)
                currentUrl = stateData.getString(CURRENT_URL, ABOUT_BLANK)
                applySettingsAndSetDelegates()
                if (!geckoSession.isOpen) {
                    geckoSession.open(geckoRuntime!!)
                }
                setSession(geckoSession)
            } else {
                // App was backgrounded and restored;
                // GV restored the GeckoSession itself, but we need to restore our variables
                canGoBack = stateData.getBoolean(CAN_GO_BACK, false)
                canGoForward = stateData.getBoolean(CAN_GO_FORWARD, false)
                isSecure = stateData.getBoolean(IS_SECURE, false)
                webViewTitle = stateData.getString(WEBVIEW_TITLE, null)
                currentUrl = stateData.getString(CURRENT_URL, ABOUT_BLANK)
                applySettingsAndSetDelegates()
                restored = false
            }
        }

        override fun onRestoreInstanceState(state: Parcelable?) {
            if (geckoSession.isOpen) {
                geckoSession.close()
            }
            restored = true
            super.onRestoreInstanceState(state)
        }

        override fun saveWebViewState(session: Session) {
            val sessionBundle = Bundle()
            sessionBundle.putParcelable(GECKO_SESSION, geckoSession)
            sessionBundle.putBoolean(CAN_GO_BACK, canGoBack)
            sessionBundle.putBoolean(CAN_GO_FORWARD, canGoForward)
            sessionBundle.putBoolean(IS_SECURE, isSecure)
            sessionBundle.putString(WEBVIEW_TITLE, webViewTitle)
            sessionBundle.putString(CURRENT_URL, currentUrl)
            session.savedWebViewState = sessionBundle
        }

        override fun getTitle(): String? {
            return webViewTitle
        }

        override fun exitFullscreen() {
            geckoSession.exitFullScreen()
        }

        override fun findAllAsync(find: String) {
            finder.find(find, 0).then({ result ->
                if (result != null) {
                    findListener?.onFindResultReceived(result.current, result.total, true)
                }
                GeckoResult<Void>()
            }, { _ ->
                GeckoResult<Void>()
            })
        }

        override fun findNext(forward: Boolean) {
            finder.find(null, if (forward) 0 else GeckoSession.FINDER_FIND_BACKWARDS)
                .then({ result ->
                    if (result != null) {
                        findListener?.onFindResultReceived(result.current, result.total, true)
                    }
                    GeckoResult<Void>()
                }, { _ ->
                    GeckoResult<Void>()
                })
        }

        override fun clearMatches() {
            finder.clear()
        }

        override fun setFindListener(findListener: IFindListener) {
            this.findListener = findListener
        }

        override fun loadData(
            baseURL: String,
            data: String,
            mimeType: String,
            encoding: String,
            historyURL: String
        ) {
            isLoadingInternalUrl = historyURL == LocalizedContent.URL_RIGHTS || historyURL ==
                    LocalizedContent.URL_ABOUT
            geckoSession.loadData(data.toByteArray(Charsets.UTF_8), mimeType)
            currentUrl = historyURL
        }

        override fun releaseGeckoSession() {
            releaseSession()
        }

        override fun onDetachedFromWindow() {
            PreferenceManager.getDefaultSharedPreferences(context)
                .unregisterOnSharedPreferenceChangeListener(this)
            releaseSession()
            super.onDetachedFromWindow()
        }

        companion object {
            private const val TAG = "GeckoWebView"
        }
    }

    companion object {
        @Volatile
        private var geckoRuntime: GeckoRuntime? = null
        private var internalAboutData: String? = null
        private var internalRightsData: String? = null
        private const val USER_AGENT =
            "Mozilla/5.0 (Android 8.1.0; Mobile; rv:60.0) Gecko/60.0 Firefox/60.0"
        const val PREF_FIRST_GECKO_RUN: String = "first_gecko_run"
        const val PROGRESS_100 = 100
        const val CAN_GO_BACK = "canGoBack"
        const val CAN_GO_FORWARD = "canGoForward"
        const val GECKO_SESSION = "geckoSession"
        const val IS_SECURE = "isSecure"
        const val WEBVIEW_TITLE = "webViewTitle"
        const val CURRENT_URL = "currentUrl"
        const val ABOUT_BLANK = "about:blank"

        /**
         * Provides an ErrorType corresponding to the error code provided.
         */
        @Suppress("ComplexMethod")
        internal fun geckoErrorToErrorType(errorCode: Int) =
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
