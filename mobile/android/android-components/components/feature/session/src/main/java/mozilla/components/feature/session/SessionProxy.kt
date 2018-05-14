/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

package mozilla.components.feature.session

import mozilla.components.concept.engine.EngineSession
import mozilla.components.browser.session.Session

/**
 * Proxy class that will subscribe to an engine session and update the browser session object
 * whenever new data is available.
 */
class SessionProxy(
    val session: Session,
    val engineSession: EngineSession
) : EngineSession.Observer {

    init {
        engineSession.register(this)
    }

    override fun onLocationChange(url: String) {
        session.url = url
    }

    override fun onProgress(progress: Int) {
        session.progress = progress
    }

    override fun onLoadingStateChange(loading: Boolean) {
        session.loading = loading
    }

    override fun onNavigationStateChange(canGoBack: Boolean?, canGoForward: Boolean?) {
        canGoBack?.let { session.canGoBack = canGoBack }
        canGoForward?.let { session.canGoForward = canGoForward }
    }
}
