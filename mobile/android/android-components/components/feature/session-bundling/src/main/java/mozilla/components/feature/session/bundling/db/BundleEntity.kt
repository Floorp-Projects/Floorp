/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

package mozilla.components.feature.session.bundling.db

import android.arch.persistence.room.ColumnInfo
import android.arch.persistence.room.Entity
import android.arch.persistence.room.PrimaryKey
import mozilla.components.browser.session.SessionManager
import mozilla.components.browser.session.storage.SnapshotSerializer
import mozilla.components.concept.engine.Engine
import mozilla.components.feature.session.bundling.SessionBundle
import org.json.JSONException
import java.io.IOException
import java.util.concurrent.TimeUnit

/**
 * Internal entity representing a session bundle as it gets saved to the database. This class implements [SessionBundle]
 * which only exposes the part of the API we want to make public.
 */
@Entity(tableName = "bundles")
internal data class BundleEntity(
    @PrimaryKey(autoGenerate = true)
    @ColumnInfo(name = "id")
    var id: Long?,

    @ColumnInfo(name = "state")
    var state: String,

    @ColumnInfo(name = "saved_at")
    var savedAt: Long,

    @ColumnInfo(name = "urls")
    var urls: UrlList
) {
    /**
     * Updates this entity with the value from the given snapshot.
     */
    fun updateFrom(snapshot: SessionManager.Snapshot): BundleEntity {
        state = SnapshotSerializer().toJSON(snapshot)
        savedAt = System.currentTimeMillis()
        urls = UrlList(snapshot.sessions.map { it.session.url })
        return this
    }
}

internal data class UrlList(
    val entries: List<String>
)
