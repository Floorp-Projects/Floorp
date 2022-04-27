/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

package mozilla.components.service.pocket.spocs

import androidx.annotation.VisibleForTesting
import mozilla.components.concept.fetch.Client
import mozilla.components.service.pocket.PocketNetworkUseCase
import mozilla.components.service.pocket.PocketNetworkUseCases
import mozilla.components.service.pocket.PocketSponsoredStory
import mozilla.components.service.pocket.PocketSponsoredStoryShim
import mozilla.components.service.pocket.logger
import mozilla.components.service.pocket.stories.api.PocketResponse.Failure
import mozilla.components.service.pocket.stories.api.PocketResponse.Success
import java.util.UUID

/**
 * Possible actions regarding the list of sponsored stories.
 */
internal class SpocsUseCases : PocketNetworkUseCase() {
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
    internal inner class DeleteProfile(
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
            return when(provider.deleteProfile()) {
                is Success -> true
                is Failure -> false
            }
        }
    }

    @VisibleForTesting
    internal fun getSpocsProvider(client: Client, profileId: UUID, appId: String) =
        SpocsEndpoint.newInstance(client, profileId, appId)

    internal companion object : PocketNetworkUseCases by PocketNetworkUseCase.Companion
}
