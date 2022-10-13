/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

package mozilla.components.service.pocket.spocs.api

import androidx.annotation.VisibleForTesting
import androidx.annotation.WorkerThread
import mozilla.components.concept.fetch.Client
import mozilla.components.service.pocket.spocs.api.SpocsEndpoint.Companion.newInstance
import mozilla.components.service.pocket.stories.api.PocketEndpoint.Companion.newInstance
import mozilla.components.service.pocket.stories.api.PocketResponse
import java.util.UUID

/**
 * Makes requests to the sponsored stories API and returns the requested data.
 *
 * @see [newInstance] to retrieve an instance.
 */
internal class SpocsEndpoint internal constructor(
    @get:VisibleForTesting internal val rawEndpoint: SpocsEndpointRaw,
    private val jsonParser: SpocsJSONParser,
) : SpocsProvider {

    /**
     * Download a new list of sponsored Pocket stories.
     *
     * If the API returns unexpectedly formatted results, these entries will be omitted and the rest of the items are
     * returned.
     *
     * @return a [PocketResponse.Success] with the sponsored Pocket stories (list may be empty)
     * or [PocketResponse.Failure] if the request didn't complete successfully.
     */
    @WorkerThread
    override suspend fun getSponsoredStories(): PocketResponse<List<ApiSpoc>> {
        val response = rawEndpoint.getSponsoredStories()
        val spocs = if (response.isNullOrBlank()) null else jsonParser.jsonToSpocs(response)
        return PocketResponse.wrap(spocs)
    }

    @WorkerThread
    override suspend fun deleteProfile(): PocketResponse<Boolean> {
        val response = rawEndpoint.deleteProfile()
        return PocketResponse.wrap(response)
    }

    companion object {
        /**
         * Returns a new instance of [SpocsEndpoint].
         *
         * @param client the HTTP client to use for network requests.
         * @param profileId Unique profile identifier which will be presented with sponsored stories.
         * @param appId Unique identifier of the application using this feature.
         */
        fun newInstance(client: Client, profileId: UUID, appId: String): SpocsEndpoint {
            val rawEndpoint = SpocsEndpointRaw.newInstance(client, profileId, appId)
            return SpocsEndpoint(rawEndpoint, SpocsJSONParser)
        }
    }
}
