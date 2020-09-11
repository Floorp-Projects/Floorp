/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

package mozilla.components.browser.session.storage

import android.content.Context
import android.util.AtomicFile
import androidx.annotation.CheckResult
import androidx.annotation.VisibleForTesting
import androidx.annotation.WorkerThread
import mozilla.components.browser.session.SessionManager
import mozilla.components.browser.session.ext.readSnapshot
import mozilla.components.browser.state.selector.normalTabs
import mozilla.components.browser.state.selector.selectedTab
import mozilla.components.browser.state.state.BrowserState
import mozilla.components.browser.state.store.BrowserStore
import mozilla.components.concept.engine.Engine
import mozilla.components.concept.base.crash.CrashReporting
import mozilla.components.support.base.log.logger.Logger
import java.io.File
import java.util.Locale
import java.util.concurrent.TimeUnit

private const val STORE_FILE_NAME_FORMAT = "mozilla_components_session_storage_%s.json"

private val sessionFileLock = Any()

/**
 * Session storage for persisting the state of a [SessionManager] to disk (browser and engine session states).
 */
class SessionStorage(
    private val context: Context,
    private val engine: Engine,
    private val crashReporting: CrashReporting? = null
) : AutoSave.Storage {
    private val logger = Logger("SessionStorage")
    private val snapshotSerializer = SnapshotSerializer()
    private val stateSerializer = BrowserStateSerializer()

    /**
     * Reads the saved state from disk. Returns null if no state was found on disk or if reading the file failed.
     */
    @WorkerThread
    fun restore(): SessionManager.Snapshot? {
        synchronized(sessionFileLock) {
            return getFileForEngine(context, engine)
                .readSnapshot(engine, snapshotSerializer)
        }
    }

    /**
     * Clears the state saved on disk.
     */
    @WorkerThread
    fun clear() {
        removeSnapshotFromDisk(context, engine)
    }

    /**
     * Saves the given state to disk.
     */
    @WorkerThread
    override fun save(state: BrowserState): Boolean {
        if (state.normalTabs.isEmpty()) {
            clear()
            return true
        }

        if (state.selectedTabId != null) {
            requireNotNull(state.selectedTab) {
                "Selected session must exist"
            }
        }

        return synchronized(sessionFileLock) {
            try {
                val file = getFileForEngine(context, engine)
                stateSerializer.write(state, file)
            } catch (e: OutOfMemoryError) {
                crashReporting?.submitCaughtException(e)
                logger.error("Failed to save state to disk due to OutOfMemoryError", e)
                false
            }
        }
    }

    /**
     * Starts configuring automatic saving of the state.
     */
    @CheckResult
    fun autoSave(
        store: BrowserStore,
        interval: Long = AutoSave.DEFAULT_INTERVAL_MILLISECONDS,
        unit: TimeUnit = TimeUnit.MILLISECONDS
    ): AutoSave {
        return AutoSave(store, this, unit.toMillis(interval))
    }
}

private fun removeSnapshotFromDisk(context: Context, engine: Engine) {
    synchronized(sessionFileLock) {
        getFileForEngine(context, engine)
            .delete()
    }
}

@VisibleForTesting(otherwise = VisibleForTesting.PRIVATE)
internal fun getFileForEngine(context: Context, engine: Engine): AtomicFile {
    return AtomicFile(File(context.filesDir, String.format(STORE_FILE_NAME_FORMAT, engine.name())
        .toLowerCase(Locale.ROOT)))
}
