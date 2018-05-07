/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

package mozilla.components.feature.session

import android.content.Context
import mozilla.components.browser.session.Session
import mozilla.components.browser.session.SessionManager
import mozilla.components.concept.engine.Engine
import mozilla.components.concept.engine.EngineSession
import java.util.WeakHashMap
import java.util.concurrent.Executors
import java.util.concurrent.ScheduledExecutorService
import java.util.concurrent.TimeUnit

/**
 * Manages access to active browser / engine sessions and their persistent state.
 */
class SessionProvider(
    private val context: Context,
    initialSession: Session = Session(""),
    private val storage: SessionStorage = DefaultSessionStorage(context),
    private val savePeriodically: Boolean = false,
    private val saveIntervalInSeconds: Long = 300,
    private val scheduler: ScheduledExecutorService = Executors.newSingleThreadScheduledExecutor()
) {
    private val sessions = WeakHashMap<Session, EngineSession>()
    val sessionManager: SessionManager = SessionManager(initialSession)

    val selectedSession
        get() = sessionManager.selectedSession

    /**
     * Starts this provider and schedules periodic saves.
     */
    fun start(engine: Engine) {
        storage.restore(engine).forEach {
            sessionManager.add(it.session)
            sessions.put(it.session, it.engineSession)
        }
        storage.getSelected()?.let { sessionManager.select(it) }

        if (savePeriodically) {
            scheduler.scheduleAtFixedRate(
                { storage.persist(sessionManager.selectedSession) },
                saveIntervalInSeconds,
                saveIntervalInSeconds,
                TimeUnit.SECONDS)
        }
    }

    /**
     * Returns the engine session corresponding to the current or given browser session. A new
     * engine session will be created if none exists for the given browser session.
     */
    @Synchronized
    fun getOrCreateEngineSession(engine: Engine, session: Session = selectedSession): EngineSession {
        return sessions.getOrPut(session, {
            val engineSession = engine.createSession()
            storage.add(SessionProxy(session, engineSession))

            engineSession.loadUrl(session.url)
            engineSession
        })
    }

    /**
     * Stops this provider and periodic saves.
     */
    fun stop() {
        scheduler.shutdown()
    }
}
