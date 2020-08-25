/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

package mozilla.components.feature.top.sites.db

import androidx.annotation.WorkerThread
import androidx.room.Dao
import androidx.room.Delete
import androidx.room.Insert
import androidx.room.Query

/**
 * Internal DAO for accessing [PinnedSiteEntity] instances.
 */
@Dao
internal interface PinnedSiteDao {
    @WorkerThread
    @Insert
    fun insertPinnedSite(site: PinnedSiteEntity): Long

    @WorkerThread
    @Delete
    fun deletePinnedSite(site: PinnedSiteEntity)

    @WorkerThread
    @Query("SELECT * FROM top_sites")
    fun getPinnedSites(): List<PinnedSiteEntity>
}
