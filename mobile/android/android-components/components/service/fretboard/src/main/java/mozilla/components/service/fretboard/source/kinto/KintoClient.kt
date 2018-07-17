/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

package mozilla.components.service.fretboard.source.kinto

import java.net.URL

/**
 * Helper class to make it easier to interact with Kinto
 *
 * @param httpClient http client to use
 * @param baseUrl Kinto server url
 * @param bucketName name of the bucket to fetch
 * @param collectionName name of the collection to fetch
 * @param headers headers to provide along with the request
 */
internal class KintoClient(
        private val httpClient: HttpClient = HttpURLConnectionHttpClient(),
        val baseUrl: String,
        val bucketName: String,
        val collectionName: String,
        val headers: Map<String, String>? = null
) {

    /**
     * Returns all records from the collection
     *
     * @return Kinto response with all records
     */
    fun get(): String {
        return httpClient.get(URL(recordsUrl()), headers)
    }

    /**
     * Performs a diff, given the last_modified time
     *
     * @param lastModified last modified time
     * @return Kinto diff response
     */
    fun diff(lastModified: Long): String {
        return httpClient.get(URL("${recordsUrl()}?_since=$lastModified"), headers)
    }

    private fun recordsUrl() = "$baseUrl/buckets/$bucketName/collections/$collectionName/records"
}
