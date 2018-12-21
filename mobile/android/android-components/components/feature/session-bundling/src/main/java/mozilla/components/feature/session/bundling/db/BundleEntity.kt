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

/**
 * Internal entity representing a session bundle as it gets saved to the database. This class implements [SessionBundle]
 * which only exposes the part of the API we want to make public.
 */
@Entity(tableName = "bundles")
internal data class BundleEntity(
    @PrimaryKey(autoGenerate = true) var id: Long?,
    var state: String,
    @ColumnInfo(name = "saved_at") var savedAt: Long
) : SessionBundle {
    /**
     * Updates this entity with the value from the given snapshot.
     */
    fun updateFrom(snapshot: SessionManager.Snapshot): BundleEntity {
        state = SnapshotSerializer().toJSON(snapshot)
        savedAt = System.currentTimeMillis()
        return this
    }

    /**
     * Re-create the [SessionManager.Snapshot] from the state saved in the database.
     */
    override fun restoreSnapshot(engine: Engine): SessionManager.Snapshot? {
        return try {
            SnapshotSerializer().fromJSON(engine, state)
        } catch (e: IOException) {
            null
        } catch (e: JSONException) {
            null
        }
    }
}
