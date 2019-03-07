/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

package mozilla.components.feature.tabs.tabstray

import androidx.recyclerview.widget.DiffUtil
import androidx.recyclerview.widget.ListUpdateCallback
import mozilla.components.browser.session.Session
import mozilla.components.browser.session.SessionManager
import mozilla.components.concept.tabstray.TabsTray

/**
 * Presenter implementation for a tabs tray implementation in order to update the tabs tray whenever
 * the state of the session manager changes.
 */
class TabsTrayPresenter(
    private val tabsTray: TabsTray,
    private val sessionManager: SessionManager,
    private val closeTabsTray: () -> Unit,
    internal var sessionsFilter: (Session) -> Boolean = { true }
) : SessionManager.Observer {
    private var sessions = sessionManager.sessions.filter { sessionsFilter(it) }
    private var selectedIndex = sessions.indexOf(sessionManager.selectedSession)

    fun start() {
        sessionManager.register(this)

        tabsTray.displaySessions(sessions, selectedIndex)
    }

    fun stop() {
        sessionManager.unregister(this)
    }

    override fun onSessionRemoved(session: Session) {
        calculateDiffAndUpdateTabsTray()

        if (sessions.isEmpty()) {
            closeTabsTray.invoke()
        }
    }

    override fun onSessionAdded(session: Session) {
        calculateDiffAndUpdateTabsTray()
    }

    override fun onSessionSelected(session: Session) {
        calculateDiffAndUpdateTabsTray()
    }

    override fun onAllSessionsRemoved() {
        calculateDiffAndUpdateTabsTray()

        closeTabsTray.invoke()
    }

    /**
     * Calculates a diff between the last know state and the new state. After that it updates the
     * tab tray with the new data and notifies it about what changes happened so that it can animate
     * those changes.
     */
    internal fun calculateDiffAndUpdateTabsTray() {
        val updatedSessions = sessionManager.sessions.filter { sessionsFilter(it) }
        val updatedIndex = if (updatedSessions.isNotEmpty()) {
            updatedSessions.indexOf(sessionManager.selectedSession)
        } else {
            -1
        }

        val result = DiffUtil.calculateDiff(
            DiffCallback(sessions, updatedSessions, selectedIndex, updatedIndex),
            false)

        sessions = updatedSessions
        selectedIndex = updatedIndex

        tabsTray.updateSessions(updatedSessions, updatedIndex)

        result.dispatchUpdatesTo(object : ListUpdateCallback {
            override fun onChanged(position: Int, count: Int, payload: Any?) {
                tabsTray.onSessionsChanged(position, count)
            }

            override fun onMoved(fromPosition: Int, toPosition: Int) {
                tabsTray.onSessionMoved(fromPosition, toPosition)
            }

            override fun onInserted(position: Int, count: Int) {
                tabsTray.onSessionsInserted(position, count)
            }

            override fun onRemoved(position: Int, count: Int) {
                tabsTray.onSessionsRemoved(position, count)
            }
        })
    }
}

internal class DiffCallback(
    private var currentSessions: List<Session>,
    private var updatedSessions: List<Session>,
    private var currentIndex: Int,
    private var updatedIndex: Int
) : DiffUtil.Callback() {
    override fun areItemsTheSame(oldItemPosition: Int, newItemPosition: Int): Boolean =
        currentSessions[oldItemPosition] == updatedSessions[newItemPosition]

    override fun getOldListSize(): Int = currentSessions.size

    override fun getNewListSize(): Int = updatedSessions.size

    override fun areContentsTheSame(oldItemPosition: Int, newItemPosition: Int): Boolean {
        if (oldItemPosition == currentIndex && newItemPosition != updatedIndex) {
            // This item was previously selected and is not selected anymore (-> changed).
            return false
        }

        if (newItemPosition == updatedIndex && oldItemPosition != currentIndex) {
            // This item was not selected previously and is now selected (-> changed).
            return false
        }

        // If the URL of both items is the same then we treat this item as the same (-> unchanged).
        return currentSessions[oldItemPosition].url == updatedSessions[newItemPosition].url
    }
}
