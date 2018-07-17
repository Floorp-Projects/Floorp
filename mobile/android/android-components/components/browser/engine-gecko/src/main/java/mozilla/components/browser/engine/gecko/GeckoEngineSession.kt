/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

package mozilla.components.browser.engine.gecko

import kotlinx.coroutines.experimental.CompletableDeferred
import kotlinx.coroutines.experimental.launch
import kotlinx.coroutines.experimental.runBlocking
import mozilla.components.concept.engine.EngineSession
import org.mozilla.geckoview.GeckoResponse
import org.mozilla.geckoview.GeckoRuntime
import org.mozilla.geckoview.GeckoSession

/**
 * Gecko-based EngineSession implementation.
 */
class GeckoEngineSession(
    runtime: GeckoRuntime
) : EngineSession() {

    internal var geckoSession = GeckoSession()

    private var initialLoad = true

    init {
        geckoSession.open(runtime)

        geckoSession.navigationDelegate = createNavigationDelegate()
        geckoSession.progressDelegate = createProgressDelegate()
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
    @Throws(GeckoEngineException::class)
    override fun saveState(): Map<String, Any> = runBlocking {
        val stateMap = CompletableDeferred<Map<String, Any>>()
        launch {
            geckoSession.saveState { state ->
                if (state != null) {
                    stateMap.complete(mapOf(GECKO_STATE_KEY to state.toString()))
                } else {
                    stateMap.completeExceptionally(GeckoEngineException("Failed to save state"))
                }
            }
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
        override fun onLocationChange(session: GeckoSession?, url: String) {
            // Ignore initial load of about:blank (see https://github.com/mozilla-mobile/android-components/issues/403)
            if (initialLoad && url == ABOUT_BLANK) {
                return
            }
            initialLoad = false

            notifyObservers { onLocationChange(url) }
        }

        override fun onLoadRequest(
            session: GeckoSession?,
            uri: String?,
            target: Int,
            flags: Int,
            response: GeckoResponse<Boolean>
        ) {
            response.respond(false)
        }

        override fun onCanGoForward(session: GeckoSession?, canGoForward: Boolean) {
            notifyObservers { onNavigationStateChange(canGoForward = canGoForward) }
        }

        override fun onCanGoBack(session: GeckoSession?, canGoBack: Boolean) {
            notifyObservers { onNavigationStateChange(canGoBack = canGoBack) }
        }

        override fun onNewSession(
            session: GeckoSession?,
            uri: String?,
            response: GeckoResponse<GeckoSession>
        ) {}
    }

    /**
    * ProgressDelegate implementation for forwarding callbacks to observers of the session.
    */
    private fun createProgressDelegate() = object : GeckoSession.ProgressDelegate {
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

    companion object {
        internal const val PROGRESS_START = 25
        internal const val PROGRESS_STOP = 100
        internal const val GECKO_STATE_KEY = "GECKO_STATE"
        internal const val MOZ_NULL_PRINCIPAL = "moz-nullprincipal:"
        internal const val ABOUT_BLANK = "about:blank"
    }
}
