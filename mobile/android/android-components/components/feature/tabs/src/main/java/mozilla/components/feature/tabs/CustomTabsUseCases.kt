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

            val loadUrlFlags = EngineSession.LoadUrlFlags.external()
            sessionManager.add(session, initialLoadFlags = loadUrlFlags)
            loadUrlUseCase(url, session.id, loadUrlFlags, additionalHeaders)
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
            if (session?.customTabConfig != null) {
                sessionManager.remove(session)
                return true
            }
            return false
        }
    }

    /**
     * Use case for migrating a custom tab to a regular tab.
     */
    class MigrateCustomTabUseCase(
        private val sessionManager: SessionManager
    ) {
        /**
         * Migrates the custom tab with the given [customTabId] to a regular
         * tab. This method has no effect if the custom tab does not exist.
         *
         * @param customTabId the custom tab to turn into a regular tab.
         * @param select whether or not to select the regular tab, defaults to true.
         */
        operator fun invoke(customTabId: String, select: Boolean = true) {
            val customTabSession = sessionManager.findSessionById(customTabId)
            customTabSession?.let {
                it.customTabConfig = null

                if (select) {
                    sessionManager.select(it)
                }
            }
        }
    }

    val add by lazy { AddCustomTabUseCase(sessionManager, loadUrlUseCase) }
    val remove by lazy { RemoveCustomTabUseCase(sessionManager) }
    val migrate by lazy { MigrateCustomTabUseCase(sessionManager) }
}
