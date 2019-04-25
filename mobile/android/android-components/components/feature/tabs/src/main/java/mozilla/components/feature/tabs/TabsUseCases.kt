/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

package mozilla.components.feature.tabs

import mozilla.components.browser.session.Session
import mozilla.components.browser.session.Session.Source
import mozilla.components.browser.session.SessionManager
import mozilla.components.feature.session.SessionUseCases.LoadUrlUseCase

/**
 * Contains use cases related to the tabs feature.
 */
class TabsUseCases(
    sessionManager: SessionManager
) {
    class SelectTabUseCase internal constructor(
        private val sessionManager: SessionManager
    ) {
        /**
         * Marks the provided session as selected.
         *
         * @param session The session to select.
         */
        fun invoke(session: Session) {
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
        fun invoke(session: Session) {
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
         */
        override fun invoke(url: String) {
            this.invoke(url, true, true, null)
        }

        /**
         * Adds a new tab and loads the provided URL.
         *
         * @param url The URL to be loaded in the new tab.
         * @param selectTab True (default) if the new tab should be selected immediately.
         * @param startLoading True (default) if the new tab should start loading immediately.
         * @param parent the parent session to use for the newly created session.
         */
        fun invoke(
            url: String,
            selectTab: Boolean = true,
            startLoading: Boolean = true,
            parent: Session? = null
        ): Session {
            val session = Session(url, false, Source.NEW_TAB)
            sessionManager.add(session, selected = selectTab, parent = parent)

            if (startLoading) {
                sessionManager.getOrCreateEngineSession(session).loadUrl(url)
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
         */
        override fun invoke(url: String) {
            this.invoke(url, true, true, null)
        }

        /**
         * Adds a new private tab and loads the provided URL.
         *
         * @param url The URL to be loaded in the new tab.
         * @param selectTab True (default) if the new tab should be selected immediately.
         * @param startLoading True (default) if the new tab should start loading immediately.
         * @param parent the parent session to use for the newly created session.
         */
        fun invoke(
            url: String,
            selectTab: Boolean = true,
            startLoading: Boolean = true,
            parent: Session? = null
        ): Session {
            val session = Session(url, true, Source.NEW_TAB)
            sessionManager.add(session, selected = selectTab, parent = parent)

            if (startLoading) {
                sessionManager.getOrCreateEngineSession(session).loadUrl(url)
            }

            return session
        }
    }

    class RemoveAllTabsUseCase internal constructor(
        private val sessionManager: SessionManager
    ) {
        fun invoke() {
            sessionManager.removeSessions()
        }
    }

    class RemoveAllTabsOfTypeUseCase internal constructor(
        private val sessionManager: SessionManager
    ) {
        /**
         * @param private pass true if only private tabs should be removed otherwise normal tabs will be removed
         */

        fun invoke(private: Boolean) {
            sessionManager.sessions.filter { it.private == private }.forEach {
                sessionManager.remove(it)
            }
        }
    }

    val selectTab: SelectTabUseCase by lazy { SelectTabUseCase(sessionManager) }
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
