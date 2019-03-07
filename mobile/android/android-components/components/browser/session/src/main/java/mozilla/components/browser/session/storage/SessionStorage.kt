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
import mozilla.components.browser.session.ext.writeSnapshot
import mozilla.components.concept.engine.Engine
import java.io.File
import java.util.concurrent.TimeUnit

private const val STORE_FILE_NAME_FORMAT = "mozilla_components_session_storage_%s.json"

private val sessionFileLock = Any()

/**
 * Session storage for persisting the state of a [SessionManager] to disk (browser and engine session states).
 */
class SessionStorage(
    private val context: Context,
    private val engine: Engine
) : AutoSave.Storage {
    private val serializer = SnapshotSerializer()

    /**
     * Reads the saved state from disk. Returns null if no state was found on disk or if reading the file failed.
     */
    @WorkerThread
    fun restore(): SessionManager.Snapshot? {
        synchronized(sessionFileLock) {
            return getFileForEngine(context, engine)
                .readSnapshot(engine, serializer)
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
    override fun save(snapshot: SessionManager.Snapshot): Boolean {
        if (snapshot.isEmpty()) {
            clear()
            return true
        }

        requireNotNull(snapshot.sessions.getOrNull(snapshot.selectedSessionIndex)) {
            "SessionSnapshot's selected index must be in bounds"
        }

        synchronized(sessionFileLock) {
            return getFileForEngine(context, engine)
                .writeSnapshot(snapshot, serializer)
        }
    }

    /**
     * Starts configuring automatic saving of the state.
     */
    @CheckResult
    fun autoSave(
        sessionManager: SessionManager,
        interval: Long = AutoSave.DEFAULT_INTERVAL_MILLISECONDS,
        unit: TimeUnit = TimeUnit.MILLISECONDS
    ): AutoSave {
        return AutoSave(sessionManager, this, unit.toMillis(interval))
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
    return AtomicFile(File(context.filesDir, String.format(STORE_FILE_NAME_FORMAT, engine.name()).toLowerCase()))
}
