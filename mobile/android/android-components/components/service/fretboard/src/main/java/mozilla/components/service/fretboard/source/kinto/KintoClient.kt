/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

package mozilla.components.service.fretboard.source.kinto

import mozilla.components.concept.fetch.Client
import mozilla.components.concept.fetch.MutableHeaders
import mozilla.components.concept.fetch.Request
import mozilla.components.concept.fetch.isSuccess
import mozilla.components.service.fretboard.ExperimentDownloadException
import java.io.IOException

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
    private val httpClient: Client,
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
        return fetch(recordsUrl())
    }

    /**
     * Performs a diff, given the last_modified time
     *
     * @param lastModified last modified time as a UNIX timestamp
     *
     * @return Kinto diff response
     */
    fun diff(lastModified: Long): String {
        return fetch("${recordsUrl()}?_since=$lastModified")
    }

    /**
     * Gets the collection associated metadata
     *
     * @return collection metadata
     */
    fun getMetadata(): String {
        return fetch(collectionUrl())
    }

    @Suppress("TooGenericExceptionCaught", "ThrowsCount")
    internal fun fetch(url: String): String {
        try {
            val headers = MutableHeaders().also {
                headers?.forEach { (k, v) -> it.append(k, v) }
            }

            val request = Request(url, headers = headers, useCaches = false)
            val response = httpClient.fetch(request)
            if (!response.isSuccess) {
                throw ExperimentDownloadException("Status code: ${response.status}")
            }
            return response.body.string()
        } catch (e: IOException) {
            throw ExperimentDownloadException(e)
        } catch (e: ArrayIndexOutOfBoundsException) {
            // On some devices we are seeing an ArrayIndexOutOfBoundsException being thrown
            // somewhere inside AOSP/okhttp.
            // See: https://github.com/mozilla-mobile/android-components/issues/964
            throw ExperimentDownloadException(e)
        }
    }

    private fun recordsUrl() = "${collectionUrl()}/records"
    private fun collectionUrl() = "$baseUrl/buckets/$bucketName/collections/$collectionName"
}
