/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

package mozilla.components.service.pocket.stories

import android.content.Context
import androidx.annotation.VisibleForTesting
import mozilla.components.service.pocket.PocketStory.PocketRecommendedStory
import mozilla.components.service.pocket.ext.toPartialTimeShownUpdate
import mozilla.components.service.pocket.ext.toPocketLocalStory
import mozilla.components.service.pocket.ext.toPocketRecommendedStory
import mozilla.components.service.pocket.stories.api.PocketApiStory
import mozilla.components.service.pocket.stories.db.PocketRecommendationsDatabase

/**
 * Wrapper over our local database.
 * Allows for easy CRUD operations.
 */
internal class PocketRecommendationsRepository(context: Context) {
    private val database: Lazy<PocketRecommendationsDatabase> = lazy { PocketRecommendationsDatabase.get(context) }

    @VisibleForTesting
    internal val pocketRecommendationsDao by lazy { database.value.pocketRecommendationsDao() }

    /**
     * Get the current locally persisted list of Pocket recommended articles.
     */
    suspend fun getPocketRecommendedStories(): List<PocketRecommendedStory> {
        return pocketRecommendationsDao.getPocketStories().map { it.toPocketRecommendedStory() }
    }

    suspend fun updateShownPocketRecommendedStories(updatedStories: List<PocketRecommendedStory>) {
        return pocketRecommendationsDao.updateTimesShown(
            updatedStories.map { it.toPartialTimeShownUpdate() },
        )
    }

    /**
     * Replace the current list of locally persisted Pocket recommended articles.
     *
     * @param stories The list of Pocket recommended articles to persist locally.
     */
    suspend fun addAllPocketApiStories(stories: List<PocketApiStory>) {
        pocketRecommendationsDao.cleanOldAndInsertNewPocketStories(stories.map { it.toPocketLocalStory() })
    }
}
