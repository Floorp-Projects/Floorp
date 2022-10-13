/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

package mozilla.components.feature.pwa.db

import androidx.annotation.WorkerThread
import androidx.room.Dao
import androidx.room.Insert
import androidx.room.OnConflictStrategy
import androidx.room.Query
import androidx.room.Update

/**
 * Internal DAO for accessing [ManifestEntity] instances.
 */
@Dao
internal interface ManifestDao {
    @WorkerThread
    @Query("SELECT * from manifests WHERE start_url = :startUrl")
    fun getManifest(startUrl: String): ManifestEntity?

    @WorkerThread
    @Query("SELECT * from manifests WHERE :url LIKE scope||'%' ORDER BY LENGTH(scope) DESC")
    fun getManifestsByScope(url: String): List<ManifestEntity>

    @WorkerThread
    @Query("SELECT count(start_url) from manifests WHERE :url LIKE scope||'%' AND used_at > :thresholdMs")
    fun hasRecentManifest(url: String, thresholdMs: Long): Int

    @WorkerThread
    @Query("SELECT count(start_url) from manifests WHERE used_at > :thresholdMs")
    fun recentManifestsCount(thresholdMs: Long): Int

    @WorkerThread
    @Query(
        """
        SELECT * from manifests 
        WHERE has_share_targets == 1 
        AND used_at > :deadline 
        ORDER BY used_at DESC
    """,
    )
    fun getRecentShareableManifests(deadline: Long): List<ManifestEntity>

    @WorkerThread
    @Insert(onConflict = OnConflictStrategy.REPLACE)
    fun insertManifest(manifest: ManifestEntity): Long

    @WorkerThread
    @Update
    fun updateManifest(manifest: ManifestEntity)

    @WorkerThread
    @Query("DELETE FROM manifests WHERE start_url IN (:startUrls)")
    fun deleteManifests(startUrls: List<String>)

    @WorkerThread
    @Query("SELECT * from manifests WHERE used_at > :expiresAt ORDER BY LENGTH(scope)")
    fun getInstalledScopes(expiresAt: Long): List<ManifestEntity>
}
