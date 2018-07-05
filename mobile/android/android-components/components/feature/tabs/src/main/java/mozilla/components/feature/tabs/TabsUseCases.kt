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

    val selectSession: SelectTabUseCase by lazy { SelectTabUseCase(sessionManager) }
    val removeSession: RemoveTabUseCase by lazy { RemoveTabUseCase(sessionManager) }
}
