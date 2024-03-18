/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

package mozilla.components.service.pocket.stories.api

import androidx.annotation.VisibleForTesting
import androidx.annotation.WorkerThread
import mozilla.components.concept.fetch.Client
import mozilla.components.service.pocket.stories.api.PocketEndpoint.Companion.newInstance

/**
 * Makes requests to the Pocket API and returns the requested data.
 *
 * @see [newInstance] to retrieve an instance.
 */
internal class PocketEndpoint internal constructor(
    @get:VisibleForTesting internal val rawEndpoint: PocketEndpointRaw,
    private val jsonParser: PocketJSONParser,
) {

    /**
     * Gets a response, filled with the Pocket stories recommendations from the Pocket API server on success.
     *
     * If the API returns unexpectedly formatted results, these entries will be omitted and the rest of the items are
     * returned.
     *
     * @return a [PocketResponse.Success] with the Pocket stories recommendations (the list will never be empty)
     * or, on error, a [PocketResponse.Failure].
     */
    @WorkerThread
    fun getRecommendedStories(): PocketResponse<List<PocketApiStory>> {
        val response = rawEndpoint.getRecommendedStories()
        val stories = response?.let { jsonParser.jsonToPocketApiStories(it) }
        return PocketResponse.wrap(stories)
    }

    companion object {
        /**
         * Returns a new instance of [PocketEndpoint].
         *
         * @param client the HTTP client to use for network requests.
         */
        fun newInstance(client: Client): PocketEndpoint {
            val rawEndpoint = PocketEndpointRaw.newInstance(client)
            return PocketEndpoint(rawEndpoint, PocketJSONParser())
        }
    }
}
