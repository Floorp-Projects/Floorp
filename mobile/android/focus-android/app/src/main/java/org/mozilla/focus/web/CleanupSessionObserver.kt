/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

package org.mozilla.focus.web

import android.content.Context
import mozilla.components.browser.session.Session
import mozilla.components.browser.session.SessionManager
import org.mozilla.focus.ext.components

class CleanupSessionObserver(
    private val context: Context
) : SessionManager.Observer {

    override fun onSessionRemoved(session: Session) {
        if (context.components.sessionManager.sessions.isEmpty()) {
            WebViewProvider.performCleanup(context)
        }
    }

    override fun onAllSessionsRemoved() {
        WebViewProvider.performCleanup(context)
    }
}
