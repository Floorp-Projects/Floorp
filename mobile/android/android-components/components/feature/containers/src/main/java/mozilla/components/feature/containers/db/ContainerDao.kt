/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

package mozilla.components.feature.containers.db

import androidx.paging.DataSource
import androidx.room.Dao
import androidx.room.Delete
import androidx.room.Insert
import androidx.room.Query
import androidx.room.Transaction
import kotlinx.coroutines.flow.Flow

/**
 * Internal DAO for accessing [ContainerEntity] instances.
 */
@Dao
internal interface ContainerDao {
    @Insert
    suspend fun insertContainer(container: ContainerEntity): Long

    @Delete
    suspend fun deleteContainer(identity: ContainerEntity)

    @Transaction
    @Query("SELECT * FROM containers")
    fun getContainers(): Flow<List<ContainerEntity>>

    @Transaction
    @Query("SELECT * FROM containers")
    fun getContainersPaged(): DataSource.Factory<Int, ContainerEntity>
}
