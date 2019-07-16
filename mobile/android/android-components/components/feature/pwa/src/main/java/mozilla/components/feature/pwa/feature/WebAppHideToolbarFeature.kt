/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

package mozilla.components.feature.pwa.feature

import android.net.Uri
import android.view.View
import androidx.core.net.toUri
import androidx.core.view.isGone
import mozilla.components.browser.session.Session
import mozilla.components.browser.session.SessionManager
import mozilla.components.concept.engine.manifest.WebAppManifest
import mozilla.components.support.base.feature.LifecycleAwareFeature

/**
 * Hides a custom tab toolbar for Progressive Web Apps and Trusted Web Activities.
 *
 * When the [Session] is inside a trusted scope, the toolbar will be hidden.
 * Once the [Session] navigates to another scope, the toolbar will be revealed.
 *
 * @param toolbar Toolbar to show or hide.
 * @param sessionId ID of the custom tab session.
 * @param trustedScopes Scopes to hide the toolbar at.
 * Scopes correspond to [WebAppManifest.scope]. They can be a path (PWA) or just an origin (TWA).
 */
class WebAppHideToolbarFeature(
    private val sessionManager: SessionManager,
    private val toolbar: View,
    private val sessionId: String,
    private val trustedScopes: List<Uri>
) : Session.Observer, LifecycleAwareFeature {

    init {
        // Hide the toolbar by default to prevent a flash.
        // If we don't trust any scopes don't bother with hiding it.
        toolbar.isGone = trustedScopes.isNotEmpty()
    }

    /**
     * Hides or reveals the toolbar when the session navigates to a new URL.
     */
    override fun onUrlChanged(session: Session, url: String) {
        toolbar.isGone = isInScope(url.toUri())
    }

    override fun start() {
        if (trustedScopes.isNotEmpty()) {
            sessionManager.findSessionById(sessionId)?.register(this)
        }
    }

    override fun stop() {
        sessionManager.findSessionById(sessionId)?.unregister(this)
    }

    /**
     * Checks that the [target] URL is in scope of the web app.
     *
     * https://www.w3.org/TR/appmanifest/#dfn-within-scope
     */
    private fun isInScope(target: Uri): Boolean {
        val path = target.path.orEmpty()
        return trustedScopes.any { scope ->
            sameOrigin(scope, target) && path.startsWith(scope.path.orEmpty())
        }
    }

    /**
     * Checks that [a] and [b] have the same origin.
     *
     * https://html.spec.whatwg.org/multipage/origin.html#same-origin
     */
    private fun sameOrigin(a: Uri, b: Uri) =
        a.scheme == b.scheme && a.host == b.host && a.port == b.port
}
