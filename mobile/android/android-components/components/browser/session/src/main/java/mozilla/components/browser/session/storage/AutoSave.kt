/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

package mozilla.components.browser.session.storage

import android.os.SystemClock
import androidx.annotation.VisibleForTesting
import androidx.lifecycle.Lifecycle
import androidx.lifecycle.LifecycleObserver
import androidx.lifecycle.OnLifecycleEvent
import androidx.lifecycle.ProcessLifecycleOwner
import kotlinx.coroutines.Dispatchers
import kotlinx.coroutines.GlobalScope
import kotlinx.coroutines.Job
import kotlinx.coroutines.delay
import kotlinx.coroutines.launch
import mozilla.components.browser.session.SelectionAwareSessionObserver
import mozilla.components.browser.session.Session
import mozilla.components.browser.session.SessionManager
import mozilla.components.support.base.log.logger.Logger
import java.util.concurrent.Executors
import java.util.concurrent.ScheduledExecutorService
import java.util.concurrent.ScheduledFuture
import java.util.concurrent.TimeUnit

class AutoSave(
    private val sessionManager: SessionManager,
    private val sessionStorage: Storage,
    private val minimumIntervalMs: Long
) {
    interface Storage {
        fun save(snapshot: SessionManager.Snapshot): Boolean
    }

    internal val logger = Logger("SessionStorage/AutoSave")
    internal var saveJob: Job? = null
    private var lastSaveTimestamp: Long = now()

    /**
     * Saves the state periodically when the app is in the foreground.
     *
     * @param interval The interval in which the state should be saved to disk.
     * @param unit The time unit of the [interval] parameter.
     */
    fun periodicallyInForeground(
        interval: Long = 300,
        unit: TimeUnit = TimeUnit.SECONDS,
        scheduler: ScheduledExecutorService = Executors.newSingleThreadScheduledExecutor(),
        lifecycle: Lifecycle = ProcessLifecycleOwner.get().lifecycle
    ): AutoSave {
        lifecycle.addObserver(
            AutoSavePeriodically(
                this,
                scheduler,
                interval,
                unit
            )
        )
        return this
    }

    /**
     * Saves the state automatically when the app goes to the background.
     */
    fun whenGoingToBackground(
        lifecycle: Lifecycle = ProcessLifecycleOwner.get().lifecycle
    ): AutoSave {
        lifecycle.addObserver(AutoSaveBackground(this))
        return this
    }

    /**
     * Saves the state automatically when the sessions change, e.g. sessions get added and removed.
     */
    fun whenSessionsChange(): AutoSave {
        AutoSaveSessionChange(
            this,
            sessionManager
        ).observeSelected()
        return this
    }

    /**
     * Triggers saving the current state to disk.
     *
     * This method will not schedule a new save job if a job is already in flight. Additionally it will obey the
     * interval passed to [SessionStorage.autoSave]; job may get delayed.
     *
     * @param delaySave Whether to delay the save job to obey the interval passed to [SessionStorage.autoSave].
     */
    @Synchronized
    internal fun triggerSave(delaySave: Boolean = true): Job {
        val currentJob = saveJob

        if (currentJob != null && currentJob.isActive) {
            logger.debug("Skipping save, other job already in flight")
            return currentJob
        }

        val now = now()
        val delayMs = lastSaveTimestamp + minimumIntervalMs - now
        lastSaveTimestamp = now

        GlobalScope.launch(Dispatchers.IO) {
            if (delaySave && delayMs > 0) {
                logger.debug("Delaying save (${delayMs}ms)")
                delay(delayMs)
            }

            val start = now()

            try {
                val snapshot = sessionManager.createSnapshot()
                sessionStorage.save(snapshot)
            } finally {
                val took = now() - start
                logger.debug("Saved state to disk [${took}ms]")
            }
        }.also {
            saveJob = it
            return it
        }
    }

    @VisibleForTesting(otherwise = VisibleForTesting.PRIVATE)
    internal fun now() = SystemClock.elapsedRealtime()

    companion object {
        // Minimum interval between saving states.
        const val DEFAULT_INTERVAL_MILLISECONDS = 2000L
    }
}

/**
 * [LifecycleObserver] to start/stop a task that saves the state at a periodic interval.
 */
private class AutoSavePeriodically(
    private val autoSave: AutoSave,
    private val scheduler: ScheduledExecutorService,
    private val interval: Long,
    private val unit: TimeUnit
) : LifecycleObserver {
    private var scheduledFuture: ScheduledFuture<*>? = null

    @OnLifecycleEvent(Lifecycle.Event.ON_START)
    fun start() {
        scheduledFuture = scheduler.scheduleAtFixedRate(
            {
                autoSave.logger.info("Save: Periodic")
                autoSave.triggerSave()
            },
            interval,
            interval,
            unit
        )
    }

    @OnLifecycleEvent(Lifecycle.Event.ON_STOP)
    fun stop() {
        scheduledFuture?.cancel(false)
    }
}

/**
 * [LifecycleObserver] to save the state when the app goes to the background.
 */
private class AutoSaveBackground(
    private val autoSave: AutoSave
) : LifecycleObserver {
    @OnLifecycleEvent(Lifecycle.Event.ON_STOP)
    fun stop() {
        autoSave.logger.info("Save: Background")

        autoSave.triggerSave(delaySave = false)
    }
}

/**
 * [SelectionAwareSessionObserver] to save the state whenever sessions change.
 */
private class AutoSaveSessionChange(
    private val autoSave: AutoSave,
    sessionManager: SessionManager
) : SelectionAwareSessionObserver(sessionManager) {
    override fun onSessionSelected(session: Session) {
        super.onSessionSelected(session)
        autoSave.logger.info("Save: Session selected")
        autoSave.triggerSave()
    }

    override fun onLoadingStateChanged(session: Session, loading: Boolean) {
        if (!loading) {
            autoSave.logger.info("Save: Load finished")
            autoSave.triggerSave()
        }
    }

    override fun onSessionAdded(session: Session) {
        autoSave.logger.info("Save: Session added")
        autoSave.triggerSave()
    }

    override fun onSessionRemoved(session: Session) {
        autoSave.logger.info("Save: Session removed")
        autoSave.triggerSave()
    }

    override fun onAllSessionsRemoved() {
        autoSave.logger.info("Save: All sessions removed")
        autoSave.triggerSave()
    }
}
