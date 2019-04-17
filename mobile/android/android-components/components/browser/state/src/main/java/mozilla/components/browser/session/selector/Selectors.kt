/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

package mozilla.components.browser.session.selector

import mozilla.components.browser.session.state.BrowserState
import mozilla.components.browser.session.state.SessionState

// Extension functions for querying and dissecting [BrowserState]

/**
 * The currently selected [SessionState] if there's one.
 *
 * Only one [SessionState] can be selected at a given time.
 */
val BrowserState.selectedSessionState: SessionState?
    get() = if (selectedSessionId.isNullOrEmpty()) {
        null
    } else {
        sessions.firstOrNull { it.id == selectedSessionId }
    }

/**
 * Finds and returns the session with the given id. Returns null if no matching session could be
 * found.
 */
fun BrowserState.findSession(sessionId: String): SessionState? {
    return sessions.firstOrNull { it.id == sessionId }
}

/**
 * Finds and returns the session with the given id or the selected session if no id was provided (null). Returns null
 * if no matching session could be found or if no selected session exists.
 */
fun BrowserState.findSessionOrSelected(sessionId: String? = null): SessionState? {
    return if (sessionId != null) {
        findSession(sessionId)
    } else {
        selectedSessionState
    }
}
