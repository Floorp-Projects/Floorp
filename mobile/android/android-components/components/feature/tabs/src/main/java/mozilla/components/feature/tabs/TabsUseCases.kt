/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

package mozilla.components.feature.tabs

import mozilla.components.browser.session.Session
import mozilla.components.browser.session.SessionManager

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
    ) {
        /**
         * Add a new tab and load the provided URL.
         *
         * @param url The URL to be loaded in the new tab.
         * @param selectTab True (default) if the new tab should be selected immediately.
         */
        fun invoke(url: String, selectTab: Boolean = true) {
            val session = Session(url)
            sessionManager.add(session, selected = selectTab)
        }
    }

    val selectSession: SelectTabUseCase by lazy { SelectTabUseCase(sessionManager) }
    val removeSession: RemoveTabUseCase by lazy { RemoveTabUseCase(sessionManager) }
    val addSession: AddNewTabUseCase by lazy { AddNewTabUseCase(sessionManager) }
}
