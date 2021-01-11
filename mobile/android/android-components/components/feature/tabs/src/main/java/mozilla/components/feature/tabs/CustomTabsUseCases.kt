/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

package mozilla.components.feature.tabs

import mozilla.components.browser.session.Session
import mozilla.components.browser.session.SessionManager
import mozilla.components.browser.state.state.CustomTabConfig
import mozilla.components.browser.state.state.SessionState
import mozilla.components.concept.engine.EngineSession
import mozilla.components.feature.session.SessionUseCases

/**
 * UseCases for custom tabs.
 */
class CustomTabsUseCases(
    sessionManager: SessionManager,
    loadUrlUseCase: SessionUseCases.DefaultLoadUrlUseCase
) {
    /**
     * Use case for adding a new custom tab.
     */
    class AddCustomTabUseCase(
        private val sessionManager: SessionManager,
        private val loadUrlUseCase: SessionUseCases.DefaultLoadUrlUseCase
    ) {
        /**
         * Adds a new custom tab with URL [url].
         */
        operator fun invoke(
            url: String,
            customTabConfig: CustomTabConfig,
            private: Boolean,
            additionalHeaders: Map<String, String>? = null
        ): String {
            val session = Session(url, private = private, source = SessionState.Source.CUSTOM_TAB)
            session.customTabConfig = customTabConfig

            sessionManager.add(session)

            loadUrlUseCase(url, session.id, EngineSession.LoadUrlFlags.external(), additionalHeaders)

            return session.id
        }
    }

    /**
     * Use case for removing a custom tab.
     */
    class RemoveCustomTabUseCase(
        private val sessionManager: SessionManager
    ) {
        /**
         * Removes the custom tab with the given [customTabId].
         */
        operator fun invoke(customTabId: String): Boolean {
            val session = sessionManager.findSessionById(customTabId)
            if (session != null) {
                sessionManager.remove(session)
                return true
            }
            return false
        }
    }

    val add by lazy { AddCustomTabUseCase(sessionManager, loadUrlUseCase) }
    val remove by lazy { RemoveCustomTabUseCase(sessionManager) }
}
