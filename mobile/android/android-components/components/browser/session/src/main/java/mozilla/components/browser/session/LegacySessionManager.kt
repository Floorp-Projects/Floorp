/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

package mozilla.components.browser.session

import androidx.annotation.GuardedBy
import mozilla.components.concept.engine.Engine
import mozilla.components.support.base.observer.Observable
import mozilla.components.support.base.observer.ObserverRegistry
import kotlin.math.max
import kotlin.math.min

/**
 * This class provides access to a centralized registry of all active sessions.
 */
@Suppress("TooManyFunctions", "LargeClass")
class LegacySessionManager(
    val engine: Engine,
    delegate: Observable<SessionManager.Observer> = ObserverRegistry()
) : Observable<SessionManager.Observer> by delegate {
    // It's important that any access to `values` is synchronized;
    @GuardedBy("values")
    private val values = mutableListOf<Session>()

    @GuardedBy("values")
    private var selectedIndex: Int = NO_SELECTION

    /**
     * Returns the number of session including CustomTab sessions.
     */
    val size: Int
        get() = synchronized(values) { values.size }

    /**
     * Gets the currently selected session if there is one.
     *
     * Only one session can be selected at a given time.
     */
    val selectedSession: Session?
        get() = synchronized(values) {
            if (selectedIndex == NO_SELECTION) {
                null
            } else {
                values[selectedIndex]
            }
        }

    /**
     * Gets the currently selected session or throws an IllegalStateException if no session is
     * selected.
     *
     * It's application specific whether a session manager can have no selected session (= no sessions)
     * or not. In applications that always have at least one session dealing with the nullable
     * <code>selectedSession</code> property can be cumbersome. In those situations, where you always
     * expect a session to exist, you can use <code>selectedSessionOrThrow</code> to avoid dealing
     * with null values.
     *
     * Only one session can be selected at a given time.
     */
    val selectedSessionOrThrow: Session
        get() = selectedSession ?: throw IllegalStateException("No selected session")

    /**
     * Returns a list of active sessions and filters out sessions used for CustomTabs.
     */
    val sessions: List<Session>
        get() = synchronized(values) { values.filter { !it.isCustomTabSession() } }

    /**
     * Returns a list of all active sessions (including CustomTab sessions).
     */
    val all: List<Session>
        get() = synchronized(values) { values.toList() }

    /**
     * Adds the provided session.
     */
    fun add(
        session: Session,
        selected: Boolean = false,
        parent: Session? = null
    ) = synchronized(values) {
        addInternal(session, selected, parent = parent, viaRestore = false)
    }

    @Suppress("LongParameterList", "ComplexMethod")
    private fun addInternal(
        session: Session,
        selected: Boolean = false,
        parent: Session? = null,
        viaRestore: Boolean = false,
        notifyObservers: Boolean = true
    ) = synchronized(values) {
        if (values.find { it.id == session.id } != null) {
            throw IllegalArgumentException("Session with same ID already exists")
        }

        if (parent != null) {
            val parentIndex = values.indexOf(parent)
            values.add(parentIndex + 1, session)
        } else {
            if (viaRestore) {
                // We always restore Sessions at the beginning of the list to pretend those sessions existed before
                // we added any other sessions (e.g. coming from an Intent)
                values.add(0, session)
                if (selectedIndex != NO_SELECTION) {
                    selectedIndex++
                }
            } else {
                values.add(session)
            }
        }

        // If session is being added via restore, skip notification and auto-selection.
        // Restore will handle these actions as appropriate.
        if (viaRestore || !notifyObservers) {
            return@synchronized
        }

        notifyObservers { onSessionAdded(session) }

        // Auto-select incoming session if we don't have a currently selected one.
        if (selected || selectedIndex == NO_SELECTION && !session.isCustomTabSession()) {
            select(session)
        }
    }

    /**
     * Adds multiple sessions.
     *
     * Note that for performance reasons this method will invoke
     * [SessionManager.Observer.onSessionsRestored] and not [SessionManager.Observer.onSessionAdded]
     * for every added [Session].
     */
    fun add(sessions: List<Session>) = synchronized(values) {
        if (sessions.isEmpty()) {
            return
        }

        sessions.forEach {
            addInternal(session = it, notifyObservers = false)
        }

        if (selectedIndex == NO_SELECTION) {
            // If there is no selection yet then select first non-private session
            sessions.find { !it.private }?.let { select(it) }
        }

        // This is a performance hack to not call `onSessionAdded` for every added session. Normally
        // we'd add a new observer method for that. But since we are trying to migrate away from
        // browser-session, we should not add a new one and force the apps to use it.
        notifyObservers { onSessionsRestored() }
    }

    /**
     * Restores sessions from the provided [Snapshot].
     *
     * Notification behaviour is as follows:
     * - onSessionAdded notifications will not fire,
     * - onSessionSelected notification will fire exactly once if the snapshot isn't empty (See [updateSelection]
     *   parameter),
     * - once snapshot has been restored, and appropriate session has been selected, onSessionsRestored
     *   notification will fire.
     *
     * @param snapshot A [SessionManager.Snapshot] which may be produced by [createSnapshot].
     * @param updateSelection Whether the selected session should be updated from the restored snapshot.
     */
    fun restore(snapshot: SessionManager.Snapshot, updateSelection: Boolean = true) = synchronized(values) {
        if (snapshot.sessions.isEmpty()) {
            return
        }

        snapshot.sessions.asReversed().forEach {
            addInternal(
                it.session,
                parent = null,
                viaRestore = true
            )
        }

        if (updateSelection) {
            // Let's be forgiving about incoming illegal selection indices, but strict about what we
            // produce ourselves in `createSnapshot`.
            val sessionTupleToSelect = snapshot.sessions.getOrElse(snapshot.selectedSessionIndex) {
                // Default to the first session if we received an illegal selection index.
                snapshot.sessions[0]
            }

            select(sessionTupleToSelect.session)
        }

        notifyObservers { onSessionsRestored() }
    }

    /**
     * Removes the provided session. If no session is provided then the selected session is removed.
     */
    fun remove(
        session: Session = selectedSessionOrThrow,
        selectParentIfExists: Boolean = false
    ) = synchronized(values) {
        val indexToRemove = values.indexOf(session)
        if (indexToRemove == -1) {
            return
        }

        val selectedBeforeRemove = selectedSession

        values.removeAt(indexToRemove)

        recalculateSelectionIndex(
            indexToRemove,
            selectParentIfExists,
            session.private,
            session.parentId
        )

        values.filter { it.parentId == session.id }
            .forEach { child -> child.parentId = session.parentId }

        notifyObservers { onSessionRemoved(session) }

        if (selectedBeforeRemove != selectedSession && selectedIndex != NO_SELECTION) {
            notifyObservers { onSessionSelected(selectedSessionOrThrow) }
        }
    }

    /**
     * Recalculate the index of the selected session after a session was removed. Returns true if the index has changed.
     * False otherwise.
     */
    @Suppress("ComplexMethod")
    private fun recalculateSelectionIndex(
        indexToRemove: Int,
        selectParentIfExists: Boolean,
        private: Boolean,
        parentId: String?
    ) {
        // Recalculate selection
        var newSelectedIndex = when {
            // All items have been removed
            values.size == 0 -> NO_SELECTION

            // The selected session has been removed. We need a new selection.
            indexToRemove == selectedIndex -> newSelection(indexToRemove, selectParentIfExists, private, parentId)

            // Something on the left of our selection has been removed. We need to move the index to the left.
            indexToRemove < selectedIndex -> selectedIndex - 1

            // The selection is out of bounds. Let's select the previous item.
            selectedIndex == values.size -> selectedIndex - 1

            // We can keep the selected index.
            else -> selectedIndex
        }

        if (newSelectedIndex != NO_SELECTION && values[newSelectedIndex].isCustomTabSession()) {
            // Oh shoot! The session we want to select is a custom tab and that's a no no. Let's find a regular session
            // that is still close.
            newSelectedIndex = findNearbyNonCustomTabSession(
                // Find a new index. Either from the one we wanted to select or if that was the parent then start from
                // the index we just removed.
                if (selectParentIfExists) indexToRemove else newSelectedIndex)
        }

        selectedIndex = newSelectedIndex
    }

    private fun newSelection(
        index: Int,
        selectParentIfExists: Boolean,
        private: Boolean,
        parentId: String?
    ): Int {
        return if (selectParentIfExists && parentId != null) {
            // Select parent session
            val parent = findSessionById(parentId)
                ?: throw java.lang.IllegalStateException("Parent session referenced by id does not exist")

            values.indexOf(parent)
        } else {
            // Find session of same type (private|regular)
            if (index <= values.lastIndex && values[index].private == private) {
                return index
            }

            val nearbyMatch = findNearbySession(index) { it.private == private }

            // If there's no other private session to select we use the newest non-private one
            if (nearbyMatch == NO_SELECTION && private) values.lastIndex else nearbyMatch
        }
    }

    /**
     * @return Index of a new session that is not a custom tab and as close as possible to the given index. Returns
     * [NO_SELECTION] if no session to select could be found.
     */
    private fun findNearbyNonCustomTabSession(index: Int): Int {
        return findNearbySession(index) { !it.isCustomTabSession() }
    }

    /**
     * Tries to find a [Session] that is near [index] and satisfies the [predicate].
     *
     * Its implementation is synchronized with the behavior in `TabListReducer` of the `browser-state` component.
     */
    private fun findNearbySession(index: Int, predicate: (Session) -> Boolean): Int {
        // Okay, this is a bit stupid and complex. This implementation intends to implement the same behavior we use in
        // BrowserStore - which is operating on a list without custom tabs. Since LegacySessionManager uses a list with
        // custom tabs mixed in, we need to shift the index around a bit.

        // First we need to determine the number of custom tabs that are *before* the index we removed from. This is
        // later needed to calculate the index we would have removed from in a list without custom tabs.
        var numberOfCustomTabsBeforeRemovedIndex = 0
        for (i in 0..min(index - 1, values.lastIndex)) {
            if (values[i].isCustomTabSession()) {
                numberOfCustomTabsBeforeRemovedIndex++
            }
        }

        // Now calculate the index and list without custom tabs included.
        val indexWithoutCustomTabs = index - numberOfCustomTabsBeforeRemovedIndex
        val sessionsWithoutCustomTabs = sessions

        // Code run before this one would have re-used the selected index if it is still a valid index that satisfies
        // the predicate - however that list contained custom tabs. Now without custom tabs we may be able to re-use
        // the exiting index. Let's check if we can:
        if (indexWithoutCustomTabs >= 0 &&
            indexWithoutCustomTabs <= sessionsWithoutCustomTabs.lastIndex &&
            predicate(sessionsWithoutCustomTabs[indexWithoutCustomTabs])) {
            return values.indexOf(sessionsWithoutCustomTabs[indexWithoutCustomTabs])
        }

        // Finally try sessions (without custom tabs) oscillating near the index.
        val range = 1..max(sessionsWithoutCustomTabs.lastIndex - indexWithoutCustomTabs, indexWithoutCustomTabs)
        for (steps in range) {
            listOf(indexWithoutCustomTabs - steps, indexWithoutCustomTabs + steps).forEach { current ->
                if (current >= 0 &&
                    current <= sessionsWithoutCustomTabs.lastIndex &&
                    predicate(sessionsWithoutCustomTabs[current])) {
                    // Now that we have a match we need to re-calculate the index in the list that includes custom tabs.
                    val session = sessionsWithoutCustomTabs[current]
                    return values.indexOf(session)
                }
            }
        }

        return NO_SELECTION
    }

    /**
     * Removes all sessions but CustomTab sessions.
     */
    fun removeSessions() = synchronized(values) {
        sessions.forEach {
            values.remove(it)
        }

        selectedIndex = NO_SELECTION

        // NB: This callback indicates to observers that either removeSessions or removeAll were
        // invoked, not that the manager is now empty.
        notifyObservers { onAllSessionsRemoved() }
    }

    /**
     * Removes all sessions including CustomTab sessions.
     */
    fun removeAll() = synchronized(values) {
        values.clear()

        selectedIndex = NO_SELECTION

        // NB: This callback indicates to observers that either removeSessions or removeAll were
        // invoked, not that the manager is now empty.
        notifyObservers { onAllSessionsRemoved() }
    }

    /**
     * Marks the given session as selected.
     */
    fun select(session: Session) = synchronized(values) {
        val index = values.indexOf(session)

        require(index != -1) {
            "Value to select is not in list"
        }

        selectedIndex = index

        notifyObservers { onSessionSelected(session) }
    }

    /**
     * Finds and returns the session with the given id. Returns null if no matching session could be
     * found.
     */
    fun findSessionById(id: String): Session? = synchronized(values) {
        values.find { session -> session.id == id }
    }

    companion object {
        const val NO_SELECTION = -1
    }
}
