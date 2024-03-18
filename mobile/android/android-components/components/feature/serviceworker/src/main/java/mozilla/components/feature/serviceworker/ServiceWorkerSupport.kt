/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

package mozilla.components.feature.serviceworker

import androidx.annotation.VisibleForTesting
import mozilla.components.browser.state.state.SessionState
import mozilla.components.concept.engine.Engine
import mozilla.components.concept.engine.EngineSession
import mozilla.components.concept.engine.EngineSession.LoadUrlFlags
import mozilla.components.concept.engine.serviceworker.ServiceWorkerDelegate
import mozilla.components.feature.tabs.TabsUseCases.AddNewTabUseCase
import mozilla.components.support.base.log.logger.Logger

/**
 * Add support for all [Engine] registered service workers' events and callbacks.
 * Call [install] to actually start observing them.
 */
object ServiceWorkerSupport {
    @VisibleForTesting internal lateinit var addTabUseCase: AddNewTabUseCase
    private val logger = Logger("ServiceWorkerSupport")

    /**
     * Start observing service workers' events and callbacks.
     *
     * @param engine [Engine] for which to observe service workers events and callbacks.
     * @param addTabUseCase [AddNewTabUseCase] delegate for opening new tabs when requested by service workers.
     */
    fun install(
        engine: Engine,
        addTabUseCase: AddNewTabUseCase,
    ) {
        try {
            engine.registerServiceWorkerDelegate(
                object : ServiceWorkerDelegate {
                    override fun addNewTab(engineSession: EngineSession): Boolean {
                        addTabUseCase(
                            flags = LoadUrlFlags.external(),
                            engineSession = engineSession,
                            source = SessionState.Source.Internal.None,
                        )

                        return true
                    }
                },
            )
        } catch (e: UnsupportedOperationException) {
            logger.error("failed to register a service worker delegate", e)
        }
    }
}
