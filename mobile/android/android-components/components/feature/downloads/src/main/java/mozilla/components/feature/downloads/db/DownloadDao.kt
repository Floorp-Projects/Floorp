/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

package mozilla.components.feature.downloads.db

import androidx.paging.DataSource
import androidx.room.Dao
import androidx.room.Delete
import androidx.room.Insert
import androidx.room.Query
import androidx.room.Update
import kotlinx.coroutines.flow.Flow

/**
 * Internal dao for accessing and modifying Downloads in the database.
 */
@Dao
internal interface DownloadDao {

    @Insert
    suspend fun insert(entity: DownloadEntity): Long

    @Update
    suspend fun update(entity: DownloadEntity)

    @Query("SELECT * FROM downloads ORDER BY created_at DESC")
    fun getDownloads(): Flow<List<DownloadEntity>>

    @Query("SELECT * FROM downloads ORDER BY created_at DESC")
    suspend fun getDownloadsList(): List<DownloadEntity>

    @Delete
    suspend fun delete(entity: DownloadEntity)

    @Query("DELETE FROM downloads")
    suspend fun deleteAllDownloads()

    @Query("SELECT * FROM downloads ORDER BY created_at DESC")
    fun getDownloadsPaged(): DataSource.Factory<Int, DownloadEntity>
}
