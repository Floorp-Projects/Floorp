/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

package mozilla.components.service.pocket.api

import android.net.Uri
import androidx.annotation.VisibleForTesting
import androidx.annotation.WorkerThread
import mozilla.components.concept.fetch.Client
import mozilla.components.concept.fetch.Request
import mozilla.components.service.pocket.DEFAULT_STORIES_COUNT
import mozilla.components.service.pocket.DEFAULT_STORIES_LOCALE
import mozilla.components.service.pocket.api.Arguments.assertApiKeyHasValidStructure
import mozilla.components.service.pocket.api.Arguments.assertIsNotBlank
import mozilla.components.service.pocket.api.PocketEndpointRaw.Companion.newInstance
import mozilla.components.service.pocket.api.ext.fetchBodyOrNull

/**
 * Makes requests to the Pocket endpoint and returns the raw JSON data.
 *
 * @see [PocketEndpoint], which wraps this to make it more practical.
 * @see [newInstance] to retrieve an instance.
 */
internal class PocketEndpointRaw internal constructor(
    @VisibleForTesting internal val client: Client,
    @VisibleForTesting internal val urls: PocketURLs
) {
    /**
     * Gets the current stories recommendations from the Pocket server.
     *
     * Specifying a stories count for how many to download and an stories locale
     * is highly recommended but defaults are provided.
     *
     * @param count number of stories to download. Defaults to 3.
     * @param locale ISO-639 locale of stories to download. Defaults to "en-US".
     *
     * @return The global stories recommendations as a raw JSON string or null on error.
     */
    @WorkerThread
    fun getGlobalStoriesRecommendations(
        count: Int = DEFAULT_STORIES_COUNT,
        locale: String = DEFAULT_STORIES_LOCALE
    ): String? = makeRequest(urls.getLocaleStoriesRecommendations(count, locale))

    /**
     * @return The requested JSON as a String or null on error.
     */
    @WorkerThread // synchronous request.
    private fun makeRequest(pocketEndpoint: Uri): String? {
        val request = Request(
            url = pocketEndpoint.toString()
        )
        return client.fetchBodyOrNull(request)
    }

    companion object {

        /**
         * Returns a new instance of [PocketEndpointRaw].
         *
         * @param pocketApiKey the API key for Pocket network requests.
         * @param client the HTTP client to use for network requests.
         *
         * @throws IllegalArgumentException if the provided API key is deemed invalid.
         */
        fun newInstance(pocketApiKey: String, client: Client): PocketEndpointRaw {
            assertIsValidApiKey(
                pocketApiKey
            )

            return PocketEndpointRaw(
                client,
                PocketURLs(pocketApiKey)
            )
        }
    }
}

private fun assertIsValidApiKey(apiKey: String) {
    assertIsNotBlank(apiKey, "API key")
    assertApiKeyHasValidStructure(apiKey)
}
