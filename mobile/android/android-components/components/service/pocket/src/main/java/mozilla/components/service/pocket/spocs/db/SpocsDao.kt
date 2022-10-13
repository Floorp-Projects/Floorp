/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

package mozilla.components.service.pocket.spocs.db

import androidx.room.Dao
import androidx.room.Delete
import androidx.room.Insert
import androidx.room.OnConflictStrategy
import androidx.room.Query
import androidx.room.Transaction
import mozilla.components.service.pocket.stories.db.PocketRecommendationsDatabase
import java.util.concurrent.TimeUnit

@Dao
internal interface SpocsDao {
    @Transaction
    suspend fun cleanOldAndInsertNewSpocs(spocs: List<SpocEntity>) {
        val newSpocs = spocs.map { it.id }
        val oldStoriesToDelete = getAllSpocs()
            .filterNot { newSpocs.contains(it.id) }

        deleteSpocs(oldStoriesToDelete)
        insertSpocs(spocs)
    }

    @Insert(onConflict = OnConflictStrategy.REPLACE) // Maybe some details changed
    suspend fun insertSpocs(stories: List<SpocEntity>)

    @Transaction
    suspend fun recordImpressions(stories: List<SpocImpressionEntity>) {
        stories.forEach {
            recordImpression(it.spocId, it.impressionDateInSeconds)
        }
    }

    /**
     * INSERT OR IGNORE method needed to prevent against "FOREIGN KEY constraint failed" exceptions
     * if clients try to insert new impressions spocs not existing anymore in the database in cases where
     * a different list of spocs were downloaded but the client operates with stale in-memory data.
     *
     * @param targetSpocId The `id` of the [SpocEntity] to add a new impression for.
     * A new impression will be persisted only if a story with the indicated [targetSpocId] currently exists.
     * @param targetImpressionDateInSeconds The timestamp expressed in seconds from Epoch for this impression.
     * Defaults to the current time expressed in seconds as get from `System.currentTimeMillis / 1000`.
     */
    @Query(
        "WITH newImpression(spocId, impressionDateInSeconds) AS (VALUES" +
            "(:targetSpocId, :targetImpressionDateInSeconds)" +
            ")" +
            "INSERT INTO spocs_impressions(spocId, impressionDateInSeconds) " +
            "SELECT impression.spocId, impression.impressionDateInSeconds " +
            "FROM newImpression impression " +
            "WHERE EXISTS (SELECT 1 FROM spocs spoc WHERE spoc.id = impression.spocId)",
    )
    suspend fun recordImpression(
        targetSpocId: Int,
        targetImpressionDateInSeconds: Long = TimeUnit.MILLISECONDS.toSeconds(System.currentTimeMillis()),
    )

    @Query("DELETE FROM ${PocketRecommendationsDatabase.TABLE_NAME_SPOCS}")
    suspend fun deleteAllSpocs()

    @Delete
    suspend fun deleteSpocs(stories: List<SpocEntity>)

    @Query("SELECT * FROM ${PocketRecommendationsDatabase.TABLE_NAME_SPOCS}")
    suspend fun getAllSpocs(): List<SpocEntity>

    @Query("SELECT * FROM ${PocketRecommendationsDatabase.TABLE_NAME_SPOCS_IMPRESSIONS}")
    suspend fun getSpocsImpressions(): List<SpocImpressionEntity>
}
