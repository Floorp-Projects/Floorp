/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

package mozilla.components.service.glean.net

import androidx.annotation.VisibleForTesting
import androidx.annotation.VisibleForTesting.PRIVATE
import mozilla.components.concept.fetch.Client
import mozilla.components.concept.fetch.Header
import mozilla.components.concept.fetch.MutableHeaders
import mozilla.components.concept.fetch.Request
import mozilla.components.concept.fetch.isClientError
import mozilla.components.concept.fetch.isSuccess
import mozilla.components.support.base.log.logger.Logger
import java.io.IOException
import java.util.concurrent.TimeUnit

/**
 * A simple ping Uploader, which implements a "send once" policy, never
 * storing or attempting to send the ping again. This uses Android Component's
 * `concept-fetch`.
 */
class ConceptFetchHttpUploader(
    internal val client: Lazy<Client>
) : PingUploader {
    private val logger = Logger("glean/ConceptFetchHttpUploader")

    companion object {
        // The timeout, in milliseconds, to use when connecting to the server.
        const val DEFAULT_CONNECTION_TIMEOUT = 10000L
        // The timeout, in milliseconds, to use when reading from the server.
        const val DEFAULT_READ_TIMEOUT = 30000L
    }

    /**
     * Synchronously upload a ping to a server.
     *
     * @param url the URL path to upload the data to
     * @param data the serialized text data to send
     * @param headers a [HeadersList] containing String to String [Pair] with
     *        the first entry being the header name and the second its value.
     *
     * @return true if the ping was correctly dealt with (sent successfully
     *         or faced an unrecoverable error), false if there was a recoverable
     *         error callers can deal with.
     */
    override fun upload(url: String, data: String, headers: HeadersList): Boolean {
        val request = buildRequest(url, data, headers)

        return try {
            performUpload(client.value, request)
        } catch (e: IOException) {
            logger.warn("IOException while uploading ping", e)
            false
        }
    }

    @VisibleForTesting(otherwise = PRIVATE)
    internal fun buildRequest(
        url: String,
        data: String,
        headers: HeadersList
    ): Request {
        val conceptHeaders = MutableHeaders(headers.map { Header(it.first, it.second) })

        return Request(
            url = url,
            method = Request.Method.POST,
            connectTimeout = Pair(DEFAULT_CONNECTION_TIMEOUT, TimeUnit.MILLISECONDS),
            readTimeout = Pair(DEFAULT_READ_TIMEOUT, TimeUnit.MILLISECONDS),
            headers = conceptHeaders,
            // Make sure we are not sending cookies. Unfortunately, HttpURLConnection doesn't
            // offer a better API to do that, so we nuke all cookies going to our telemetry
            // endpoint.
            cookiePolicy = Request.CookiePolicy.OMIT,
            body = Request.Body.fromString(data)
        )
    }

    @Throws(IOException::class)
    internal fun performUpload(client: Client, request: Request): Boolean {
        logger.debug("Submitting ping to: ${request.url}")
        client.fetch(request).use { response ->
            when {
                response.isSuccess -> {
                    // Known success errors (2xx):
                    // 200 - OK. Request accepted into the pipeline.

                    // We treat all success codes as successful upload even though we only expect 200.
                    logger.debug("Ping successfully sent (${response.status})")
                    return true
                }

                response.isClientError -> {
                    // Known client (4xx) errors:
                    // 404 - not found - POST/PUT to an unknown namespace
                    // 405 - wrong request type (anything other than POST/PUT)
                    // 411 - missing content-length header
                    // 413 - request body too large (Note that if we have badly-behaved clients that
                    //       retry on 4XX, we should send back 202 on body/path too long).
                    // 414 - request path too long (See above)

                    // Something our client did is not correct. It's unlikely that the client is going
                    // to recover from this by re-trying again, so we just log and error and report a
                    // successful upload to the service.
                    logger.error("Server returned client error code ${response.status} for ${request.url}")
                    return true
                }

                else -> {
                    // Known other errors:
                    // 500 - internal error

                    // For all other errors we log a warning an try again at a later time.
                    logger.warn("Server returned response code ${response.status} for ${request.url}")
                    return false
                }
            }
        }
    }
}
