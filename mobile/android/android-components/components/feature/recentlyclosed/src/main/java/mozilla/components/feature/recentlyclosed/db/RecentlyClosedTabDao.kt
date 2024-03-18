/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

package mozilla.components.feature.recentlyclosed.db

import androidx.room.Dao
import androidx.room.Delete
import androidx.room.Insert
import androidx.room.OnConflictStrategy
import androidx.room.Query
import androidx.room.Transaction
import kotlinx.coroutines.flow.Flow

/**
 * Internal DAO for accessing [RecentlyClosedTabEntity] instances.
 */
@Dao
internal interface RecentlyClosedTabDao {
    @Insert(onConflict = OnConflictStrategy.REPLACE)
    fun insertTab(tab: RecentlyClosedTabEntity): Long

    @Delete
    fun deleteTab(tab: RecentlyClosedTabEntity)

    @Transaction
    @Query(
        """
        SELECT *
        FROM recently_closed_tabs
        ORDER BY created_at DESC
    """,
    )
    fun getTabs(): Flow<List<RecentlyClosedTabEntity>>

    @Transaction
    @Query(
        """
        DELETE FROM recently_closed_tabs
    """,
    )
    fun removeAllTabs()
}
