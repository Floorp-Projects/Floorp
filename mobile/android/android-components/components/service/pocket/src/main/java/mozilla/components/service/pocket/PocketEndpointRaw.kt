/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

package mozilla.components.service.pocket

import android.net.Uri
import androidx.annotation.WorkerThread
import mozilla.components.concept.fetch.Client
import mozilla.components.concept.fetch.Headers.Names.USER_AGENT
import mozilla.components.concept.fetch.MutableHeaders
import mozilla.components.concept.fetch.Request
import mozilla.components.service.pocket.Arguments.assertIsNotBlank
import mozilla.components.service.pocket.ext.fetchBodyOrNull

/**
 * Make requests to the Pocket endpoint and returns the raw JSON data: this class is intended to be very dumb.
 *
 * @see [PocketEndpoint], which wraps this to make it more practical.
 * @see [newInstance] to retrieve an instance.
 */
class PocketEndpointRaw internal constructor(
    private val client: Client,
    private val urls: PocketURLs,
    private val userAgent: String
) {

    /**
     * @return The global video recommendations as a raw JSON string or null on error.
     */
    @WorkerThread // synchronous request.
    fun getGlobalVideoRecommendations(): String? = makeRequest(urls.globalVideoRecs)

    /**
     * @return The requested JSON as a String or null on error.
     */
    @WorkerThread // synchronous request.
    private fun makeRequest(pocketEndpoint: Uri): String? {
        val request = Request(
            url = pocketEndpoint.toString(),
            headers = MutableHeaders(USER_AGENT to userAgent)
        )
        return client.fetchBodyOrNull(request)
    }

    companion object {

        /**
         * Returns a new instance of [PocketEndpointRaw].
         *
         * @param client the HTTP client to use for network requests.
         * @param pocketApiKey the API key for Pocket network requests.
         * @param userAgent the user agent for network requests.
         *
         * @throws IllegalArgumentException if the provided API key or user agent is deemed invalid.
         */
        fun newInstance(client: Client, pocketApiKey: String, userAgent: String): PocketEndpointRaw {
            assertIsValidApiKey(pocketApiKey)
            Arguments.assertIsValidUserAgent(userAgent)

            return PocketEndpointRaw(client, PocketURLs(pocketApiKey), userAgent)
        }
    }
}

private fun assertIsValidApiKey(apiKey: String) {
    assertIsNotBlank(apiKey, "API key")
}
