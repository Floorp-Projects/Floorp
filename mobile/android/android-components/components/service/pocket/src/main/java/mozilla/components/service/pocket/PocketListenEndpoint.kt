/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

package mozilla.components.service.pocket

import androidx.annotation.WorkerThread
import mozilla.components.concept.fetch.Client
import mozilla.components.service.pocket.PocketListenEndpoint.Companion.newInstance
import mozilla.components.service.pocket.data.PocketListenArticleMetadata
import mozilla.components.service.pocket.net.PocketResponse

// See @Ignore tests in test class to do end-to-end testing.

/**
 * Makes requests to the Pocket Listen API and returns the requested data.
 *
 * @see [newInstance] to retrieve an instance.
 */
class PocketListenEndpoint internal constructor(
    private val rawEndpoint: PocketListenEndpointRaw,
    private val jsonParser: PocketListenJSONParser
) {

    /**
     * Gets a response, filled with the given article's listen metadata on success. This network call is
     * synchronous and the results are not cached.
     *
     * @return a [PocketResponse.Success] with the given article's listen metada on success or, on error, a
     * [PocketResponse.Failure].
     */
    @WorkerThread // Synchronous network call.
    fun getListenArticleMetadata(articleID: Long, articleURL: String): PocketResponse<PocketListenArticleMetadata> {
        val json = rawEndpoint.getArticleListenMetadata(articleID, articleURL)
        val metadata = json?.let { jsonParser.jsonToListenArticleMetadata(it) }
        return PocketResponse.wrap(metadata)
    }

    companion object {

        /**
         * Returns a new instance of [PocketListenEndpoint].
         *
         * @param client the HTTP client to use for network requests.
         * @param accessToken the access token for Pocket Listen network requests.
         * @param userAgent the user agent for network requests.
         *
         * @throws IllegalArgumentException if the provided access token or user agent is deemed invalid.
         */
        fun newInstance(client: Client, accessToken: String, userAgent: String): PocketListenEndpoint {
            assertIsValidAccessToken(accessToken)
            Arguments.assertIsValidUserAgent(userAgent)

            val endpoint = PocketListenEndpointRaw(client, PocketListenURLs(), userAgent, accessToken)
            return PocketListenEndpoint(endpoint, PocketListenJSONParser())
        }
    }
}

private fun assertIsValidAccessToken(accessToken: String) {
    Arguments.assertIsNotBlank(accessToken, "access token")
}
