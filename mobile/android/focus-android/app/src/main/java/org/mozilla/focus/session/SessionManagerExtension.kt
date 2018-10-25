/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

package org.mozilla.focus.session

import mozilla.components.browser.session.Session
import mozilla.components.browser.session.SessionManager
import org.mozilla.focus.ext.savedWebViewState
import org.mozilla.focus.utils.AppConstants
import org.mozilla.focus.web.GeckoWebViewProvider
import org.mozilla.geckoview.GeckoSession

fun SessionManager.removeAndCloseSession(session: Session) {
    if (AppConstants.isGeckoBuild) {
        val geckoSession =
            session.savedWebViewState?.getParcelable<GeckoSession>(GeckoWebViewProvider.GECKO_SESSION)
        if (geckoSession != null && geckoSession.isOpen) {
            geckoSession.close()
        }
    }
    remove(session)
}

fun SessionManager.removeAndCloseAllSessions() {
    if (AppConstants.isGeckoBuild) {
        sessions.forEach { session ->
            val geckoSession =
                session.savedWebViewState?.getParcelable<GeckoSession>(GeckoWebViewProvider.GECKO_SESSION)
            if (geckoSession != null && geckoSession.isOpen) {
                geckoSession.close()
            }
        }
    }
    removeSessions()
}

fun SessionManager.removeAllAndCloseAllSessions() {
    if (AppConstants.isGeckoBuild) {
        all.forEach { session ->
            val geckoSession =
                session.savedWebViewState?.getParcelable<GeckoSession>(GeckoWebViewProvider.GECKO_SESSION)
            if (geckoSession != null && geckoSession.isOpen) {
                geckoSession.close()
            }
        }
    }
    removeAll()
}
