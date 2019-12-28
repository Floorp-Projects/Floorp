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
    private val context: Context,
    private val notificationService: SessionNotificationService.Companion = SessionNotificationService
) : SessionManager.Observer {

    override fun onSessionRemoved(session: Session) {
        val privateTabsEmpty = context.components.core.sessionManager.sessionsOfType(private = true).none()

        if (privateTabsEmpty) {
            notificationService.stop(context)
        }
    }

    override fun onAllSessionsRemoved() {
        notificationService.stop(context)
    }

    override fun onSessionAdded(session: Session) {
        // Custom tabs are meant to feel like part of the app that opened them, not Fenix, so we
        // don't need to show a 'close tab' notification for them
        if (session.private && !session.isCustomTabSession()) {
            notificationService.start(context)
        }
    }
}
