/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

package mozilla.components.browser.session.usecases

import mozilla.components.browser.session.SessionManager
import mozilla.components.concept.engine.EngineSession

/**
 * UseCases for getting and creating [EngineSession] instances for tabs.
 *
 * This class and its use cases are an interim solution in order to migrate components away from
 * using SessionManager for getting and creating [EngineSession] instances.
 */
class EngineSessionUseCases(
    sessionManager: SessionManager
) {
    /**
     * Use case for getting or creating an [EngineSession] for a tab.
     */
    class GetOrCreateUseCase internal constructor(
        private val sessionManager: SessionManager
    ) {
        /**
         * Gets the linked engine session for a tab or creates (and links) one if needed.
         */
        operator fun invoke(tabId: String): EngineSession? {
            sessionManager.findSessionById(tabId)?.let {
                return sessionManager.getOrCreateEngineSession(it)
            }

            // SessionManager and BrowserStore are "eventually consistent". If this method gets
            // invoked with an ID from a BrowserStore state then it is possible that is tab is
            // already removed in SessionManager (and this will be synced with BrowserStore
            // eventually).
            //
            // In that situation we can't look up the tab anymore and can't return an EngineSession
            // here. So we need to weaken the contract and allow returning null from here.
            //
            // Eventually, once only BrowserStore keeps EngineSession references, this will no
            // longer happen.

            return null
        }
    }

    val getOrCreateEngineSession = GetOrCreateUseCase(sessionManager)
}
