/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

package mozilla.components.browser.session

import android.content.ComponentCallbacks2
import mozilla.components.browser.session.engine.EngineObserver
import mozilla.components.browser.session.ext.syncDispatch
import mozilla.components.browser.session.ext.toCustomTabSessionState
import mozilla.components.browser.session.ext.toTabSessionState
import mozilla.components.browser.state.action.CustomTabListAction
import mozilla.components.browser.state.action.EngineAction.LinkEngineSessionAction
import mozilla.components.browser.state.action.EngineAction.UpdateEngineSessionStateAction
import mozilla.components.browser.state.action.EngineAction.UnlinkEngineSessionAction
import mozilla.components.browser.state.action.ReaderAction
import mozilla.components.browser.state.action.SystemAction
import mozilla.components.browser.state.action.TabListAction
import mozilla.components.browser.state.selector.findTab
import mozilla.components.browser.state.state.ReaderState
import mozilla.components.browser.state.store.BrowserStore
import mozilla.components.concept.engine.Engine
import mozilla.components.concept.engine.EngineSession
import mozilla.components.concept.engine.EngineSessionState
import mozilla.components.support.base.log.logger.Logger
import mozilla.components.support.base.memory.MemoryConsumer
import mozilla.components.support.base.observer.Observable
import mozilla.components.support.ktx.kotlin.isExtensionUrl
import java.lang.IllegalArgumentException

/**
 * This class provides access to a centralized registry of all active sessions.
 */
@Suppress("TooManyFunctions", "LargeClass")
class SessionManager(
    val engine: Engine,
    private val store: BrowserStore? = null,
    private val linker: EngineSessionLinker = EngineSessionLinker(store),
    private val delegate: LegacySessionManager = LegacySessionManager(engine, linker)
) : Observable<SessionManager.Observer> by delegate, MemoryConsumer {
    private val logger = Logger("SessionManager")

    /**
     * This class only exists for migrating from browser-session
     * to browser-state. We need a way to dispatch the corresponding browser
     * actions when an engine session is linked and unlinked.
     */
    class EngineSessionLinker(private val store: BrowserStore?) {
        /**
         * Links the provided [Session] and [EngineSession].
         */
        fun link(
            session: Session,
            engineSession: EngineSession,
            parentEngineSession: EngineSession?,
            sessionRestored: Boolean = false,
            skipLoading: Boolean = false
        ) {
            unlink(session)

            session.engineSessionHolder.apply {
                this.engineSession = engineSession
                this.engineObserver = EngineObserver(session, store).also { observer ->
                    engineSession.register(observer)
                    if (!sessionRestored && !skipLoading) {
                        if (session.url.isExtensionUrl()) {
                            // The parent tab/session is used as a referrer which is not accurate
                            // for extension pages. The extension page is not loaded by the parent
                            // tab, but opened by an extension e.g. via browser.tabs.update.
                            engineSession.loadUrl(session.url)
                        } else {
                            engineSession.loadUrl(session.url, parentEngineSession)
                        }
                    }
                }
            }

            store?.syncDispatch(LinkEngineSessionAction(session.id, engineSession))
        }

        fun unlink(session: Session) {
            val observer = session.engineSessionHolder.engineObserver
            val engineSession = session.engineSessionHolder.engineSession

            if (observer != null && engineSession != null) {
                engineSession.unregister(observer)
            }

            session.engineSessionHolder.engineSession = null
            session.engineSessionHolder.engineObserver = null

            // Note: We could consider not clearing the engine session state here. Instead we could
            // try to actively set it here by calling save() on the engine session (if we have one).
            // That way adding the same Session again could restore the previous state automatically
            // (although we would need to make sure we do not override it with null in link()).
            // Clearing the engine session state would be left to the garbage collector whenever the
            // session itself gets collected.
            session.engineSessionHolder.engineSessionState = null

            store?.syncDispatch(UnlinkEngineSessionAction(session.id))

            // Now, that neither the session manager nor the store keep a reference to the engine
            // session, we can close it.
            engineSession?.close()
        }
    }

    /**
     * Returns the number of session including CustomTab sessions.
     */
    val size: Int
        get() = delegate.size

    /**
     * Produces a snapshot of this manager's state, suitable for restoring via [SessionManager.restore].
     * Only regular sessions are included in the snapshot. Private and Custom Tab sessions are omitted.
     *
     * @return [Snapshot] of the current session state.
     */
    fun createSnapshot(): Snapshot {
        val snapshot = delegate.createSnapshot()

        // The reader state is no longer part of the browser session so we copy
        // it from the store if it exists. This can be removed once browser-state
        // migration is complete and the session storage uses the store instead
        // of the session manager.
        return snapshot.copy(
            sessions = snapshot.sessions.map { item ->
                store?.state?.findTab(item.session.id)?.let {
                    item.copy(readerState = it.readerState)
                } ?: item
            }
        )
    }

    /**
     * Produces a [Snapshot.Item] of a single [Session], suitable for restoring via [SessionManager.restore].
     */
    fun createSessionSnapshot(session: Session): Snapshot.Item {
        val item = delegate.createSessionSnapshot(session)

        // The reader state is no longer part of the browser session so we copy
        // it from the store if it exists. This can be removed once browser-state
        // migration is complete and the session storage uses the store instead
        // of the session manager.
        return store?.state?.findTab(item.session.id)?.let {
            item.copy(readerState = it.readerState)
        } ?: item
    }

    /**
     * Gets the currently selected session if there is one.
     *
     * Only one session can be selected at a given time.
     */
    val selectedSession
        get() = delegate.selectedSession

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
        get() = delegate.selectedSessionOrThrow

    /**
     * Returns a list of active sessions and filters out sessions used for CustomTabs.
     */
    val sessions: List<Session>
        get() = delegate.sessions

    /**
     * Returns a list of all active sessions (including CustomTab sessions).
     */
    val all: List<Session>
        get() = delegate.all

    /**
     * Adds the provided session.
     */
    fun add(
        session: Session,
        selected: Boolean = false,
        engineSession: EngineSession? = null,
        engineSessionState: EngineSessionState? = null,
        parent: Session? = null
    ) {
        // Add store to Session so that it can dispatch actions whenever it changes.
        session.store = store

        if (parent != null) {
            require(all.contains(parent)) { "The parent does not exist" }
            session.parentId = parent.id
        }

        if (session.isCustomTabSession()) {
            store?.syncDispatch(
                CustomTabListAction.AddCustomTabAction(
                    session.toCustomTabSessionState()
                )
            )
        } else {
            store?.syncDispatch(
                TabListAction.AddTabAction(
                    session.toTabSessionState(),
                    select = selected
                )
            )
        }

        if (engineSessionState != null && engineSession == null) {
            // If the caller passed us an engine session state then also notify the store. We only
            // do this if there is no engine session, because this mirrors the behavior in
            // LegacySessionManager, which will either link an engine session or keep its state.
            store?.syncDispatch(UpdateEngineSessionStateAction(
                session.id,
                engineSessionState
            ))
        }

        delegate.add(session, selected, engineSession, engineSessionState, parent)
    }

    /**
     * Adds multiple sessions.
     *
     * Note that for performance reasons this method will invoke
     * [SessionManager.Observer.onSessionsRestored] and not [SessionManager.Observer.onSessionAdded]
     * for every added [Session].
     */
    fun add(sessions: List<Session>) {
        // We disallow bulk adding custom tabs or sessions with a parent. At the moment this is not
        // needed and it makes the browser-state migration logic easier.

        sessions.find { it.isCustomTabSession() }?.let {
            throw IllegalArgumentException("Bulk adding of custom tab sessions is not supported.")
        }

        sessions.find { it.parentId != null }?.let {
            throw IllegalArgumentException("Bulk adding of sessions with a parent is not supported. ")
        }

        // Add store to each Session so that it can dispatch actions whenever it changes.
        sessions.forEach { it.store = store }

        delegate.add(sessions)

        store?.syncDispatch(TabListAction.AddMultipleTabsAction(
            tabs = sessions.map { it.toTabSessionState() }
        ))
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
     * @param snapshot A [Snapshot] which may be produced by [createSnapshot].
     * @param updateSelection Whether the selected session should be updated from the restored snapshot.
     */
    @Suppress("ComplexMethod")
    fun restore(snapshot: Snapshot, updateSelection: Boolean = true) {
        // Add store to each Session so that it can dispatch actions whenever it changes.
        snapshot.sessions.forEach { it.session.store = store }

        delegate.restore(snapshot, updateSelection)

        val items = snapshot.sessions
            .filter {
                // A restored snapshot should not contain any custom tab so we should be able to safely ignore
                // them here.
                !it.session.isCustomTabSession()
            }

        val tabs = items.map { item ->
            item.session.toTabSessionState()
        }

        val selectedTabId = if (updateSelection && snapshot.selectedSessionIndex != NO_SELECTION) {
            val index = snapshot.selectedSessionIndex
            if (index in 0..snapshot.sessions.lastIndex) {
                snapshot.sessions[index].session.id
            } else {
                null
            }
        } else {
            null
        }

        store?.syncDispatch(TabListAction.RestoreAction(tabs, selectedTabId))

        items.forEach { item ->
            item.engineSession?.let { store?.syncDispatch(LinkEngineSessionAction(item.session.id, it)) }
            item.engineSessionState?.let { store?.syncDispatch(UpdateEngineSessionStateAction(item.session.id, it)) }
            item.readerState?.let {
                store?.syncDispatch(ReaderAction.UpdateReaderActiveAction(item.session.id, it.active))
                it.activeUrl?.let { activeUrl ->
                    store?.syncDispatch(ReaderAction.UpdateReaderActiveUrlAction(item.session.id, activeUrl))
                }
            }
        }
    }

    /**
     * Gets the linked engine session for the provided session (if it exists).
     */
    fun getEngineSession(session: Session = selectedSessionOrThrow) = delegate.getEngineSession(session)

    /**
     * Gets the linked engine session for the provided session and creates it if needed.
     */
    fun getOrCreateEngineSession(
        session: Session = selectedSessionOrThrow,
        skipLoading: Boolean = false
    ): EngineSession {
        return delegate.getOrCreateEngineSession(session, skipLoading)
    }

    /**
     * Removes the provided session. If no session is provided then the selected session is removed.
     */
    fun remove(
        session: Session = selectedSessionOrThrow,
        selectParentIfExists: Boolean = false
    ) {
        delegate.remove(session, selectParentIfExists)

        if (session.isCustomTabSession()) {
            store?.syncDispatch(
                CustomTabListAction.RemoveCustomTabAction(session.id)
            )
        } else {
            store?.syncDispatch(
                TabListAction.RemoveTabAction(session.id)
            )
        }
    }

    /**
     * Removes all sessions but CustomTab sessions.
     */
    fun removeSessions() {
        delegate.removeSessions()
        store?.syncDispatch(
            TabListAction.RemoveAllTabsAction
        )
    }

    /**
     * Removes all sessions including CustomTab sessions.
     */
    fun removeAll() {
        delegate.removeAll()
        store?.syncDispatch(
            TabListAction.RemoveAllTabsAction
        )
        store?.syncDispatch(
            CustomTabListAction.RemoveAllCustomTabsAction
        )
    }

    /**
     * Marks the given session as selected.
     */
    fun select(session: Session) {
        delegate.select(session)

        store?.syncDispatch(
            TabListAction.SelectTabAction(session.id)
        )
    }

    /**
     * Finds and returns the session with the given id. Returns null if no matching session could be
     * found.
     */
    fun findSessionById(id: String) = delegate.findSessionById(id)

    /**
     * Informs this [SessionManager] that the OS is in low memory condition so it
     * can reduce its allocated objects.
     */
    @Deprecated("Use onTrimMemory instead.", replaceWith = ReplaceWith("onTrimMemory"))
    fun onLowMemory() {
        onTrimMemory(ComponentCallbacks2.TRIM_MEMORY_MODERATE)
    }

    @Synchronized
    @Suppress("NestedBlockDepth")
    override fun onTrimMemory(level: Int) {
        val clearThumbnails = shouldClearThumbnails(level)
        val closeEngineSessions = shouldCloseEngineSessions(level)

        logger.debug("onTrimMemory($level): clearThumbnails=$clearThumbnails, closeEngineSessions=$closeEngineSessions")

        if (!clearThumbnails && !closeEngineSessions) {
            // Nothing to do for now.
            return
        }

        val selectedSession = selectedSession
        val states = mutableMapOf<String, EngineSessionState>()
        val sessions = sessions

        var clearedThumbnails = 0
        var unlinkedEngineSessions = 0

        sessions.forEach {
            if (it != selectedSession) {
                if (clearThumbnails) {
                    it.thumbnail = null
                    clearedThumbnails++
                }

                if (closeEngineSessions) {
                    val engineSession = it.engineSessionHolder.engineSession
                    if (engineSession != null) {
                        val state = engineSession.saveState()
                        linker.unlink(it)

                        it.engineSessionHolder.engineSessionState = state
                        states[it.id] = state

                        unlinkedEngineSessions++
                    }
                }
            }
        }

        logger.debug("Cleared $clearedThumbnails thumbnail(s) and unlinked $unlinkedEngineSessions engine sessions(s)")

        store?.syncDispatch(SystemAction.LowMemoryAction(states))
    }

    companion object {
        const val NO_SELECTION = -1
    }

    data class Snapshot(
        val sessions: List<Item>,
        val selectedSessionIndex: Int
    ) {
        fun isEmpty() = sessions.isEmpty()

        data class Item(
            val session: Session,
            val engineSession: EngineSession? = null,
            val engineSessionState: EngineSessionState? = null,
            val readerState: ReaderState? = null
        )

        companion object {
            fun empty() = Snapshot(emptyList(), NO_SELECTION)
            fun singleItem(item: Item) = Snapshot(listOf(item), NO_SELECTION)
        }
    }

    /**
     * Interface to be implemented by classes that want to observe the session manager.
     */
    interface Observer {
        /**
         * The selection has changed and the given session is now the selected session.
         */
        fun onSessionSelected(session: Session) = Unit

        /**
         * The given session has been added.
         */
        fun onSessionAdded(session: Session) = Unit

        /**
         * Sessions have been restored via a snapshot. This callback is invoked at the end of the
         * call to <code>read</code>, after every session in the snapshot was added, and
         * appropriate session was selected.
         */
        fun onSessionsRestored() = Unit

        /**
         * The given session has been removed.
         */
        fun onSessionRemoved(session: Session) = Unit

        /**
         * All sessions have been removed. Note that this will callback will be invoked whenever
         * <code>removeAll()</code> or <code>removeSessions</code> have been called on the
         * <code>SessionManager</code>. This callback will NOT be invoked when just the last
         * session has been removed by calling <code>remove()</code> on the <code>SessionManager</code>.
         */
        fun onAllSessionsRemoved() = Unit
    }
}

/**
 * Tries to find a session with the provided session ID and runs the block if found.
 *
 * @return The result of the [block] executed.
 */
fun SessionManager.runWithSession(
    sessionId: String?,
    block: SessionManager.(Session) -> Boolean
): Boolean {
    sessionId?.let {
        findSessionById(sessionId)?.let { session ->
            return block(session)
        }
    }
    return false
}

/**
 * Tries to find a session with the provided session ID or uses the selected session and runs the block if found.
 *
 * @return The result of the [block] executed.
 */
fun SessionManager.runWithSessionIdOrSelected(
    sessionId: String?,
    block: SessionManager.(Session) -> Boolean
): Boolean {
    sessionId?.let {
        findSessionById(sessionId)?.let { session ->
            return block(session)
        }
    }
    selectedSession?.let {
        return block(it)
    }

    return false
}

private fun shouldClearThumbnails(level: Int): Boolean {
    return when (level) {
        // Foreground: The device is running much lower on memory. The app is running and not killable, but the
        // system wants us to release unused resources to improve system performance.
        ComponentCallbacks2.TRIM_MEMORY_RUNNING_LOW,
        // Foreground: The device is running extremely low on memory. The app is not yet considered a killable
        // process, but the system will begin killing background processes if apps do not release resources.
        ComponentCallbacks2.TRIM_MEMORY_RUNNING_CRITICAL -> true

        // Background: The system is running low on memory and our process is near the middle of the LRU list.
        // If the system becomes further constrained for memory, there's a chance our process will be killed.
        ComponentCallbacks2.TRIM_MEMORY_MODERATE,
        // Background: The system is running low on memory and our process is one of the first to be killed
        // if the system does not recover memory now.
        ComponentCallbacks2.TRIM_MEMORY_COMPLETE -> true

        else -> false
    }
}

private fun shouldCloseEngineSessions(level: Int): Boolean {
    return when (level) {
        // Foreground: The device is running extremely low on memory. The app is not yet considered a killable
        // process, but the system will begin killing background processes if apps do not release resources.
        ComponentCallbacks2.TRIM_MEMORY_RUNNING_CRITICAL -> true

        // Background: The system is running low on memory and our process is near the middle of the LRU list.
        // If the system becomes further constrained for memory, there's a chance our process will be killed.
        ComponentCallbacks2.TRIM_MEMORY_MODERATE,
        // Background: The system is running low on memory and our process is one of the first to be killed
        // if the system does not recover memory now.
        ComponentCallbacks2.TRIM_MEMORY_COMPLETE -> true

        else -> false
    }
}
