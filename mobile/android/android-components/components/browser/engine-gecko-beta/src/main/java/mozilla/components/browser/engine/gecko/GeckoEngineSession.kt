/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

package mozilla.components.browser.engine.gecko

import kotlinx.coroutines.experimental.CompletableDeferred
import kotlinx.coroutines.experimental.runBlocking
import mozilla.components.concept.engine.EngineSession
import mozilla.components.concept.engine.HitResult
import mozilla.components.concept.engine.Settings
import mozilla.components.concept.engine.request.RequestInterceptor
import mozilla.components.support.ktx.kotlin.isEmail
import mozilla.components.support.ktx.kotlin.isGeoLocation
import mozilla.components.support.ktx.kotlin.isPhone
import org.mozilla.gecko.util.ThreadUtils
import org.mozilla.geckoview.GeckoResult
import org.mozilla.geckoview.GeckoRuntime
import org.mozilla.geckoview.GeckoSession
import org.mozilla.geckoview.GeckoSession.ContentDelegate.ELEMENT_TYPE_AUDIO
import org.mozilla.geckoview.GeckoSession.ContentDelegate.ELEMENT_TYPE_IMAGE
import org.mozilla.geckoview.GeckoSession.ContentDelegate.ELEMENT_TYPE_NONE
import org.mozilla.geckoview.GeckoSession.ContentDelegate.ELEMENT_TYPE_VIDEO
import org.mozilla.geckoview.GeckoSessionSettings

/**
 * Gecko-based EngineSession implementation.
 */
@Suppress("TooManyFunctions")
class GeckoEngineSession(
    runtime: GeckoRuntime,
    privateMode: Boolean = false,
    defaultSettings: Settings? = null
) : EngineSession() {

    internal var geckoSession = GeckoSession()

    /**
     * See [EngineSession.settings]
     */
    override val settings: Settings = object : Settings {
        override var requestInterceptor: RequestInterceptor? = null
    }

    private var initialLoad = true

    init {
        defaultSettings?.trackingProtectionPolicy?.let { enableTrackingProtection(it) }
        defaultSettings?.requestInterceptor?.let { settings.requestInterceptor = it }

        geckoSession.settings.setBoolean(GeckoSessionSettings.USE_PRIVATE_MODE, privateMode)
        geckoSession.open(runtime)

        geckoSession.navigationDelegate = createNavigationDelegate()
        geckoSession.progressDelegate = createProgressDelegate()
        geckoSession.contentDelegate = createContentDelegate()
        geckoSession.trackingProtectionDelegate = createTrackingProtectionDelegate()
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
     * GeckoView provides a String representing the entire session state. We
     * store this String using a single Map entry with key GECKO_STATE_KEY.

     * See https://bugzilla.mozilla.org/show_bug.cgi?id=1441810 for
     * discussion on sync vs. async, where a decision was made that
     * callers should provide synchronous wrappers, if needed. In case we're
     * asking for the state when persisting, a separate (independent) thread
     * is used so we're not blocking anything else. In case of calling this
     * method from onPause or similar, we also want a synchronous response.
     */
    override fun saveState(): Map<String, Any> = runBlocking {
        val stateMap = CompletableDeferred<Map<String, Any>>()

        ThreadUtils.sGeckoHandler.post {
            geckoSession.saveState().then({ state ->
                stateMap.complete(mapOf(GECKO_STATE_KEY to state.toString()))
                GeckoResult<Void>()
            }, { throwable ->
                stateMap.completeExceptionally(throwable)
                GeckoResult<Void>()
            })
        }

        stateMap.await()
    }

    /**
     * See [EngineSession.restoreState]
     */
    override fun restoreState(state: Map<String, Any>) {
        if (state.containsKey(GECKO_STATE_KEY)) {
            val sessionState = GeckoSession.SessionState(state[GECKO_STATE_KEY] as String)
            geckoSession.restoreState(sessionState)
        }
    }

    /**
     * See [EngineSession.enableTrackingProtection]
     */
    override fun enableTrackingProtection(policy: TrackingProtectionPolicy) {
        geckoSession.settings.setBoolean(GeckoSessionSettings.USE_TRACKING_PROTECTION, true)
        notifyObservers { onTrackerBlockingEnabledChange(true) }
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
    override fun setDesktopMode(enable: Boolean, reload: Boolean) {
        // no-op (requires v63+)
    }

    /**
     * See [EngineSession.findAll]
     */
    override fun findAll(text: String) {
        notifyObservers { onFind(text) }
        geckoSession.finder.find(text, 0).then { result: GeckoSession.FinderResult? ->
            result?.let {
                notifyObservers { onFindResult(it.current, it.total, true) }
            }
            GeckoResult<Void>()
        }
    }

    /**
     * See [EngineSession.findNext]
     */
    override fun findNext(forward: Boolean) {
        val findFlags = if (forward) 0 else GeckoSession.FINDER_FIND_BACKWARDS
        geckoSession.finder.find(null, findFlags).then { result: GeckoSession.FinderResult? ->
            result?.let {
                notifyObservers { onFindResult(it.current, it.total, true) }
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
     * NavigationDelegate implementation for forwarding callbacks to observers of the session.
     */
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
            uri: String,
            target: Int,
            flags: Int
        ): GeckoResult<Boolean>? {
            val response = settings.requestInterceptor?.onLoadRequest(
                this@GeckoEngineSession,
                uri
            )?.apply {
                loadData(data, mimeType, encoding)
            }

            return GeckoResult.fromValue(response != null)
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
        ): GeckoResult<GeckoSession> {
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
                if (securityInfo != null) {
                    onSecurityChange(securityInfo.isSecure, securityInfo.host, securityInfo.issuerOrganization)
                } else {
                    onSecurityChange(false)
                }
            }
        }

        override fun onPageStart(session: GeckoSession?, url: String?) {
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

    internal fun createContentDelegate() = object : GeckoSession.ContentDelegate {
        override fun onContextMenu(
            session: GeckoSession,
            screenX: Int,
            screenY: Int,
            uri: String?,
            elementType: Int,
            elementSrc: String?
        ) {
            val hitResult = handleLongClick(elementSrc, elementType, uri)
            hitResult?.let {
                notifyObservers { onLongPress(it) }
            }
        }

        override fun onCrash(session: GeckoSession?) = Unit

        override fun onFullScreen(session: GeckoSession, fullScreen: Boolean) = Unit

        override fun onExternalResponse(session: GeckoSession, response: GeckoSession.WebResponseInfo) {
            notifyObservers {
                onExternalResource(
                    url = response.uri,
                    contentLength = response.contentLength,
                    contentType = response.contentType,
                    fileName = response.filename)
            }
        }

        override fun onCloseRequest(session: GeckoSession) = Unit

        override fun onTitleChange(session: GeckoSession, title: String) {
            notifyObservers { onTitleChange(title) }
        }

        override fun onFocusRequest(session: GeckoSession) = Unit
    }

    private fun createTrackingProtectionDelegate() = GeckoSession.TrackingProtectionDelegate {
        session, uri, _ ->
            session?.let { uri?.let { notifyObservers { onTrackerBlocked(it) } } }
    }

    @Suppress("ComplexMethod")
    fun handleLongClick(elementSrc: String?, elementType: Int, uri: String? = null): HitResult? {
        return when (elementType) {
            ELEMENT_TYPE_AUDIO ->
                elementSrc?.let {
                    HitResult.AUDIO(it)
                }
            ELEMENT_TYPE_VIDEO ->
                elementSrc?.let {
                    HitResult.VIDEO(it)
                }
            ELEMENT_TYPE_IMAGE -> {
                when {
                    elementSrc != null && uri != null ->
                        HitResult.IMAGE_SRC(elementSrc, uri)
                    elementSrc != null ->
                        HitResult.IMAGE(elementSrc)
                    else -> HitResult.UNKNOWN("")
                }
            }
            ELEMENT_TYPE_NONE -> {
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

    companion object {
        internal const val PROGRESS_START = 25
        internal const val PROGRESS_STOP = 100
        internal const val GECKO_STATE_KEY = "GECKO_STATE"
        internal const val MOZ_NULL_PRINCIPAL = "moz-nullprincipal:"
        internal const val ABOUT_BLANK = "about:blank"
    }
}
