/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

package mozilla.components.concept.tabstray

import android.view.View
import mozilla.components.browser.session.Session
import mozilla.components.support.base.observer.Observable

/**
 * Generic interface for components that provide "tabs tray" functionality.
 */
interface TabsTray : Observable<TabsTray.Observer> {
    /**
     * Interface to be implemented by classes that want to observe a tabs tray.
     */
    interface Observer {
        /**
         * A new tab has been selected.
         */
        fun onTabSelected(session: Session)

        /**
         * A tab has been closed.
         */
        fun onTabClosed(session: Session)
    }

    /**
     * Displays the given list of sessions.
     *
     * This method will be invoked with the initial list of sessions that should be displayed.
     *
     * @param sessions The list of sessions to be displayed.
     * @param selectedIndex The list index of the currently selected session.
     */
    fun displaySessions(sessions: List<Session>, selectedIndex: Int)

    /**
     * Updates the list of sessions.
     *
     * Calling this method is usually followed by calling onSession*() methods to indicate what
     * exactly has changed. This allows the tabs tray implementation to animate between the old and
     * new state.
     *
     * @param sessions The list of sessions to be displayed.
     * @param selectedIndex The list index of the currently selected session.
     */
    fun updateSessions(sessions: List<Session>, selectedIndex: Int)

    /**
     * Called after updateSessions() when <code>count</code> number of sessions are inserted at the
     * given position.
     */
    fun onSessionsInserted(position: Int, count: Int)

    /**
     * Called after updateSessions() when <code>count</code> number of sessions are removed from
     * the given position.
     */
    fun onSessionsRemoved(position: Int, count: Int)

    /**
     * Called after updateSessions() when a session changes it position.
     */
    fun onSessionMoved(fromPosition: Int, toPosition: Int)

    /**
     * Called after updateSessions() when <code>count</code> number of sessions are updated at the
     * given position.
     */
    fun onSessionsChanged(position: Int, count: Int)

    /**
     * Convenience method to cast the implementation of this interface to an Android View object.
     */
    fun asView(): View = this as View
}
