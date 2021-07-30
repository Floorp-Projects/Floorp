/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

package mozilla.components.service.pocket.stories

import android.content.Context
import androidx.annotation.VisibleForTesting
import mozilla.components.concept.fetch.Client
import mozilla.components.service.pocket.PocketRecommendedStory
import mozilla.components.service.pocket.api.PocketEndpoint
import mozilla.components.service.pocket.api.PocketResponse
import mozilla.components.support.base.log.Log

/**
 * Possible actions regarding the list of recommended stories.
 */
class PocketStoriesUseCases {

    /**
     * Allows for refreshing the list of pocket stories we have cached.
     *
     * @param context Android Context. Prefer sending application context to limit the possibility of even small leaks.
     * @param pocketToken Pocket OAuth request token used for downloading recommended stories.
     * @param pocketItemsCount How many recommended stories to download.
     *     Once downloaded these stories will replace any that we already had cached.
     * @param pocketItemsLocale The ISO-639 locale for Pocket recommended stories.
     */
    internal inner class RefreshPocketStories(
        @VisibleForTesting
        internal val context: Context,
        @VisibleForTesting
        internal val pocketItemsCount: Int,
        @VisibleForTesting
        internal val pocketItemsLocale: String
    ) {
        /**
         * Do a full download from Pocket -> persist locally cycle for recommended stories.
         */
        suspend operator fun invoke(): Boolean {
            val apiKey = pocketApiKey
            val client = fetchClient
            if (apiKey == null || client == null) {
                Log.log(
                    Log.Priority.ERROR,
                    this.javaClass.simpleName,
                    message = "Cannot download new stories. Service has incomplete setup"
                )
                return false
            }

            val pocket = getPocketEndpoint(apiKey, client)
            val response = pocket.getTopStories(pocketItemsCount, pocketItemsLocale)

            if (response is PocketResponse.Success) {
                getPocketRepository(context)
                    .addAllPocketPocketRecommendedStories(response.data)
                return true
            }

            return false
        }
    }

    /**
     * Allows for querying the list of locally available Pocket recommended stories.
     */
    inner class GetPocketStories(private val context: Context) {
        /**
         * Returns the current locally persisted list of Pocket recommended stories.
         */
        suspend operator fun invoke(): List<PocketRecommendedStory> {
            return getPocketRepository(context)
                .getPocketRecommendedStories()
        }
    }

    @VisibleForTesting
    internal fun getPocketRepository(context: Context) = PocketRecommendationsRepository(context)

    @VisibleForTesting
    internal fun getPocketEndpoint(apiKey: String, client: Client) = PocketEndpoint.newInstance(apiKey, client)

    internal companion object {
        @VisibleForTesting internal var pocketApiKey: String? = null
        @VisibleForTesting internal var fetchClient: Client? = null

        /**
         * Convenience method for setting the Pocket api key and the HTTP Client which will be used
         * for all REST communication with the Pocket server.
         *
         * Already downloaded data can still be queried but no new data can be downloaded until this data is set.
         */
        internal fun initialize(pocketApiKey: String, client: Client) {
            this.pocketApiKey = pocketApiKey
            this.fetchClient = client
        }

        /**
         * Convenience method for cleaning up any resources held for communicating with the Pocket server.
         *
         * Already downloaded data can still be queried but no new data can be downloaded until
         * [initialize] is used again.
         */
        internal fun reset() {
            pocketApiKey = null
            fetchClient = null
        }
    }
}
