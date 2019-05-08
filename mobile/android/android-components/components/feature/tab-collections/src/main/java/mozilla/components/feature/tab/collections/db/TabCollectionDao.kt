/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

package mozilla.components.feature.tab.collections.db

import androidx.paging.DataSource
import androidx.room.Dao
import androidx.room.Delete
import androidx.room.Insert
import androidx.room.Query
import androidx.room.Update

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

    @Query("""
        SELECT *
        FROM tab_collections LEFT JOIN tabs ON tab_collections.id = tab_collection_id
        GROUP BY tab_collections.id
        ORDER BY created_at DESC
    """)
    fun getTabCollectionsPaged(): DataSource.Factory<Int, TabCollectionWithTabs>
}
