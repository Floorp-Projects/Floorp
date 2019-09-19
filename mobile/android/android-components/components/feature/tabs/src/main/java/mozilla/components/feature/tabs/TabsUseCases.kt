/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

package mozilla.components.feature.tabs

import mozilla.components.browser.session.Session
import mozilla.components.browser.session.Session.Source
import mozilla.components.browser.session.SessionManager
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
         */
        operator fun invoke(
            url: String,
            selectTab: Boolean = true,
            startLoading: Boolean = true,
            parentId: String? = null,
            flags: LoadUrlFlags = LoadUrlFlags.none()
        ): Session {
            val session = Session(url, false, Source.NEW_TAB)
            val parent = parentId?.let { sessionManager.findSessionById(parentId) }
            sessionManager.add(session, selected = selectTab, parent = parent)

            if (startLoading) {
                sessionManager.getOrCreateEngineSession(session).loadUrl(url, flags)
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
         */
        operator fun invoke(
            url: String,
            selectTab: Boolean = true,
            startLoading: Boolean = true,
            parentId: String? = null,
            flags: LoadUrlFlags = LoadUrlFlags.none()
        ): Session {
            val session = Session(url, true, Source.NEW_TAB)
            val parent = parentId?.let { sessionManager.findSessionById(parentId) }
            sessionManager.add(session, selected = selectTab, parent = parent)

            if (startLoading) {
                sessionManager.getOrCreateEngineSession(session).loadUrl(url, flags)
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
