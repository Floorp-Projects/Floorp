/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

package mozilla.components.browser.session

/**
 * Interface to be implemented by classes that provide the initial set of sessions to the
 * SessionManager.
 */
interface SessionProvider {
    /**
     * Return the initial list of sessions and index of the selected session. An implementation is
     * required to return at least one session.
     */
    fun getInitialSessions(): Pair<List<Session>, Int>
}
