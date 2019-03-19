/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

package mozilla.components.service.pocket

import android.net.Uri
import android.support.annotation.WorkerThread
import mozilla.components.concept.fetch.Client
import mozilla.components.concept.fetch.Headers.Names.USER_AGENT
import mozilla.components.concept.fetch.MutableHeaders
import mozilla.components.concept.fetch.Request
import mozilla.components.service.pocket.ext.fetchBodyOrNull

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
            headers = MutableHeaders(USER_AGENT to userAgent)
        )
        return client.fetchBodyOrNull(request)
    }
}
