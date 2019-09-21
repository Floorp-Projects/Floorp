/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

package mozilla.components.feature.session

import mozilla.components.browser.session.SelectionAwareSessionObserver
import mozilla.components.browser.session.Session
import mozilla.components.browser.session.SessionManager
import mozilla.components.support.base.feature.BackHandler
import mozilla.components.support.base.feature.LifecycleAwareFeature

/**
 * Feature implementation for handling fullscreen mode (exiting and back button presses).
 */
open class FullScreenFeature(
    sessionManager: SessionManager,
    private val sessionUseCases: SessionUseCases,
    private val sessionId: String? = null,
    private val fullScreenChanged: (Boolean) -> Unit
) : SelectionAwareSessionObserver(sessionManager), LifecycleAwareFeature, BackHandler {

    /**
     * Starts the feature and a observer to listen for fullscreen changes.
     */
    override fun start() {
        observeIdOrSelected(sessionId)
    }

    override fun onFullScreenChanged(session: Session, enabled: Boolean) = fullScreenChanged(enabled)

    /**
     * To be called when the back button is pressed, so that only fullscreen mode closes.
     *
     * @return Returns true if the fullscreen mode was successfully exited; false if no effect was taken.
     */
    override fun onBackPressed(): Boolean {
        activeSession?.let {
            if (it.fullScreenMode) {
                sessionUseCases.exitFullscreen(it)
                return true
            }
        }
        return false
    }
}
