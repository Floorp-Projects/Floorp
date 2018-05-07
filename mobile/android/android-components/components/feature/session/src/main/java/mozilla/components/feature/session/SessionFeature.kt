/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

package mozilla.components.feature.session

import mozilla.components.concept.engine.Engine
import mozilla.components.concept.engine.EngineView

/**
 * Feature implementation for connecting the engine module with the session module.
 */
class SessionFeature(
    private val sessionProvider: SessionProvider,
    private val sessionUseCases: SessionUseCases,
    private val engine: Engine,
    engineView: EngineView
) {
    internal val presenter = EngineViewPresenter(sessionProvider, engine, engineView)

    /**
     * Start feature: App is in the foreground.
     */
    fun start() {
        presenter.start()
        sessionProvider.start(engine)
    }

    /**
     * Handler for back pressed events in activities that use this feature.
     *
     * @return true if the event was handled, otherwise false.
     */
    fun handleBackPressed(): Boolean {
        if (sessionProvider.selectedSession.canGoBack) {
            sessionUseCases.goBack.invoke()
            return true
        }

        return false
    }

    /**
     * Stop feature: App is in the background.
     */
    fun stop() {
        presenter.stop()
        sessionProvider.stop()
    }
}
