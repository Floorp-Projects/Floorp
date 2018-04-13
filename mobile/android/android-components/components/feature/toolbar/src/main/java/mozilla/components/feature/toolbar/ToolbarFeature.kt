/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

package mozilla.components.feature.toolbar

import mozilla.components.concept.toolbar.Toolbar
import mozilla.components.browser.session.SessionManager
import mozilla.components.feature.session.SessionUseCases

/**
 * Feature implementation for connecting a toolbar implementation with the session module.
 */
class ToolbarFeature(
    sessionManager: SessionManager,
    loadUrlUseCase: SessionUseCases.LoadUrlUseCase,
    val toolbar: Toolbar
) {
    private val presenter = ToolbarPresenter(sessionManager, toolbar)
    private val interactor = ToolbarInteractor(loadUrlUseCase, toolbar)

    /**
     * Start feature: App is in the foreground.
     */
    fun start() {
        interactor.start()
        presenter.start()
    }

    /**
     * Stop feature: App is in the background.
     */
    fun stop() {
        interactor.stop()
        presenter.stop()
    }
}
