/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

package mozilla.components.feature.pwa.db

import androidx.annotation.WorkerThread
import androidx.room.Dao
import androidx.room.Insert
import androidx.room.OnConflictStrategy
import androidx.room.Query

/**
 * Internal DAO for accessing [ManifestEntity] instances.
 */
@Dao
internal interface ManifestDao {
    @WorkerThread
    @Query("SELECT * from manifests WHERE start_url = :startUrl")
    fun getManifest(startUrl: String): ManifestEntity?

    @WorkerThread
    @Insert(onConflict = OnConflictStrategy.REPLACE)
    fun insertManifest(manifest: ManifestEntity): Long

    @WorkerThread
    @Query("DELETE FROM manifests WHERE start_url IN (:startUrls)")
    fun deleteManifests(startUrls: List<String>)
}
