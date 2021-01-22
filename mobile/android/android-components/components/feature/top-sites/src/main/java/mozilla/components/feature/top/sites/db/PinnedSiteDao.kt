/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

package mozilla.components.feature.top.sites.db

import androidx.annotation.WorkerThread
import androidx.room.Dao
import androidx.room.Delete
import androidx.room.Insert
import androidx.room.Query
import androidx.room.Transaction
import androidx.room.Update

/**
 * Internal DAO for accessing [PinnedSiteEntity] instances.
 */
@Dao
internal interface PinnedSiteDao {
    @WorkerThread
    @Insert
    fun insertPinnedSite(site: PinnedSiteEntity): Long

    @WorkerThread
    @Update
    fun updatePinnedSite(site: PinnedSiteEntity)

    @WorkerThread
    @Delete
    fun deletePinnedSite(site: PinnedSiteEntity)

    @WorkerThread
    @Transaction
    fun insertAllPinnedSites(sites: List<PinnedSiteEntity>): List<Long> {
        return sites.map { entity ->
            val id = insertPinnedSite(entity)
            entity.id = id
            id
        }
    }

    @WorkerThread
    @Query("SELECT * FROM top_sites")
    fun getPinnedSites(): List<PinnedSiteEntity>

    @Query("SELECT COUNT(*) FROM top_sites")
    fun getPinnedSitesCount(): Int
}
