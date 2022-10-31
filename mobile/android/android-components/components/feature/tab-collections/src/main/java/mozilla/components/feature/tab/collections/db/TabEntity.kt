/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

package mozilla.components.feature.tab.collections.db

import android.util.AtomicFile
import androidx.room.ColumnInfo
import androidx.room.Entity
import androidx.room.ForeignKey
import androidx.room.Index
import androidx.room.PrimaryKey
import java.io.File

/**
 * Internal entity representing a tab that is part of a collection.
 */
@Entity(
    tableName = "tabs",
    foreignKeys = [
        ForeignKey(
            entity = TabCollectionEntity::class,
            parentColumns = ["id"],
            childColumns = ["tab_collection_id"],
            onDelete = ForeignKey.CASCADE,
        ),
    ],
    indices = [
        Index(value = ["tab_collection_id"]),
    ],
)
internal data class TabEntity(
    @PrimaryKey(autoGenerate = true)
    @ColumnInfo(name = "id")
    var id: Long? = null,

    @ColumnInfo(name = "title")
    var title: String,

    @ColumnInfo(name = "url")
    var url: String,

    @ColumnInfo(name = "stat_file")
    var stateFile: String,

    @ColumnInfo(name = "tab_collection_id")
    var tabCollectionId: Long,

    @ColumnInfo(name = "created_at")
    var createdAt: Long,
) {
    internal fun getStateFile(filesDir: File): AtomicFile {
        return AtomicFile(File(getStateDirectory(filesDir), stateFile))
    }

    companion object {
        internal fun getStateDirectory(filesDir: File): File {
            return File(filesDir, "mozac.feature.tab.collections").apply {
                mkdirs()
            }
        }
    }
}
