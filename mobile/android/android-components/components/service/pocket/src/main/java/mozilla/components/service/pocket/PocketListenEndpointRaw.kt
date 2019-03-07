/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

package mozilla.components.service.pocket

import androidx.annotation.VisibleForTesting
import androidx.annotation.VisibleForTesting.PRIVATE
import androidx.annotation.WorkerThread
import mozilla.components.concept.fetch.Client
import mozilla.components.concept.fetch.Headers.Names.CONTENT_TYPE
import mozilla.components.concept.fetch.Headers.Names.USER_AGENT
import mozilla.components.concept.fetch.Headers.Values.CONTENT_TYPE_FORM_URLENCODED
import mozilla.components.concept.fetch.MutableHeaders
import mozilla.components.concept.fetch.Request
import mozilla.components.service.pocket.ext.fetchBodyOrNull

/**
 * Makes requests to the Pocket Listen endpoint and returns the raw JSON data: this class is intended to be very dumb.
 *
 * @see [PocketListenEndpoint], which wraps this to make it more practical.
 */
internal class PocketListenEndpointRaw(
    private val client: Client,
    private val urls: PocketListenURLs,
    private val userAgent: String,
    private val accessToken: String
) {

    /**
     * @return the metadata for the given article as a raw JSON string or null on error.
     */
    @WorkerThread // Synchronous network call.
    fun getArticleListenMetadata(articleID: Long, articleUrl: String): String? {
        val request = Request(
            urls.articleService.toString(),
            Request.Method.POST,
            MutableHeaders(
                USER_AGENT to userAgent,
                CONTENT_TYPE to CONTENT_TYPE_FORM_URLENCODED,
                X_ACCESS_TOKEN to accessToken
            ),
            body = Request.Body.fromParamsForFormUrlEncoded(
                "url" to articleUrl,
                "article_id" to articleID.toString(),
                "v" to "2",
                "locale" to "en-US"
            )
        )

        // If an invalid body is sent to the server, 404 is returned.
        return client.fetchBodyOrNull(request)
    }

    companion object {
        @VisibleForTesting(otherwise = PRIVATE) const val X_ACCESS_TOKEN = "x-access-token"
    }
}
