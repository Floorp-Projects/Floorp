/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

package mozilla.components.service.pocket

import android.net.Uri
import android.support.annotation.VisibleForTesting
import android.support.annotation.VisibleForTesting.PRIVATE
import android.support.annotation.WorkerThread
import mozilla.components.concept.fetch.Client
import mozilla.components.concept.fetch.Headers.Common.CONTENT_TYPE
import mozilla.components.concept.fetch.Headers.Common.USER_AGENT
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
    fun getArticleListenMetadata(articleID: Int, articleUrl: String): String? {
        /** @return "key=value&key2=value2..." for the hard-coded key-value pairs. */
        fun getRequestBodyString(): String = Uri.Builder()
            .appendQueryParameter("url", articleUrl)
            .appendQueryParameter("article_id", articleID.toString())
            .appendQueryParameter("v", "2")
            .appendQueryParameter("locale", "en-US")
            .build()
            .encodedQuery!! // We added query params so this should be non-null: assert it.

        val request = Request(
            urls.articleService.toString(),
            Request.Method.POST,
            MutableHeaders(
                USER_AGENT to userAgent,
                CONTENT_TYPE to CONTENT_TYPE_URL_ENCODED,
                X_ACCESS_TOKEN to accessToken
            ),
            body = Request.Body(getRequestBodyString().byteInputStream())
        )

        // If an invalid body is sent to the server, 404 is returned.
        return client.fetchBodyOrNull("getArticleListenMetadata", request)
    }

    companion object {
        @VisibleForTesting(otherwise = PRIVATE) const val X_ACCESS_TOKEN = "x-access-token"
        @VisibleForTesting(otherwise = PRIVATE) const val CONTENT_TYPE_URL_ENCODED = "application/x-www-form-urlencoded"
    }
}
