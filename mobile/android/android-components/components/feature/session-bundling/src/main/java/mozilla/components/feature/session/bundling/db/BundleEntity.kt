/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

@file:Suppress("DEPRECATION")

package mozilla.components.feature.session.bundling.db

import android.content.Context
import android.util.AtomicFile
import androidx.annotation.VisibleForTesting
import androidx.annotation.VisibleForTesting.PRIVATE
import androidx.annotation.WorkerThread
import androidx.room.ColumnInfo
import androidx.room.Entity
import androidx.room.PrimaryKey
import mozilla.components.browser.session.SessionManager
import mozilla.components.concept.engine.Engine
import mozilla.components.feature.session.bundling.SessionBundle
import mozilla.components.feature.session.bundling.SessionBundleStorage
import java.io.File
import java.util.UUID

/**
 * Internal entity representing a session bundle as it gets saved to the database. This class implements [SessionBundle]
 * which only exposes the part of the API we want to make public.
 */
@Entity(tableName = "bundles")
internal data class BundleEntity(
    @PrimaryKey(autoGenerate = true)
    @ColumnInfo(name = "id")
    var id: Long?,

    @ColumnInfo(name = "saved_at")
    var savedAt: Long,

    @ColumnInfo(name = "urls")
    var urls: UrlList,

    @ColumnInfo(name = "file")
    val file: String = UUID.randomUUID().toString()
) {
    /**
     * Updates this entity with the value from the given snapshot.
     */
    fun updateFrom(snapshot: SessionManager.Snapshot): BundleEntity {
        savedAt = System.currentTimeMillis()
        urls = UrlList(snapshot.sessions.map { it.session.url })
        return this
    }

    @WorkerThread
    fun stateFile(context: Context, engine: Engine): AtomicFile {
        return AtomicFile(statePath(context, engine))
    }

    @VisibleForTesting(otherwise = PRIVATE)
    internal fun statePath(context: Context, engine: Engine) =
        File(SessionBundleStorage.getStateDirectory(context), engine.name() + '_' + file)
}

internal data class UrlList(
    val entries: List<String>
)
