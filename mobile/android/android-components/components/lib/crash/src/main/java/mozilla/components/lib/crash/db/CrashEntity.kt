/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

package mozilla.components.lib.crash.db

import androidx.room.ColumnInfo
import androidx.room.Entity
import androidx.room.PrimaryKey
import mozilla.components.lib.crash.Crash
import mozilla.components.support.base.ext.getStacktraceAsString

/**
 * Database entity modeling a crash that has happened.
 */
@Entity(
    tableName = "crashes",
)
internal data class CrashEntity(
    /**
     * Generated UUID for this crash.
     */
    @PrimaryKey
    @ColumnInfo(name = "uuid")
    var uuid: String,

    /**
     * The stacktrace of the crash (if this crash was caused by an exception/throwable): otherwise
     * a string describing the type of crash.
     */
    @ColumnInfo(name = "stacktrace")
    var stacktrace: String,

    /**
     * Timestamp (in milliseconds) of when the crash happened.
     */
    @ColumnInfo(name = "created_at")
    var createdAt: Long,
)

internal fun Crash.toEntity(): CrashEntity {
    return CrashEntity(
        uuid = uuid,
        stacktrace = getStacktrace(),
        createdAt = System.currentTimeMillis(),
    )
}

internal fun Crash.getStacktrace(): String {
    return when (this) {
        is Crash.NativeCodeCrash -> "<native crash>"
        is Crash.UncaughtExceptionCrash -> throwable.getStacktraceAsString()
    }
}
