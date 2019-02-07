/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

package mozilla.components.feature.toolbar

import mozilla.components.concept.toolbar.Toolbar
import mozilla.components.browser.session.SessionManager
import mozilla.components.feature.session.SessionUseCases
import mozilla.components.support.base.feature.BackHandler
import mozilla.components.support.base.feature.LifecycleAwareFeature

/**
 * A function representing the search use case, accepting
 * the search terms as string.
 */
typealias SearchUseCase = (String) -> Unit

/**
 * Feature implementation for connecting a toolbar implementation with the session module.
 */
class ToolbarFeature(
    val toolbar: Toolbar,
    sessionManager: SessionManager,
    loadUrlUseCase: SessionUseCases.LoadUrlUseCase,
    searchUseCase: SearchUseCase? = null,
    sessionId: String? = null
) : LifecycleAwareFeature, BackHandler {
    private val presenter = ToolbarPresenter(toolbar, sessionManager, sessionId)
    private val interactor = ToolbarInteractor(toolbar, loadUrlUseCase, searchUseCase)

    /**
     * Start feature: App is in the foreground.
     */
    override fun start() {
        interactor.start()
        presenter.start()
    }

    /**
     * Handler for back pressed events in activities that use this feature.
     *
     * @return true if the event was handled, otherwise false.
     */
    override fun onBackPressed(): Boolean {
        return toolbar.onBackPressed()
    }

    /**
     * Stop feature: App is in the background.
     */
    override fun stop() {
        interactor.stop()
        presenter.stop()
    }
}
