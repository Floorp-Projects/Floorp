/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

package mozilla.components.service.pocket

import android.net.Uri
import android.support.annotation.VisibleForTesting
import android.support.annotation.VisibleForTesting.PRIVATE
import android.support.annotation.WorkerThread
import mozilla.components.concept.fetch.Client
import mozilla.components.concept.fetch.MutableHeaders
import mozilla.components.concept.fetch.Request
import mozilla.components.concept.fetch.Response
import mozilla.components.concept.fetch.isSuccess
import java.io.IOException

/**
 * Make requests to the Pocket endpoint and returns the raw JSON data: this class is intended to be very dumb.
 *
 * @see [PocketEndpoint], which wraps this to make it more practical.
 */
internal class PocketEndpointRaw(
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
            headers = MutableHeaders(HEADER_USER_AGENT to userAgent)
        )

        val response: Response? = try {
            client.fetch(request)
        } catch (e: IOException) {
            logger.debug("getGlobalVideoRecommedations: network error", e)
            null
        }

        return response?.use { if (response.isSuccess) response.body.string() else null }
    }

    companion object {
        @VisibleForTesting(otherwise = PRIVATE) const val HEADER_USER_AGENT = "User-Agent"
    }
}
