/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

package mozilla.components.service.pocket.stories

import android.content.Context
import androidx.annotation.VisibleForTesting
import mozilla.components.concept.fetch.Client
import mozilla.components.service.pocket.PocketRecommendedStory
import mozilla.components.service.pocket.PocketSponsoredStory
import mozilla.components.service.pocket.PocketSponsoredStoryShim
import mozilla.components.service.pocket.logger
import mozilla.components.service.pocket.spocs.SpocsEndpoint
import mozilla.components.service.pocket.stories.api.PocketEndpoint
import mozilla.components.service.pocket.stories.api.PocketResponse
import mozilla.components.service.pocket.stories.api.PocketResponse.Failure
import mozilla.components.service.pocket.stories.api.PocketResponse.Success
import java.util.UUID

/**
 * Possible actions regarding the list of recommended stories.
 */
class PocketStoriesUseCases {

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

    /**
     * Allows for querying the list of available Pocket sponsored stories.
     */
    internal inner class GetSponsoredStories(
        private val profileId: UUID,
        private val appId: String,
    ) {
        /**
         * Do an internet query for a list of Pocket sponsored stories.
         */
        suspend operator fun invoke(): List<PocketSponsoredStory> {
            val client = fetchClient
            if (client == null) {
                logger.error("Cannot download new sponsored stories. Service has incomplete setup")
                return emptyList()
            }
            val provider = getSpocsProvider(client, profileId, appId)
            val stories = provider.getSponsoredStories()
            return when (stories) {
                is Success -> stories.data.map {
                    PocketSponsoredStory(
                        title = it.title,
                        url = it.url,
                        imageUrl = it.imageSrc,
                        sponsor = it.sponsor,
                        shim = PocketSponsoredStoryShim(
                            click = it.shim.click,
                            impression = it.shim.impression
                        )
                    )
                }
                is Failure -> {
                    logger.error("Could not download new sponsored stories.")
                    emptyList()
                }
            }
        }
    }

    /**
     * Allows deleting all stored user data used for downloading sponsored stories.
     */
    internal inner class DeleteUserProfile(
        private val profileId: UUID,
        private val appId: String,
    ) {
        /**
         * Delete all stored user data used for downloading personalized sponsored stories.
         */
        suspend operator fun invoke(): Boolean {
            val client = fetchClient
            if (client == null) {
                logger.error("Cannot delete user data. Service has incomplete setup")
                return false
            }

            val provider = getSpocsProvider(client, profileId, appId)
            return when (provider.deleteProfile()) {
                is Success -> true
                is Failure -> false
            }
        }
    }

    @VisibleForTesting
    internal fun getPocketRepository(context: Context) = PocketRecommendationsRepository(context)

    @VisibleForTesting
    internal fun getPocketEndpoint(client: Client) = PocketEndpoint.newInstance(client)

    @VisibleForTesting
    internal fun getSpocsProvider(client: Client, profileId: UUID, appId: String) =
        SpocsEndpoint.newInstance(client, profileId, appId)

    internal companion object {
        @VisibleForTesting internal var fetchClient: Client? = null

        /**
         * Convenience method for setting the the HTTP Client which will be used
         * for all REST communications with the Pocket server.
         *
         * Already downloaded data can still be queried but no new data can be downloaded until
         * this parameter is set.
         */
        internal fun initialize(client: Client) {
            this.fetchClient = client
        }

        /**
         * Convenience method for cleaning up any resources held for communicating with the Pocket server.
         *
         * Already downloaded data can still be queried but no new data can be downloaded until
         * [initialize] is used again.
         */
        internal fun reset() {
            fetchClient = null
        }
    }
}
