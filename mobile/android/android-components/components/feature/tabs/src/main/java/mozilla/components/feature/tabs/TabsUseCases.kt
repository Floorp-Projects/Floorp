/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

package mozilla.components.feature.tabs

import mozilla.components.browser.session.Session
import mozilla.components.browser.session.Session.Source
import mozilla.components.browser.session.SessionManager
import mozilla.components.concept.engine.EngineSession
import mozilla.components.concept.engine.EngineSession.LoadUrlFlags
import mozilla.components.feature.session.SessionUseCases.LoadUrlUseCase

/**
 * Contains use cases related to the tabs feature.
 */
class TabsUseCases(
    sessionManager: SessionManager
) {
    /**
     * Contract for use cases that select a tab.
     */
    interface SelectTabUseCase {
        operator fun invoke(session: Session)
    }

    class DefaultSelectTabUseCase internal constructor(
        private val sessionManager: SessionManager
    ) : SelectTabUseCase {

        /**
         * Marks the provided session as selected.
         *
         * @param session The session to select.
         */
        override operator fun invoke(session: Session) {
            sessionManager.select(session)
        }
    }

    class RemoveTabUseCase internal constructor(
        private val sessionManager: SessionManager
    ) {

        /**
         * Removes the session with the provided ID. This method
         * has no effect if the session doesn't exist.
         *
         * @param sessionId The ID of the session to remove.
         */
        operator fun invoke(sessionId: String) {
            val session = sessionManager.findSessionById(sessionId)
            if (session != null) {
                invoke(session)
            }
        }

        /**
         * Removes the provided session.
         *
         * @param session The session to remove.
         */
        operator fun invoke(session: Session) {
            sessionManager.remove(session)
        }
    }

    class AddNewTabUseCase internal constructor(
        private val sessionManager: SessionManager
    ) : LoadUrlUseCase {

        /**
         * Adds a new tab and loads the provided URL.
         *
         * @param url The URL to be loaded in the new tab.
         * @param flags the [LoadUrlFlags] to use when loading the provided URL.
         */
        override fun invoke(url: String, flags: LoadUrlFlags) {
            this.invoke(url, true, true, null, flags)
        }

        /**
         * Adds a new tab and loads the provided URL.
         *
         * @param url The URL to be loaded in the new tab.
         * @param selectTab True (default) if the new tab should be selected immediately.
         * @param startLoading True (default) if the new tab should start loading immediately.
         * @param parentId the id of the parent tab to use for the newly created tab.
         * @param flags the [LoadUrlFlags] to use when loading the provided URL.
         * @param engineSession (optional) engine session to use for this tab.
         */
        @Suppress("LongParameterList")
        operator fun invoke(
            url: String,
            selectTab: Boolean = true,
            startLoading: Boolean = true,
            parentId: String? = null,
            flags: LoadUrlFlags = LoadUrlFlags.none(),
            engineSession: EngineSession? = null
        ): Session {
            val session = Session(url, false, Source.NEW_TAB)
            val parent = parentId?.let { sessionManager.findSessionById(parentId) }
            sessionManager.add(session, selected = selectTab, engineSession = engineSession, parent = parent)

            // If an engine session is specified then loading will have already started
            // during sessionManager.add when linking the session to its engine session.
            if (startLoading && engineSession == null) {
                val parentEngineSession = parent?.let { sessionManager.getEngineSession(it) }
                sessionManager.getOrCreateEngineSession(session).loadUrl(url, parentEngineSession, flags)
            }

            return session
        }
    }

    class AddNewPrivateTabUseCase internal constructor(
        private val sessionManager: SessionManager
    ) : LoadUrlUseCase {

        /**
         * Adds a new private tab and loads the provided URL.
         *
         * @param url The URL to be loaded in the new private tab.
         * @param flags the [LoadUrlFlags] to use when loading the provided URL.
         */
        override fun invoke(url: String, flags: LoadUrlFlags) {
            this.invoke(url, true, true, null, flags)
        }

        /**
         * Adds a new private tab and loads the provided URL.
         *
         * @param url The URL to be loaded in the new tab.
         * @param selectTab True (default) if the new tab should be selected immediately.
         * @param startLoading True (default) if the new tab should start loading immediately.
         * @param parentId the id of the parent tab to use for the newly created tab.
         * @param flags the [LoadUrlFlags] to use when loading the provided URL.
         * @param engineSession (optional) engine session to use for this tab.
         */
        @Suppress("LongParameterList")
        operator fun invoke(
            url: String,
            selectTab: Boolean = true,
            startLoading: Boolean = true,
            parentId: String? = null,
            flags: LoadUrlFlags = LoadUrlFlags.none(),
            engineSession: EngineSession? = null
        ): Session {
            val session = Session(url, true, Source.NEW_TAB)
            val parent = parentId?.let { sessionManager.findSessionById(parentId) }
            sessionManager.add(session, selected = selectTab, engineSession = engineSession, parent = parent)

            // If an engine session is specified then loading will have already started
            // during sessionManager.add when linking the session to its engine session.
            if (startLoading && engineSession == null) {
                val parentEngineSession = parent?.let { sessionManager.getEngineSession(it) }
                sessionManager.getOrCreateEngineSession(session).loadUrl(url, parentEngineSession, flags)
            }

            return session
        }
    }

    class RemoveAllTabsUseCase internal constructor(
        private val sessionManager: SessionManager
    ) {
        operator fun invoke() {
            sessionManager.removeSessions()
        }
    }

    class RemoveAllTabsOfTypeUseCase internal constructor(
        private val sessionManager: SessionManager
    ) {

        /**
         * @param private pass true if only private tabs should be removed otherwise normal tabs will be removed
         */
        operator fun invoke(private: Boolean) {
            sessionManager.sessions.filter { it.private == private }.forEach {
                sessionManager.remove(it)
            }
        }
    }

    val selectTab: SelectTabUseCase by lazy { DefaultSelectTabUseCase(sessionManager) }
    val removeTab: RemoveTabUseCase by lazy { RemoveTabUseCase(sessionManager) }
    val addTab: AddNewTabUseCase by lazy { AddNewTabUseCase(sessionManager) }
    val addPrivateTab: AddNewPrivateTabUseCase by lazy { AddNewPrivateTabUseCase(sessionManager) }
    val removeAllTabs: RemoveAllTabsUseCase by lazy { RemoveAllTabsUseCase(sessionManager) }
    val removeAllTabsOfType: RemoveAllTabsOfTypeUseCase by lazy {
        RemoveAllTabsOfTypeUseCase(
            sessionManager
        )
    }
}
