/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

package mozilla.components.service.pocket.spocs

import android.content.Context
import androidx.annotation.VisibleForTesting
import mozilla.components.concept.fetch.Client
import mozilla.components.service.pocket.PocketNetworkUseCase
import mozilla.components.service.pocket.PocketNetworkUseCases
import mozilla.components.service.pocket.PocketSponsoredStory
import mozilla.components.service.pocket.logger
import mozilla.components.service.pocket.spocs.api.SpocsEndpoint
import mozilla.components.service.pocket.stories.api.PocketResponse.Failure
import mozilla.components.service.pocket.stories.api.PocketResponse.Success
import java.util.UUID

/**
 * Possible actions regarding the list of sponsored stories.
 */
internal class SpocsUseCases : PocketNetworkUseCase() {
    /**
     * Allows for querying the list of available Pocket sponsored stories.
     *
     * @param context [Context] used for various system interactions and libraries initializations.
     * @param profileId Unique profile identifier which will be presented with sponsored stories.
     * @param appId Unique identifier of the application using this feature.
     */
    internal inner class GetSponsoredStories(
        private val context: Context,
        private val profileId: UUID,
        private val appId: String,
    ) {
        /**
         * Do an internet query for a list of Pocket sponsored stories.
         */
        suspend operator fun invoke(): List<PocketSponsoredStory> {
            return getSpocsRepository(context).getAllSpocs()
        }
    }

    /**
     * Allows deleting all stored user data used for downloading sponsored stories.
     *
     * @param context [Context] used for various system interactions and libraries initializations.
     * @param profileId Unique profile identifier previously used for downloading sponsored Pocket stories.
     * @param appId Unique app identifier previously used for downloading sponsored Pocket stories.
     */
    internal inner class DeleteProfile(
        private val context: Context,
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
                is Success -> {
                    getSpocsRepository(context).deleteAllSpocs()
                    true
                }
                is Failure -> {
                    // Don't attempt to delete locally persisted stories to prevent mismatching issues
                    // with profile deletion failing - applications still "showing it" but
                    // with no sponsored articles to show.
                    false
                }
            }
        }
    }

    @VisibleForTesting
    internal fun getSpocsRepository(context: Context) = SpocsRepository(context)

    @VisibleForTesting
    internal fun getSpocsProvider(client: Client, profileId: UUID, appId: String) =
        SpocsEndpoint.newInstance(client, profileId, appId)

    internal companion object : PocketNetworkUseCases by PocketNetworkUseCase.Companion
}
