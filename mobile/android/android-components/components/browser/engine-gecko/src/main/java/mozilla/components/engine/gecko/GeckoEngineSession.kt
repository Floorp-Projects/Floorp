/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

package mozilla.components.engine.gecko

import mozilla.components.engine.EngineSession
import org.mozilla.geckoview.GeckoRuntime
import org.mozilla.geckoview.GeckoSession

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

        override fun onCanGoForward(session: GeckoSession?, canGoForward: Boolean) {}

        override fun onCanGoBack(session: GeckoSession?, canGoBack: Boolean) {}

        override fun onNewSession(
            session: GeckoSession?,
            uri: String?,
            response: GeckoSession.Response<GeckoSession>?
        ) {}
    }
}
