/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

package mozilla.components.service.pocket.stories

import android.content.Context
import androidx.annotation.VisibleForTesting
import mozilla.components.concept.fetch.Client
import mozilla.components.service.pocket.PocketNetworkUseCase
import mozilla.components.service.pocket.PocketNetworkUseCases
import mozilla.components.service.pocket.PocketRecommendedStory
import mozilla.components.service.pocket.logger
import mozilla.components.service.pocket.stories.api.PocketEndpoint
import mozilla.components.service.pocket.stories.api.PocketResponse

/**
 * Possible actions regarding the list of recommended stories.
 */
internal class PocketStoriesUseCases : PocketNetworkUseCase() {

    /**
     * Allows for refreshing the list of pocket stories we have cached.
     *
     * @param context Android Context. Prefer sending application context to limit the possibility of even small leaks.
     */
    internal inner class RefreshPocketStories(
        @VisibleForTesting
        internal val context: Context
    ) {
        /**
         * Do a full download from Pocket -> persist locally cycle for recommended stories.
         */
        suspend operator fun invoke(): Boolean {
            val client = fetchClient
            if (client == null) {
                logger.error("Cannot download new stories. Service has incomplete setup")
                return false
            }

            val pocket = getPocketEndpoint(client)
            val response = pocket.getRecommendedStories()

            if (response is PocketResponse.Success) {
                getPocketRepository(context)
                    .addAllPocketApiStories(response.data)
                return true
            }

            return false
        }
    }

    /**
     * Allows for querying the list of locally available Pocket recommended stories.
     */
    internal inner class GetPocketStories(private val context: Context) {
        /**
         * Returns the current locally persisted list of Pocket recommended stories.
         */
        suspend operator fun invoke(): List<PocketRecommendedStory> {
            return getPocketRepository(context)
                .getPocketRecommendedStories()
        }
    }

    /**
     * Allows for atomically updating the [PocketRecommendedStory.timesShown] property of some recommended stories.
     */
    internal inner class UpdateStoriesTimesShown(private val context: Context) {
        /**
         * Update how many times certain stories were shown to the user.
         */
        suspend operator fun invoke(storiesShown: List<PocketRecommendedStory>) {
            if (storiesShown.isNotEmpty()) {
                getPocketRepository(context)
                    .updateShownPocketRecommendedStories(storiesShown)
            }
        }
    }

    @VisibleForTesting
    internal fun getPocketRepository(context: Context) = PocketRecommendationsRepository(context)

    @VisibleForTesting
    internal fun getPocketEndpoint(client: Client) = PocketEndpoint.newInstance(client)

    internal companion object : PocketNetworkUseCases by PocketNetworkUseCase.Companion
}
