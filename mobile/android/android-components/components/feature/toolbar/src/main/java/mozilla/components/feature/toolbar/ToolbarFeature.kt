/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

package mozilla.components.feature.toolbar

import mozilla.components.concept.toolbar.Toolbar
import mozilla.components.browser.session.SessionManager

/**
 * Feature implementation for connecting a toolbar implementation with the session module.
 */
class ToolbarFeature(
    sessionManager: SessionManager,
    toolbar: Toolbar
) {
    private val presenter = ToolbarPresenter(sessionManager, toolbar)

    /**
     * Start feature: App is in the foreground.
     */
    fun start() {
        presenter.start()
    }

    /**
     * Stop feature: App is in the background.
     */
    fun stop() {
        presenter.stop()
    }
}
