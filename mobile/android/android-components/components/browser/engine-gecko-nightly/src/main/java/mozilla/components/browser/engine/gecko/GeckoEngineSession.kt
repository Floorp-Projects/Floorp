/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

package mozilla.components.browser.engine.gecko

import kotlinx.coroutines.experimental.CompletableDeferred
import kotlinx.coroutines.experimental.runBlocking
import mozilla.components.concept.engine.EngineSession
import org.mozilla.gecko.util.ThreadUtils
import org.mozilla.geckoview.GeckoResult
import org.mozilla.geckoview.GeckoRuntime
import org.mozilla.geckoview.GeckoSession
import org.mozilla.geckoview.GeckoSessionSettings

/**
 * Gecko-based EngineSession implementation.
 */
@Suppress("TooManyFunctions")
class GeckoEngineSession(
    private val runtime: GeckoRuntime
) : EngineSession() {

    internal var geckoSession = GeckoSession()

    private var initialLoad = true

    init {
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
     * NavigationDelegate implementation for forwarding callbacks to observers of the session.
     */
    private fun createNavigationDelegate() = object : GeckoSession.NavigationDelegate {
        override fun onLoadError(session: GeckoSession?, uri: String?, category: Int, error: Int) = Unit

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
            return GeckoResult.fromValue(false)
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

    private fun createContentDelegate() = object : GeckoSession.ContentDelegate {
        override fun onContextMenu(
            session: GeckoSession,
            screenX: Int,
            screenY: Int,
            uri: String,
            elementType: Int,
            elementSrc: String
        ) = Unit

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

    override fun enableTrackingProtection(policy: TrackingProtectionPolicy) {
        geckoSession.settings.setBoolean(GeckoSessionSettings.USE_TRACKING_PROTECTION, true)
        runtime.settings.trackingProtectionCategories = policy.categories
        notifyObservers { onTrackerBlockingEnabledChange(true) }
    }

    override fun disableTrackingProtection() {
        geckoSession.settings.setBoolean(GeckoSessionSettings.USE_TRACKING_PROTECTION, false)
        runtime.settings.trackingProtectionCategories = TrackingProtectionPolicy.none().categories
        notifyObservers { onTrackerBlockingEnabledChange(false) }
    }

    companion object {
        internal const val PROGRESS_START = 25
        internal const val PROGRESS_STOP = 100
        internal const val GECKO_STATE_KEY = "GECKO_STATE"
        internal const val MOZ_NULL_PRINCIPAL = "moz-nullprincipal:"
        internal const val ABOUT_BLANK = "about:blank"
    }
}
