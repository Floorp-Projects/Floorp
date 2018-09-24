/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

package org.mozilla.focus.telemetry

import mozilla.components.browser.session.Session
import mozilla.components.browser.session.SessionManager

class TelemetrySessionObserver : SessionManager.Observer {
    override fun onSessionAdded(session: Session) {
        when (session.source) {
            Session.Source.ACTION_VIEW -> TelemetryWrapper.browseIntentEvent()
            Session.Source.ACTION_SEND -> TelemetryWrapper.shareIntentEvent(session.searchTerms.isNotEmpty())
            Session.Source.TEXT_SELECTION -> TelemetryWrapper.textSelectionIntentEvent()
            Session.Source.HOME_SCREEN -> TelemetryWrapper.openHomescreenShortcutEvent()
            Session.Source.CUSTOM_TAB -> TelemetryWrapper.customTabsIntentEvent(session.customTabConfig!!.options)
            else -> {
                // For other session types we create events at the place where we create the sessions.
            }
        }
    }
}
