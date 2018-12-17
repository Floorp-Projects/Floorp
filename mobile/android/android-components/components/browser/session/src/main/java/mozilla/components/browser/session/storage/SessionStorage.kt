/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

package mozilla.components.browser.session.storage

import android.arch.lifecycle.Lifecycle
import android.arch.lifecycle.LifecycleObserver
import android.arch.lifecycle.OnLifecycleEvent
import android.arch.lifecycle.ProcessLifecycleOwner
import android.content.Context
import android.os.SystemClock
import android.support.annotation.CheckResult
import android.support.annotation.VisibleForTesting
import android.support.annotation.WorkerThread
import android.util.AtomicFile
import kotlinx.coroutines.Dispatchers
import kotlinx.coroutines.GlobalScope
import kotlinx.coroutines.Job
import kotlinx.coroutines.delay
import kotlinx.coroutines.launch
import kotlinx.coroutines.runBlocking
import mozilla.components.browser.session.SelectionAwareSessionObserver
import mozilla.components.browser.session.Session
import mozilla.components.browser.session.SessionManager
import mozilla.components.concept.engine.Engine
import mozilla.components.support.base.log.logger.Logger
import org.json.JSONArray
import org.json.JSONException
import org.json.JSONObject
import java.io.File
import java.io.FileOutputStream
import java.io.IOException
import java.util.concurrent.Executors
import java.util.concurrent.ScheduledExecutorService
import java.util.concurrent.ScheduledFuture
import java.util.concurrent.TimeUnit

private const val STORE_FILE_NAME_FORMAT = "mozilla_components_session_storage_%s.json"

// Current version of the format used [SessionStorage].
private const val VERSION = 1

// Minimum interval between saving states.
private const val DEFAULT_INTERVAL_MILLISECONDS = 2000L

private val sessionFileLock = Any()

/**
 * Session storage for persisting the state of a [SessionManager] to disk (browser and engine session states).
 */
class SessionStorage(
    private val context: Context,
    private val engine: Engine
) {
    /**
     * Reads the saved state from disk. Returns null if no state was found on disk or if reading the file failed.
     */
    fun restore(): SessionManager.Snapshot? {
        return readSnapshotFromDisk(getFileForEngine(context, engine), engine)
    }

    /**
     * Clears the state saved on disk.
     */
    fun clear() {
        removeSnapshotFromDisk(context, engine)
    }

    /**
     * Saves the given state to disk.
     */
    @WorkerThread
    fun save(snapshot: SessionManager.Snapshot): Boolean {
        if (snapshot.isEmpty()) {
            clear()
            return true
        }

        return saveSnapshotToDisk(getFileForEngine(context, engine), snapshot)
    }

    /**
     * Starts configuring automatic saving of the state.
     */
    @CheckResult
    fun autoSave(
        sessionManager: SessionManager,
        interval: Long = DEFAULT_INTERVAL_MILLISECONDS,
        unit: TimeUnit = TimeUnit.MILLISECONDS
    ): AutoSave {
        return AutoSave(sessionManager, this, unit.toMillis(interval))
    }
}

class AutoSave(
    private val sessionManager: SessionManager,
    private val sessionStorage: SessionStorage,
    private val minimumIntervalMs: Long
) {
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
        lifecycle.addObserver(AutoSavePeriodically(this, scheduler, interval, unit))
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
                if (snapshot != null) {
                    sessionStorage.save(snapshot)
                }
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

        runBlocking {
            // Since the app is going to the background and can get killed at any time, we do not want to delay saving
            // the state to disk.
            autoSave.triggerSave(delaySave = false).join()
        }
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
}

@VisibleForTesting(otherwise = VisibleForTesting.PRIVATE)
@Suppress("ReturnCount")
internal fun readSnapshotFromDisk(file: AtomicFile, engine: Engine): SessionManager.Snapshot? {
    synchronized(sessionFileLock) {
        val tuples: MutableList<SessionManager.Snapshot.Item> = mutableListOf()
        var selectedSessionIndex = 0

        try {
            file.openRead().use {
                val json = it.bufferedReader().use { reader -> reader.readText() }

                val jsonRoot = JSONObject(json)
                selectedSessionIndex = jsonRoot.getInt(Keys.SELECTED_SESSION_INDEX_KEY)
                val sessionStateTuples = jsonRoot.getJSONArray(Keys.SESSION_STATE_TUPLES_KEY)
                for (i in 0 until sessionStateTuples.length()) {
                    val sessionStateTupleJson = sessionStateTuples.getJSONObject(i)
                    val session = deserializeSession(sessionStateTupleJson.getJSONObject(Keys.SESSION_KEY))
                    val state = engine.createSessionState(sessionStateTupleJson.getJSONObject(Keys.ENGINE_SESSION_KEY))

                    tuples.add(SessionManager.Snapshot.Item(session, engineSession = null, engineSessionState = state))
                }
            }
        } catch (_: IOException) {
            return null
        } catch (_: JSONException) {
            return null
        }

        if (tuples.isEmpty()) {
            return null
        }

        // If we see an illegal selected index on disk, reset it to 0.
        if (tuples.getOrNull(selectedSessionIndex) == null) {
            selectedSessionIndex = 0
        }

        return SessionManager.Snapshot(
            sessions = tuples,
            selectedSessionIndex = selectedSessionIndex
        )
    }
}

@VisibleForTesting(otherwise = VisibleForTesting.PRIVATE)
internal fun saveSnapshotToDisk(file: AtomicFile, snapshot: SessionManager.Snapshot): Boolean {
    require(snapshot.sessions.isNotEmpty()) {
        "SessionsSnapshot must not be empty"
    }
    requireNotNull(snapshot.sessions.getOrNull(snapshot.selectedSessionIndex)) {
        "SessionSnapshot's selected index must be in bounds"
    }

    synchronized(sessionFileLock) {
        var outputStream: FileOutputStream? = null

        return try {
            val json = JSONObject()
            json.put(Keys.VERSION_KEY, VERSION)
            json.put(Keys.SELECTED_SESSION_INDEX_KEY, snapshot.selectedSessionIndex)

            val sessions = JSONArray()
            snapshot.sessions.forEachIndexed { index, sessionWithState ->
                val sessionJson = JSONObject()
                sessionJson.put(Keys.SESSION_KEY, serializeSession(sessionWithState.session))

                val engineSessionState = if (sessionWithState.engineSessionState != null) {
                    sessionWithState.engineSessionState.toJSON()
                } else {
                    sessionWithState.engineSession?.saveState()?.toJSON() ?: JSONObject()
                }

                sessionJson.put(Keys.ENGINE_SESSION_KEY, engineSessionState)

                sessions.put(index, sessionJson)
            }
            json.put(Keys.SESSION_STATE_TUPLES_KEY, sessions)

            outputStream = file.startWrite()
            outputStream.write(json.toString().toByteArray())
            file.finishWrite(outputStream)
            true
        } catch (_: IOException) {
            file.failWrite(outputStream)
            false
        } catch (_: JSONException) {
            file.failWrite(outputStream)
            false
        }
    }
}

private fun removeSnapshotFromDisk(context: Context, engine: Engine) {
    synchronized(sessionFileLock) {
        getFileForEngine(context, engine).delete()
    }
}

@VisibleForTesting(otherwise = VisibleForTesting.PRIVATE)
internal fun getFileForEngine(context: Context, engine: Engine): AtomicFile {
    return AtomicFile(File(context.filesDir, String.format(STORE_FILE_NAME_FORMAT, engine.name()).toLowerCase()))
}

@Throws(JSONException::class)
private fun serializeSession(session: Session): JSONObject {
    return JSONObject().apply {
        put(Keys.SESSION_URL_KEY, session.url)
        put(Keys.SESSION_SOURCE_KEY, session.source.name)
        put(Keys.SESSION_UUID_KEY, session.id)
        put(Keys.SESSION_PARENT_UUID_KEY, session.parentId ?: "")
    }
}

@Throws(JSONException::class)
@VisibleForTesting(otherwise = VisibleForTesting.PRIVATE)
internal fun deserializeSession(json: JSONObject): Session {
    val source = try {
        Session.Source.valueOf(json.getString(Keys.SESSION_SOURCE_KEY))
    } catch (e: IllegalArgumentException) {
        Session.Source.NONE
    }
    val session = Session(
        json.getString(Keys.SESSION_URL_KEY),
        // Currently, snapshot cannot contain private sessions.
        false,
        source,
        json.getString(Keys.SESSION_UUID_KEY)
    )
    session.parentId = json.getString(Keys.SESSION_PARENT_UUID_KEY).takeIf { it != "" }
    return session
}

private object Keys {
    const val SELECTED_SESSION_INDEX_KEY = "selectedSessionIndex"
    const val SESSION_STATE_TUPLES_KEY = "sessionStateTuples"

    const val SESSION_SOURCE_KEY = "source"
    const val SESSION_URL_KEY = "url"
    const val SESSION_UUID_KEY = "uuid"
    const val SESSION_PARENT_UUID_KEY = "parentUuid"

    const val SESSION_KEY = "session"
    const val ENGINE_SESSION_KEY = "engineSession"

    const val VERSION_KEY = "version"
}
