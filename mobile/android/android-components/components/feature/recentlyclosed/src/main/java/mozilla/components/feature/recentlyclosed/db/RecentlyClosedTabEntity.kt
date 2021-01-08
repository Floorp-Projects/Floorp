/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

package mozilla.components.feature.recentlyclosed.db

import android.util.AtomicFile
import androidx.room.ColumnInfo
import androidx.room.Entity
import androidx.room.PrimaryKey
import mozilla.components.browser.state.state.recover.RecoverableTab
import mozilla.components.concept.engine.Engine
import mozilla.components.concept.engine.EngineSessionState
import mozilla.components.support.ktx.util.readAndDeserialize
import org.json.JSONObject
import java.io.File

/**
 * Internal entity representing recently closed tabs.
 */
@Entity(
    tableName = "recently_closed_tabs"
)
internal data class RecentlyClosedTabEntity(
    /**
     * Generated UUID for this closed tab.
     */
    @PrimaryKey
    @ColumnInfo(name = "uuid")
    var uuid: String,

    @ColumnInfo(name = "title")
    var title: String,

    @ColumnInfo(name = "url")
    var url: String,

    @ColumnInfo(name = "created_at")
    var createdAt: Long
) {
    internal fun toRecoverableTab(filesDir: File, engine: Engine): RecoverableTab {
        return RecoverableTab(
            id = uuid,
            title = title,
            url = url,
            state = getEngineSessionState(engine, filesDir),
            lastAccess = createdAt
        )
    }

    private fun getEngineSessionState(engine: Engine, filesDir: File): EngineSessionState? {
        val jsonObject = getStateFile(filesDir).readAndDeserialize { json ->
            JSONObject(json)
        }
        return jsonObject?.let { engine.createSessionState(it) }
    }

    internal fun getStateFile(filesDir: File): AtomicFile {
        return AtomicFile(File(getStateDirectory(filesDir), uuid))
    }

    companion object {
        internal fun getStateDirectory(filesDir: File): File {
            return File(filesDir, "mozac.feature.recentlyclosed").apply {
                mkdirs()
            }
        }
    }
}

internal fun RecoverableTab.toRecentlyClosedTabEntity(): RecentlyClosedTabEntity {
    return RecentlyClosedTabEntity(
        uuid = id,
        title = title,
        url = url,
        createdAt = lastAccess
    )
}
