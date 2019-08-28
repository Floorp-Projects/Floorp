/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

package org.mozilla.fenix.session

import android.content.Context
import mozilla.components.browser.session.Session
import mozilla.components.browser.session.SessionManager
import org.mozilla.fenix.ext.components
import org.mozilla.fenix.ext.sessionsOfType

/**
 * This observer starts and stops the service to show a notification
 * indicating that a private tab is open.
 */

class NotificationSessionObserver(
    private val context: Context
) : SessionManager.Observer {

    override fun onSessionRemoved(session: Session) {
        val privateTabsEmpty = !context.components.core.sessionManager.sessionsOfType(private = true).none()

        if (privateTabsEmpty) {
            SessionNotificationService.stop(context)
        }
    }

    override fun onAllSessionsRemoved() {
        SessionNotificationService.stop(context)
    }

    override fun onSessionAdded(session: Session) {
        if (session.private) {
            SessionNotificationService.start(context)
        }
    }
}
