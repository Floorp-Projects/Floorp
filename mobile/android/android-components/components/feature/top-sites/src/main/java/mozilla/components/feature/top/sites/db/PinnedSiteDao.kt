/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

package mozilla.components.feature.top.sites.db

import androidx.paging.DataSource
import androidx.room.Dao
import androidx.room.Delete
import androidx.room.Insert
import androidx.room.Query
import androidx.room.Transaction
import kotlinx.coroutines.flow.Flow

/**
 * Internal DAO for accessing [PinnedSiteEntity] instances.
 */
@Dao
internal interface PinnedSiteDao {
    @Insert
    fun insertPinnedSite(site: PinnedSiteEntity): Long

    @Delete
    fun deletePinnedSite(site: PinnedSiteEntity)

    @Transaction
    @Query("SELECT * FROM top_sites")
    fun getPinnedSites(): Flow<List<PinnedSiteEntity>>

    @Transaction
    @Query("SELECT * FROM top_sites")
    fun getPinnedSitesPaged(): DataSource.Factory<Int, PinnedSiteEntity>
}
