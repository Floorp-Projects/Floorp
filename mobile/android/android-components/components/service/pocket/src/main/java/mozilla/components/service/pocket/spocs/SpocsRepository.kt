/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

package mozilla.components.service.pocket.spocs

import android.content.Context
import androidx.annotation.VisibleForTesting
import mozilla.components.service.pocket.PocketStory.PocketSponsoredStory
import mozilla.components.service.pocket.ext.toLocalSpoc
import mozilla.components.service.pocket.ext.toPocketSponsoredStory
import mozilla.components.service.pocket.spocs.api.ApiSpoc
import mozilla.components.service.pocket.spocs.db.SpocImpressionEntity
import mozilla.components.service.pocket.stories.db.PocketRecommendationsDatabase

/**
 * Wrapper over our local database containing Spocs.
 * Allows for easy CRUD operations.
 */
internal class SpocsRepository(context: Context) {
    private val database: Lazy<PocketRecommendationsDatabase> = lazy { PocketRecommendationsDatabase.get(context) }

    @VisibleForTesting
    internal val spocsDao by lazy { database.value.spocsDao() }

    /**
     * Get the current locally persisted list of sponsored Pocket stories
     * complete with the list of all locally persisted impressions data.
     */
    suspend fun getAllSpocs(): List<PocketSponsoredStory> {
        val spocs = spocsDao.getAllSpocs()
        val impressions = spocsDao.getSpocsImpressions().groupBy { it.spocId }

        return spocs.map { spoc ->
            spoc.toPocketSponsoredStory(
                impressions[spoc.id]
                    ?.map { impression -> impression.impressionDateInSeconds }
                    ?: emptyList(),
            )
        }
    }

    /**
     * Delete all currently persisted sponsored Pocket stories.
     */
    suspend fun deleteAllSpocs() {
        spocsDao.deleteAllSpocs()
    }

    /**
     * Replace the current list of locally persisted sponsored Pocket stories.
     *
     * @param spocs The list of sponsored Pocket stories to persist locally.
     */
    suspend fun addSpocs(spocs: List<ApiSpoc>) {
        spocsDao.cleanOldAndInsertNewSpocs(spocs.map { it.toLocalSpoc() })
    }

    /**
     * Add a new impression record for each of the spocs identified by the ids from [spocsShown].
     * Will ignore adding new entries if the intended spocs are not persisted locally anymore.
     * Recorded entries will automatically be cleaned when the spoc they target is deleted.
     *
     * @param spocsShown List of [PocketSponsoredStory.id] for which to record new impressions.
     */
    suspend fun recordImpressions(spocsShown: List<Int>) {
        spocsDao.recordImpressions(spocsShown.map { SpocImpressionEntity(it) })
    }
}
