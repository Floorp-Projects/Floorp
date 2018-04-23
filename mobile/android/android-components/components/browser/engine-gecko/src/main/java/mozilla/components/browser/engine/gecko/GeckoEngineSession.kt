/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

package mozilla.components.browser.engine.gecko

import mozilla.components.concept.engine.EngineSession
import org.mozilla.geckoview.GeckoRuntime
import org.mozilla.geckoview.GeckoSession

const val PROGRESS_START = 25
const val PROGRESS_STOP = 100

/**
 * Gecko-based EngineSession implementation.
 */
class GeckoEngineSession(
    runtime: GeckoRuntime
) : EngineSession() {
    internal val geckoSession = GeckoSession()

    init {
        geckoSession.open(runtime)

        geckoSession.navigationDelegate = createNavigationDelegate()
        geckoSession.progressDelegate = createProgressDelegate()
    }

    /**
     * Load the given URL.
     */
    override fun loadUrl(url: String) {
        geckoSession.loadUri(url)
    }

    /**
     * NavigationDelegate implementation for forwarding callbacks to observers of the session.
     */
    private fun createNavigationDelegate() = object : GeckoSession.NavigationDelegate {
        override fun onLocationChange(session: GeckoSession?, url: String) {
            notifyObservers { onLocationChange(url) }
        }

        override fun onLoadRequest(
            session: GeckoSession?,
            uri: String?,
            target: Int,
            response: GeckoSession.Response<Boolean>
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
            response: GeckoSession.Response<GeckoSession>?
        ) {}
    }

    /**
    * ProgressDelegate implementation for forwarding callbacks to observers of the session.
    */
    private fun createProgressDelegate() = object : GeckoSession.ProgressDelegate {
        override fun onSecurityChange(
            session: GeckoSession?,
            securityInfo: GeckoSession.ProgressDelegate.SecurityInformation?
        ) { }

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
}
