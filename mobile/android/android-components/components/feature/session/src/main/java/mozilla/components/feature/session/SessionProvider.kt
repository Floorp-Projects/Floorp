/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

package mozilla.components.feature.session

import android.content.Context
import mozilla.components.browser.session.Session
import mozilla.components.browser.session.SessionManager
import mozilla.components.concept.engine.Engine
import mozilla.components.concept.engine.EngineSession
import java.util.concurrent.Executors
import java.util.concurrent.ScheduledExecutorService
import java.util.concurrent.TimeUnit

/**
 * Manages access to active browser / engine sessions and their persistent state.
 */
class SessionProvider(
    private val context: Context,
    initialSession: Session = Session(""),
    private val sessionStorage: SessionStorage = DefaultSessionStorage(context),
    private val savePeriodically: Boolean = false,
    private val saveIntervalInSeconds: Long = 300,
    private val scheduler: ScheduledExecutorService = Executors.newSingleThreadScheduledExecutor()
) {
    private val sessions = mutableMapOf<Session, EngineSession>()

    val sessionManager: SessionManager = SessionManager(initialSession)

    val selectedSession
        get() = sessionManager.selectedSession

    /**
     * Restores persisted session state and schedules periodic saves.
     */
    fun start(engine: Engine) {
        val (restoredSessions, restoredSelectedSession) = sessionStorage.restore(engine)
        sessions.putAll(restoredSessions)
        sessions.keys.forEach {
            sessionManager.add(it, it.id == restoredSelectedSession)
        }

        if (savePeriodically) {
            scheduler.scheduleAtFixedRate(
                { sessionStorage.persist(sessions, sessionManager.selectedSession?.id) },
                saveIntervalInSeconds,
                saveIntervalInSeconds,
                TimeUnit.SECONDS)
        }
    }

    /**
     * Returns the engine session corresponding to the given (or currently selected)
     * browser session. A new engine session will be created if none exists for the
     * given browser session.
     */
    @Synchronized
    fun getOrCreateEngineSession(engine: Engine, session: Session = selectedSession): EngineSession {
        return sessions.getOrPut(session, {
            val engineSession = engine.createSession()
            SessionProxy(session, engineSession)

            engineSession.loadUrl(session.url)
            engineSession
        })
    }

    /**
     * Stops this provider and periodic saves.
     */
    fun stop() {
        sessions.clear()
        scheduler.shutdown()
    }
}
