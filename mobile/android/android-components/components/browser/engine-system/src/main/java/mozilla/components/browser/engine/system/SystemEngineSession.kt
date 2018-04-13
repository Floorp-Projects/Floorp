/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

package mozilla.components.browser.engine.system

import mozilla.components.concept.engine.EngineSession
import java.lang.ref.WeakReference

/**
 * WebView-based EngineSession implementation.
 */
class SystemEngineSession : EngineSession() {
    internal var urlToLoad: String? = null
    internal var view: WeakReference<SystemEngineView>? = null

    /**
     * Load the given URL.
     */
    override fun loadUrl(url: String) {
        val internalView = view?.get()?.currentWebView

        if (internalView == null) {
            // We can't load a URL without a WebView. So let's just remember the URL here until
            // this session gets linked to a WebView. See: EngineView.render(session).
            urlToLoad = url
        } else {
            internalView.loadUrl(url)
        }
    }

    /**
     * Helper method to notify observers from other classes in this package. This is needed as
     * almost everything is implemented by WebView and its listeners. There is no actual concept of
     * a session when using WebView.
     */
    internal fun internalNotifyObservers(block: Observer.() -> Unit) {
        super.notifyObservers(block)
    }
}
