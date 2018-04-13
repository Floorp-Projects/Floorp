/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

package org.mozilla.samples.browser

import mozilla.components.browser.session.Session
import mozilla.components.browser.session.SessionProvider

/**
 * Dummy implementation that will always return a session pointing to mozilla.org.
 */
class DummySessionProvider : SessionProvider {
    override fun getInitialSessions(): Pair<List<Session>, Int> {
        val session = Session("https://www.mozilla.org")

        val sessions = mutableListOf(session)

        return Pair(sessions, 0)
    }
}