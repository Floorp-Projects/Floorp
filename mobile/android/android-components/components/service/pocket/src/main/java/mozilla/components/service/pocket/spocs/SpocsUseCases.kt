/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

package mozilla.components.service.pocket.spocs

import android.content.Context
import androidx.annotation.VisibleForTesting
import mozilla.components.concept.fetch.Client
import mozilla.components.service.pocket.PocketStory.PocketRecommendedStory
import mozilla.components.service.pocket.PocketStory.PocketSponsoredStory
import mozilla.components.service.pocket.spocs.api.SpocsEndpoint
import mozilla.components.service.pocket.stories.api.PocketResponse.Failure
import mozilla.components.service.pocket.stories.api.PocketResponse.Success
import java.util.UUID

/**
 * Possible actions regarding the list of sponsored stories.
 *
 * @param appContext Android Context. Prefer sending application context to limit the possibility of even small leaks.
 * @param fetchClient the HTTP client to use for network requests.
 * @param profileId Unique profile identifier used for downloading sponsored Pocket stories.
 * @param appId Unique app identifier used for downloading sponsored Pocket stories.
 */
internal class SpocsUseCases(
    private val appContext: Context,
    private val fetchClient: Client,
    private val profileId: UUID,
    private val appId: String,
) {
    /**
     * Download and persist an updated list of sponsored stories.
     */
    internal val refreshStories by lazy {
        RefreshSponsoredStories(appContext, fetchClient, profileId, appId)
    }

    /**
     * Get the list of available Pocket sponsored stories.
     */
    internal val getStories by lazy {
        GetSponsoredStories(appContext)
    }

    internal val recordImpression by lazy {
        RecordImpression(appContext)
    }

    /**
     * Delete all stored user data used for downloading sponsored stories.
     */
    internal val deleteProfile by lazy {
        DeleteProfile(appContext, fetchClient, profileId, appId)
    }

    /**
     * Allows for refreshing the list of Pocket sponsored stories we have cached.
     *
     * @param appContext Android Context. Prefer sending application context to limit the possibility
     * of even small leaks.
     * @param fetchClient the HTTP client to use for network requests.
     * @param profileId Unique profile identifier when using this feature.
     * @param appId Unique identifier of the application using this feature.
     */
    internal inner class RefreshSponsoredStories(
        @get:VisibleForTesting
        internal val appContext: Context = this@SpocsUseCases.appContext,
        @get:VisibleForTesting
        internal val fetchClient: Client = this@SpocsUseCases.fetchClient,
        @get:VisibleForTesting
        internal val profileId: UUID = this@SpocsUseCases.profileId,
        @get:VisibleForTesting
        internal val appId: String = this@SpocsUseCases.appId,
    ) {
        /**
         * Do a full download from Pocket -> persist locally cycle for sponsored stories.
         */
        suspend operator fun invoke(): Boolean {
            val provider = getSpocsProvider(fetchClient, profileId, appId)
            val response = provider.getSponsoredStories()

            if (response is Success) {
                getSpocsRepository(appContext).addSpocs(response.data)
                return true
            }

            return false
        }
    }

    /**
     * Allows for querying the list of available Pocket sponsored stories.
     *
     * @param context [Context] used for various system interactions and libraries initializations.

     */
    internal inner class GetSponsoredStories(
        @get:VisibleForTesting
        internal val context: Context = this@SpocsUseCases.appContext,
    ) {
        /**
         * Do an internet query for a list of Pocket sponsored stories.
         */
        suspend operator fun invoke(): List<PocketSponsoredStory> {
            return getSpocsRepository(context).getAllSpocs()
        }
    }

    /**
     * Allows for atomically updating the [PocketRecommendedStory.timesShown] property of some recommended stories.
     *
     * @param context [Context] used for various system interactions and libraries initializations.
     */
    internal inner class RecordImpression(
        @get:VisibleForTesting
        internal val context: Context = this@SpocsUseCases.appContext,
    ) {
        /**
         * Update how many times certain stories were shown to the user.
         */
        suspend operator fun invoke(storiesShown: List<Int>) {
            if (storiesShown.isNotEmpty()) {
                getSpocsRepository(context).recordImpressions(storiesShown)
            }
        }
    }

    /**
     * Allows deleting all stored user data used for downloading sponsored stories.
     *
     * @param context [Context] used for various system interactions and libraries initializations.
     * @param fetchClient the HTTP client to use for network requests.
     * @param profileId Unique profile identifier previously used for downloading sponsored Pocket stories.
     * @param appId Unique app identifier previously used for downloading sponsored Pocket stories.
     */
    internal inner class DeleteProfile(
        @get:VisibleForTesting
        internal val context: Context = this@SpocsUseCases.appContext,
        @get:VisibleForTesting
        internal val fetchClient: Client = this@SpocsUseCases.fetchClient,
        @get:VisibleForTesting
        internal val profileId: UUID = this@SpocsUseCases.profileId,
        @get:VisibleForTesting
        internal val appId: String = this@SpocsUseCases.appId,
    ) {
        /**
         * Delete all stored user data used for downloading personalized sponsored stories.
         */
        suspend operator fun invoke(): Boolean {
            val provider = getSpocsProvider(fetchClient, profileId, appId)
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
}
