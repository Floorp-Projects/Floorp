/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

package mozilla.components.feature.session

import mozilla.components.browser.session.Session
import mozilla.components.browser.session.SessionManager
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
    sessionManager: SessionManager,
    onNoSession: (String) -> Session = { url ->
        Session(url).apply { sessionManager.add(this) }
    }
) {

    /**
     * Contract for use cases that load a provided URL.
     */
    interface LoadUrlUseCase {
        fun invoke(url: String, flags: LoadUrlFlags = LoadUrlFlags.none())
    }

    class DefaultLoadUrlUseCase internal constructor(
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
         */
        override operator fun invoke(url: String, flags: LoadUrlFlags) {
            this.invoke(url, sessionManager.selectedSession, flags)
        }

        /**
         * Loads the provided URL using the specified session. If no session
         * is provided the currently selected session will be used. If there's
         * no selected session a new session will be created using [onNoSession].
         *
         * @param url The URL to be loaded using the provided session.
         * @param session the session for which the URL should be loaded.
         * @param flags The [LoadUrlFlags] to use when loading the provided url.
         */
        operator fun invoke(
            url: String,
            session: Session? = sessionManager.selectedSession,
            flags: LoadUrlFlags = LoadUrlFlags.none()
        ) {
            val loadSession = session ?: onNoSession.invoke(url)
            sessionManager.getOrCreateEngineSession(loadSession).loadUrl(url, flags)
        }
    }

    class LoadDataUseCase internal constructor(
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
            sessionManager.getOrCreateEngineSession(loadSession).loadData(data, mimeType, encoding)
        }
    }

    class ReloadUrlUseCase internal constructor(
        private val sessionManager: SessionManager
    ) {
        /**
         * Reloads the current URL of the provided session (or the currently
         * selected session if none is provided).
         *
         * @param session the session for which reload should be triggered.
         */
        operator fun invoke(session: Session? = sessionManager.selectedSession) {
            if (session != null) {
                sessionManager.getOrCreateEngineSession(session).reload()
            }
        }
    }

    class StopLoadingUseCase internal constructor(
        private val sessionManager: SessionManager
    ) {
        /**
         * Stops the current URL of the provided session from loading.
         *
         * @param session the session for which loading should be stopped.
         */
        operator fun invoke(session: Session? = sessionManager.selectedSession) {
            if (session != null) {
                sessionManager.getOrCreateEngineSession(session).stopLoading()
            }
        }
    }

    class GoBackUseCase internal constructor(
        private val sessionManager: SessionManager
    ) {
        /**
         * Navigates back in the history of the currently selected session
         */
        operator fun invoke(session: Session? = sessionManager.selectedSession) {
            if (session != null) {
                sessionManager.getOrCreateEngineSession(session).goBack()
            }
        }
    }

    class GoForwardUseCase internal constructor(
        private val sessionManager: SessionManager
    ) {
        /**
         * Navigates forward in the history of the currently selected session
         */
        operator fun invoke(session: Session? = sessionManager.selectedSession) {
            if (session != null) {
                sessionManager.getOrCreateEngineSession(session).goForward()
            }
        }
    }

    class RequestDesktopSiteUseCase internal constructor(
        private val sessionManager: SessionManager
    ) {
        /**
         * Requests the desktop version of the current session and reloads the page.
         */
        operator fun invoke(enable: Boolean, session: Session? = sessionManager.selectedSession) {
            if (session != null) {
                sessionManager.getOrCreateEngineSession(session).toggleDesktopMode(enable, true)
            }
        }
    }

    class ExitFullScreenUseCase internal constructor(
        private val sessionManager: SessionManager
    ) {
        /**
         * Exits fullscreen mode of the current session.
         */
        operator fun invoke(session: Session? = sessionManager.selectedSession) {
            if (session != null) {
                sessionManager.getOrCreateEngineSession(session).exitFullScreenMode()
            }
        }
    }

    class ClearDataUseCase internal constructor(
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
            if (session != null) {
                sessionManager.getOrCreateEngineSession(session).clearData(data)
            }
        }
    }

    /**
     * Tries to recover from a crash by restoring the last know state.
     *
     * Executing this use case on a [Session] will clear the [Session.crashed] flag.
     */
    class CrashRecoveryUseCase internal constructor(
        private val sessionManager: SessionManager
    ) {
        /**
         * Tries to recover the state of the provided [Session].
         */
        fun invoke(session: Session): Boolean = invoke(listOf(session))

        /**
         * Tries to recover the state of all crashed [Session]s (with [Session.crashed] flag set).
         */
        fun invoke(): Boolean {
            return invoke(sessionManager.sessions.filter { it.crashed })
        }

        /**
         * Tries to recover the state of all [sessions].
         */
        fun invoke(sessions: List<Session>): Boolean {
            return sessions.fold(true) { recovered, session ->
                val sessionRecovered = sessionManager.getOrCreateEngineSession(session)
                    .recoverFromCrash()

                session.crashed = false

                sessionRecovered && recovered
            }
        }
    }

    val loadUrl: DefaultLoadUrlUseCase by lazy { DefaultLoadUrlUseCase(sessionManager, onNoSession) }
    val loadData: LoadDataUseCase by lazy { LoadDataUseCase(sessionManager, onNoSession) }
    val reload: ReloadUrlUseCase by lazy { ReloadUrlUseCase(sessionManager) }
    val stopLoading: StopLoadingUseCase by lazy { StopLoadingUseCase(sessionManager) }
    val goBack: GoBackUseCase by lazy { GoBackUseCase(sessionManager) }
    val goForward: GoForwardUseCase by lazy { GoForwardUseCase(sessionManager) }
    val requestDesktopSite: RequestDesktopSiteUseCase by lazy { RequestDesktopSiteUseCase(sessionManager) }
    val exitFullscreen: ExitFullScreenUseCase by lazy { ExitFullScreenUseCase(sessionManager) }
    val clearData: ClearDataUseCase by lazy { ClearDataUseCase(sessionManager) }
    val crashRecovery: CrashRecoveryUseCase by lazy { CrashRecoveryUseCase(sessionManager) }
}
