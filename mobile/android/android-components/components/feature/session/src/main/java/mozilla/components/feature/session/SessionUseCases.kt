/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

package mozilla.components.feature.session

import mozilla.components.browser.session.Session
import mozilla.components.browser.session.SessionManager
import mozilla.components.browser.state.action.CrashAction
import mozilla.components.browser.state.action.EngineAction
import mozilla.components.browser.state.selector.findTabOrCustomTab
import mozilla.components.browser.state.store.BrowserStore
import mozilla.components.concept.engine.Engine.BrowsingData
import mozilla.components.concept.engine.EngineSession.LoadUrlFlags

/**
 * Contains use cases related to the session feature.
 *
 * @param sessionManager the application's [SessionManager].
 * @param onNoSession When invoking a use case that requires a (selected) [Session] and when no [Session] is available
 * this (optional) lambda will be invoked to create a [Session]. The default implementation creates a [Session] and adds
 * it to the [SessionManager].
 */
class SessionUseCases(
    store: BrowserStore,
    sessionManager: SessionManager,
    onNoSession: (String) -> Session = { url ->
        Session(url).apply { sessionManager.add(this) }
    }
) {

    /**
     * Contract for use cases that load a provided URL.
     */
    interface LoadUrlUseCase {
        /**
         * Loads the provided URL using the currently selected session.
         */
        fun invoke(
            url: String,
            flags: LoadUrlFlags = LoadUrlFlags.none(),
            additionalHeaders: Map<String, String>? = null
        )
    }

    class DefaultLoadUrlUseCase internal constructor(
        private val store: BrowserStore,
        private val sessionManager: SessionManager,
        private val onNoSession: (String) -> Session
    ) : LoadUrlUseCase {

        /**
         * Loads the provided URL using the currently selected session. If
         * there's no selected session a new session will be created using
         * [onNoSession].
         *
         * @param url The URL to be loaded using the selected session.
         * @param flags The [LoadUrlFlags] to use when loading the provided url.
         * @param additionalHeaders the extra headers to use when loading the provided url.
         */
        override operator fun invoke(
            url: String,
            flags: LoadUrlFlags,
            additionalHeaders: Map<String, String>?
        ) {
            this.invoke(url, sessionManager.selectedSession, flags, additionalHeaders)
        }

        /**
         * Loads the provided URL using the specified session. If no session
         * is provided the currently selected session will be used. If there's
         * no selected session a new session will be created using [onNoSession].
         *
         * @param url The URL to be loaded using the provided session.
         * @param session the session for which the URL should be loaded.
         * @param flags The [LoadUrlFlags] to use when loading the provided url.
         * @param additionalHeaders the extra headers to use when loading the provided url.
         */
        operator fun invoke(
            url: String,
            session: Session? = sessionManager.selectedSession,
            flags: LoadUrlFlags = LoadUrlFlags.none(),
            additionalHeaders: Map<String, String>? = null
        ) {
            val loadSession = session ?: onNoSession.invoke(url)

            store.dispatch(EngineAction.LoadUrlAction(
                loadSession.id,
                url,
                flags,
                additionalHeaders
            ))
        }
    }

    class LoadDataUseCase internal constructor(
        private val store: BrowserStore,
        private val sessionManager: SessionManager,
        private val onNoSession: (String) -> Session
    ) {
        /**
         * Loads the provided data based on the mime type using the provided session (or the
         * currently selected session if none is provided).
         */
        operator fun invoke(
            data: String,
            mimeType: String,
            encoding: String = "UTF-8",
            session: Session? = sessionManager.selectedSession
        ) {
            val loadSession = session ?: onNoSession.invoke("about:blank")

            store.dispatch(EngineAction.LoadDataAction(
                loadSession.id,
                data,
                mimeType,
                encoding
            ))
        }
    }

    class ReloadUrlUseCase internal constructor(
        private val store: BrowserStore,
        private val sessionManager: SessionManager
    ) {
        /**
         * Reloads the current URL of the provided session (or the currently
         * selected session if none is provided).
         *
         * @param session the session for which reload should be triggered.
         * @param flags the [LoadUrlFlags] to use when reloading the given session.
         */
        operator fun invoke(
            session: Session? = sessionManager.selectedSession,
            flags: LoadUrlFlags = LoadUrlFlags.none()
        ) {
            if (session == null) {
                return
            }

            store.dispatch(EngineAction.ReloadAction(
                session.id,
                flags
            ))
        }

        /**
         * Reloads the current page of the tab with the given [tabId].
         *
         * @param tabId id of the tab that should be reloaded
         * @param flags the [LoadUrlFlags] to use when reloading the given tabId.
         */
        operator fun invoke(tabId: String, flags: LoadUrlFlags = LoadUrlFlags.none()) {
            sessionManager.findSessionById(tabId)?.let {
                invoke(it, flags)
            }
        }
    }

    class StopLoadingUseCase internal constructor(
        private val store: BrowserStore,
        private val sessionManager: SessionManager
    ) {
        /**
         * Stops the current URL of the provided session from loading.
         *
         * @param session the session for which loading should be stopped.
         */
        operator fun invoke(session: Session? = sessionManager.selectedSession) {
            if (session == null) {
                return
            }

            store.state.findTabOrCustomTab(session.id)
                ?.engineState
                ?.engineSession
                ?.stopLoading()
        }
    }

    class GoBackUseCase internal constructor(
        private val store: BrowserStore,
        private val sessionManager: SessionManager
    ) {
        /**
         * Navigates back in the history of the currently selected session
         */
        operator fun invoke(session: Session? = sessionManager.selectedSession) {
            if (session == null) {
                return
            }

            store.dispatch(EngineAction.GoBackAction(
                session.id
            ))
        }

        /**
         * Navigates back in the history of the tab with the given [tabId].
         */
        operator fun invoke(tabId: String) {
            sessionManager.findSessionById(tabId)?.let {
                invoke(it)
            }
        }
    }

    class GoForwardUseCase internal constructor(
        private val store: BrowserStore,
        private val sessionManager: SessionManager
    ) {
        /**
         * Navigates forward in the history of the currently selected session
         */
        operator fun invoke(session: Session? = sessionManager.selectedSession) {
            if (session == null) {
                return
            }

            store.dispatch(EngineAction.GoForwardAction(
                session.id
            ))
        }
    }

    /**
     * Use case to jump to an arbitrary history index in a session's backstack.
     */
    class GoToHistoryIndexUseCase internal constructor(
        private val store: BrowserStore,
        private val sessionManager: SessionManager
    ) {
        /**
         * Navigates to a specific index in the [HistoryState] of the given session.
         * Invalid index values will be ignored.
         *
         * @param index the index in the session's [HistoryState] to navigate to
         * @param session the session whose [HistoryState] is being accessed
         */
        operator fun invoke(index: Int, session: Session? = sessionManager.selectedSession) {
            if (session == null) {
                return
            }

            store.dispatch(EngineAction.GoToHistoryIndexAction(
                session.id,
                index
            ))
        }
    }

    class RequestDesktopSiteUseCase internal constructor(
        private val store: BrowserStore,
        private val sessionManager: SessionManager
    ) {
        /**
         * Requests the desktop version of the current session and reloads the page.
         */
        operator fun invoke(enable: Boolean, session: Session? = sessionManager.selectedSession) {
            if (session == null) {
                return
            }

            store.dispatch(EngineAction.ToggleDesktopModeAction(
                session.id,
                enable
            ))
        }
    }

    class ExitFullScreenUseCase internal constructor(
        private val store: BrowserStore,
        private val sessionManager: SessionManager
    ) {
        /**
         * Exits fullscreen mode of the current session.
         */
        operator fun invoke(session: Session? = sessionManager.selectedSession) {
            if (session == null) {
                return
            }

            store.dispatch(EngineAction.ExitFullScreenModeAction(
                session.id
            ))
        }

        /**
         * Exits fullscreen mode of the tab with the given [tabId].
         */
        operator fun invoke(tabId: String) {
            sessionManager.findSessionById(tabId)?.let { invoke(it) }
        }
    }

    class ClearDataUseCase internal constructor(
        private val store: BrowserStore,
        private val sessionManager: SessionManager
    ) {
        /**
         * Clears all user data sources available.
         */
        operator fun invoke(
            session: Session? = sessionManager.selectedSession,
            data: BrowsingData = BrowsingData.all()
        ) {
            sessionManager.engine.clearData(data)

            if (session == null) {
                return
            }

            store.dispatch(EngineAction.ClearDataAction(
                session.id,
                data
            ))
        }
    }

    /**
     * Tries to recover from a crash by restoring the last know state.
     */
    class CrashRecoveryUseCase internal constructor(
        private val store: BrowserStore
    ) {
        /**
         * Tries to recover the state of all crashed sessions.
         */
        fun invoke() {
            val tabIds = store.state.let {
                it.tabs + it.customTabs
            }.filter {
                it.crashed
            }.map {
                it.id
            }

            return invoke(tabIds)
        }

        /**
         * Tries to recover the state of all sessions.
         */
        fun invoke(tabIds: List<String>) {
            tabIds.forEach { tabId ->
                store.dispatch(
                    CrashAction.RestoreCrashedSessionAction(tabId)
                )
            }
        }
    }

    val loadUrl: DefaultLoadUrlUseCase by lazy { DefaultLoadUrlUseCase(store, sessionManager, onNoSession) }
    val loadData: LoadDataUseCase by lazy { LoadDataUseCase(store, sessionManager, onNoSession) }
    val reload: ReloadUrlUseCase by lazy { ReloadUrlUseCase(store, sessionManager) }
    val stopLoading: StopLoadingUseCase by lazy { StopLoadingUseCase(store, sessionManager) }
    val goBack: GoBackUseCase by lazy { GoBackUseCase(store, sessionManager) }
    val goForward: GoForwardUseCase by lazy { GoForwardUseCase(store, sessionManager) }
    val goToHistoryIndex: GoToHistoryIndexUseCase by lazy { GoToHistoryIndexUseCase(store, sessionManager) }
    val requestDesktopSite: RequestDesktopSiteUseCase by lazy { RequestDesktopSiteUseCase(store, sessionManager) }
    val exitFullscreen: ExitFullScreenUseCase by lazy { ExitFullScreenUseCase(store, sessionManager) }
    val clearData: ClearDataUseCase by lazy { ClearDataUseCase(store, sessionManager) }
    val crashRecovery: CrashRecoveryUseCase by lazy { CrashRecoveryUseCase(store) }
}
