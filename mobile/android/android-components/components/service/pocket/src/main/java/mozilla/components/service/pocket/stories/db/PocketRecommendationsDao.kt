/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

package mozilla.components.service.pocket.stories.db

import androidx.room.Dao
import androidx.room.Delete
import androidx.room.Insert
import androidx.room.OnConflictStrategy
import androidx.room.Query
import androidx.room.Transaction
import androidx.room.Update

@Dao
internal interface PocketRecommendationsDao {
    /**
     * Add new stories to the database.
     * Stories already existing will not be updated in any way.
     * Already persisted stories but not present in [stories] will be removed from the database.
     *
     * @param stories new list of [PocketStoryEntity]s to replace the currently persisted ones.
     */
    @Transaction
    suspend fun cleanOldAndInsertNewPocketStories(stories: List<PocketStoryEntity>) {
        // If any url changed that story is obsolete and should be deleted.
        val newStoriesUrls = stories.map { it.url to it.imageUrl }
        val oldStoriesToDelete = getPocketStories()
            .filterNot { newStoriesUrls.contains(it.url to it.imageUrl) }
        delete(oldStoriesToDelete)

        insertPocketStories(stories)
    }

    @Update(entity = PocketStoryEntity::class)
    suspend fun updateTimesShown(updatedStories: List<PocketLocalStoryTimesShown>)

    @Query("SELECT * FROM ${PocketRecommendationsDatabase.TABLE_NAME_STORIES}")
    suspend fun getPocketStories(): List<PocketStoryEntity>

    @Insert(onConflict = OnConflictStrategy.IGNORE)
    suspend fun insertPocketStories(stories: List<PocketStoryEntity>)

    @Delete
    suspend fun delete(stories: List<PocketStoryEntity>)
}
