/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

package mozilla.components.feature.tab.collections.db

import androidx.paging.DataSource
import androidx.room.Dao
import androidx.room.Delete
import androidx.room.Insert
import androidx.room.Query
import androidx.room.Transaction
import androidx.room.Update
import kotlinx.coroutines.flow.Flow

/**
 * Internal DAO for accessing [TabCollectionEntity] instances.
 */
@Dao
internal interface TabCollectionDao {
    @Insert
    fun insertTabCollection(collection: TabCollectionEntity): Long

    @Delete
    fun deleteTabCollection(collection: TabCollectionEntity)

    @Update
    fun updateTabCollection(collection: TabCollectionEntity)

    @Transaction
    @Query(
        """
        SELECT tab_collections.id, tab_collections.title, tab_collections.created_at, tab_collections.updated_at
        FROM tab_collections LEFT JOIN tabs ON tab_collections.id = tab_collection_id
        GROUP BY tab_collections.id
        ORDER BY tab_collections.created_at DESC
    """,
    )
    fun getTabCollectionsPaged(): DataSource.Factory<Int, TabCollectionWithTabs>

    @Transaction
    @Query(
        """
        SELECT *
        FROM tab_collections
        ORDER BY created_at DESC
    """,
    )
    fun getTabCollections(): Flow<List<TabCollectionWithTabs>>

    @Transaction
    @Query(
        """
        SELECT *
        FROM tab_collections
        ORDER BY created_at DESC
    """,
    )
    suspend fun getTabCollectionsList(): List<TabCollectionWithTabs>

    @Query("SELECT COUNT(*) FROM tab_collections")
    fun countTabCollections(): Int
}
