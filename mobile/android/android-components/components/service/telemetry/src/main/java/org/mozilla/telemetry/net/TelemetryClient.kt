/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

package org.mozilla.telemetry.net

import androidx.annotation.VisibleForTesting
import mozilla.components.concept.fetch.Client
import mozilla.components.concept.fetch.MutableHeaders
import mozilla.components.concept.fetch.Request
import mozilla.components.concept.fetch.Response
import mozilla.components.support.base.log.logger.Logger
import org.mozilla.telemetry.config.TelemetryConfiguration
import java.io.IOException
import java.text.SimpleDateFormat
import java.util.Calendar
import java.util.Locale
import java.util.TimeZone
import java.util.concurrent.TimeUnit

class TelemetryClient(
    private val client: Client
) {
    private val logger = Logger("telemetry/client")

    fun uploadPing(configuration: TelemetryConfiguration, path: String, serializedPing: String): Boolean {
        val request = Request(
            url = configuration.serverEndpoint + path,
            method = Request.Method.POST,
            connectTimeout = Pair(configuration.connectTimeout.toLong(), TimeUnit.MILLISECONDS),
            readTimeout = Pair(configuration.readTimeout.toLong(), TimeUnit.MILLISECONDS),
            headers = MutableHeaders(
                "Content-Type" to "application/json; charset=utf-8",
                "User-Agent" to configuration.userAgent,
                "Date" to createDateHeaderValue()
            ),
            body = Request.Body.fromString(serializedPing))

        val status = try {
            client.fetch(request).use { response -> response.status }
        } catch (e: IOException) {
            logger.warn("IOException while uploading ping", e)
            return false
        }

        logger.debug("Ping upload: $status")

        return when (status) {
            in Response.SUCCESS_STATUS_RANGE -> {
                // Known success errors (2xx):
                // 200 - OK. Request accepted into the pipeline.

                // We treat all success codes as successful upload even though we only expect 200.
                true
            }

            in Response.CLIENT_ERROR_STATUS_RANGE -> {
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
                logger.error("Server returned client error code: $status")
                true
            }

            else -> {
                // Known other errors:
                // 500 - internal error

                // For all other errors we log a warning an try again at a later time.
                logger.warn("Server returned response code: $status")
                false
            }
        }
    }

    @VisibleForTesting
    internal fun createDateHeaderValue(): String {
        val calendar = Calendar.getInstance()
        val dateFormat = SimpleDateFormat("EEE, dd MMM yyyy HH:mm:ss z", Locale.US)
        dateFormat.timeZone = TimeZone.getTimeZone("GMT")
        return dateFormat.format(calendar.time)
    }
}
