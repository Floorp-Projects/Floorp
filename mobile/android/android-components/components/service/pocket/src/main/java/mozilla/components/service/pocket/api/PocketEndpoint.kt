/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

package mozilla.components.service.pocket.api

import androidx.annotation.VisibleForTesting
import androidx.annotation.WorkerThread
import mozilla.components.concept.fetch.Client
import mozilla.components.service.pocket.api.PocketEndpoint.Companion.newInstance

/**
 * Makes requests to the Pocket API and returns the requested data.
 *
 * @see [newInstance] to retrieve an instance.
 */
internal class PocketEndpoint internal constructor(
    @VisibleForTesting internal val rawEndpoint: PocketEndpointRaw
) {

    /**
     * Gets a response, filled with the Pocket global stories recommendations from the Pocket API server on success.
     *
     * If the API returns unexpectedly formatted results, these entries will be omitted and the rest of the items are
     * returned.
     *
     * @return a [PocketResponse.Success] with the Pocket global stories recommendations (the list will never be empty)
     * or, on error, a [PocketResponse.Failure].
     */
    @WorkerThread
    fun getTopStories(
        count: Int,
        locale: String
    ): PocketResponse<String> {
        val response = rawEndpoint.getGlobalStoriesRecommendations(
            count,
            locale
        )
        return PocketResponse.wrap(response)
    }

    companion object {
        /**
         * Returns a new instance of [PocketEndpoint].
         *
         * @param pocketApiKey the API key for Pocket network requests.
         * @param client the HTTP client to use for network requests.
         *
         * @throws IllegalArgumentException if the provided API key is deemed invalid.
         */
        fun newInstance(
            pocketApiKey: String,
            client: Client
        ): PocketEndpoint {
            val rawEndpoint = PocketEndpointRaw.newInstance(
                pocketApiKey,
                client
            )
            return PocketEndpoint(rawEndpoint)
        }
    }
}
