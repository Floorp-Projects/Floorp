/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

package mozilla.components.service.pocket.stories

import android.content.Context
import androidx.annotation.VisibleForTesting
import mozilla.components.concept.fetch.Client
import mozilla.components.service.pocket.PocketStory.PocketRecommendedStory
import mozilla.components.service.pocket.stories.api.PocketEndpoint
import mozilla.components.service.pocket.stories.api.PocketResponse

/**
 * Possible actions regarding the list of recommended stories.
 *
 * @param appContext Android Context. Prefer sending application context to limit the possibility of even small leaks.
 * @param fetchClient the HTTP client to use for network requests.
 */
internal class PocketStoriesUseCases(
    private val appContext: Context,
    private val fetchClient: Client,
) {
    /**
     * Download and persist an updated list of recommended stories.
     */
    internal val refreshStories by lazy { RefreshPocketStories(appContext, fetchClient) }

    /**
     * Get the list of available Pocket sponsored stories.
     */
    internal val getStories by lazy { GetPocketStories(appContext) }

    /**
     * Atomically update the number of impressions for a list of Pocket recommended stories.
     */
    internal val updateTimesShown by lazy { UpdateStoriesTimesShown(appContext) }

    /**
     * Allows for refreshing the list of pocket stories we have cached.
     *
     * @param appContext Android Context. Prefer sending application context to limit the possibility
     * of even small leaks.
     * @param fetchClient the HTTP client to use for network requests.
     */
    internal inner class RefreshPocketStories(
        @get:VisibleForTesting
        internal val appContext: Context = this@PocketStoriesUseCases.appContext,
        @get:VisibleForTesting
        internal val fetchClient: Client = this@PocketStoriesUseCases.fetchClient,
    ) {
        /**
         * Do a full download from Pocket -> persist locally cycle for recommended stories.
         */
        suspend operator fun invoke(): Boolean {
            val pocket = getPocketEndpoint(fetchClient)
            val response = pocket.getRecommendedStories()

            if (response is PocketResponse.Success) {
                getPocketRepository(appContext)
                    .addAllPocketApiStories(response.data)
                return true
            }

            return false
        }
    }

    /**
     * Allows for querying the list of locally available Pocket recommended stories.
     *
     * @param context [Context] used for various system interactions and libraries initializations.
     */
    internal inner class GetPocketStories(
        @get:VisibleForTesting
        internal val context: Context = this@PocketStoriesUseCases.appContext,
    ) {
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
     *
     * @param context [Context] used for various system interactions and libraries initializations.
     */
    internal inner class UpdateStoriesTimesShown(
        @get:VisibleForTesting
        internal val context: Context = this@PocketStoriesUseCases.appContext,
    ) {
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
}
