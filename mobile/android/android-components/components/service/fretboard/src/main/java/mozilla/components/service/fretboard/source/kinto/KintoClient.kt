/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

package mozilla.components.service.fretboard.source.kinto

import java.net.URL

/**
 * Helper class to make it easier to interact with Kinto
 *
 * @property httpClient http client to use
 * @property baseUrl Kinto server url
 * @property bucketName name of the bucket to fetch
 * @property collectionName name of the collection to fetch
 * @property headers headers to provide along with the request
 */
internal class KintoClient(
    private val httpClient: HttpClient = HttpURLConnectionHttpClient(),
    private val baseUrl: String,
    private val bucketName: String,
    private val collectionName: String,
    private val headers: Map<String, String>? = null
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
     * @param lastModified last modified time as a UNIX timestamp
     *
     * @return Kinto diff response
     */
    fun diff(lastModified: Long): String {
        return httpClient.get(URL("${recordsUrl()}?_since=$lastModified"), headers)
    }

    /**
     * Gets the collection associated metadata
     *
     * @return collection metadata
     */
    fun getMetadata(): String {
        return httpClient.get(URL(collectionUrl()))
    }

    private fun recordsUrl() = "${collectionUrl()}/records"
    private fun collectionUrl() = "$baseUrl/buckets/$bucketName/collections/$collectionName"
}
