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
import mozilla.components.service.pocket.stories.api.PocketEndpoint
import mozilla.components.service.pocket.stories.api.PocketResponse.Failure
import mozilla.components.service.pocket.stories.api.PocketResponse.Success
import java.util.UUID

/**
 * Possible actions regarding the list of sponsored stories.
 */
internal class SpocsUseCases : PocketNetworkUseCase() {

    /**
     * Allows for refreshing the list of Pocket sponsored stories we have cached.
     *
     * @param context Android Context. Prefer sending application context to limit the possibility of even small leaks.
     */
    internal inner class RefreshSponsoredStories(
        @get:VisibleForTesting
        internal val context: Context
    ) {
        /**
         * Do a full download from Pocket -> persist locally cycle for sponsored stories.
         */
        @Suppress("ReturnCount")
        suspend operator fun invoke(): Boolean {
            val client = SpocsUseCases.fetchClient
            if (client == null) {
                logger.error("Cannot download new stories. Service has incomplete setup")
                return false
            }

            val profileId = SpocsUseCases.profileId
            val appId = SpocsUseCases.appId
            if (profileId == null || appId == null) {
                logger.info("Not refreshing sponsored stories. Service has incomplete setup")
                return false
            }

            val provider = getSpocsProvider(client, profileId, appId)
            val response = provider.getSponsoredStories()

            if (response is Success) {
                getSpocsRepository(context).addSpocs(response.data)
                return true
            }

            return false
        }
    }

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

    @VisibleForTesting
    internal fun getPocketEndpoint(client: Client) = PocketEndpoint.newInstance(client)

    internal companion object : PocketNetworkUseCases by PocketNetworkUseCase.Companion {
        /**
         * Unique user identifier previously used for downloading sponsored Pocket stories.
         * May be `null` if the Sponsored Pocket stories feature is disabled.
         */
        var profileId: UUID? = null

        /**
         * Unique app identifier previously used for downloading sponsored Pocket stories.
         * May be `null` if the Sponsored Pocket stories feature is disabled.
         */
        var appId: String? = null

        /**
         * Convenience method for setting all details used when communicating with the Pocket server.
         *
         * @param client the HTTP client to use for network requests.
         * @param profileId Unique user identifier previously used for downloading sponsored Pocket stories.
         * @param appId Unique app identifier previously used for downloading sponsored Pocket stories.
         */
        fun initialize(client: Client, profileId: UUID, appId: String) {
            initialize(client)
            this.profileId = profileId
            this.appId = appId
        }

        override fun reset() {
            this.fetchClient = null
            this.profileId = null
            this.appId = null
        }
    }
}
