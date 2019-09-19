/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

package mozilla.components.service.glean.net

import androidx.annotation.VisibleForTesting
import mozilla.components.service.glean.BuildConfig
import mozilla.components.service.glean.config.Configuration
import mozilla.components.support.base.log.logger.Logger
import org.json.JSONException
import org.json.JSONObject
import java.text.SimpleDateFormat
import java.util.Calendar
import java.util.Locale
import java.util.TimeZone

/**
 * The logic for uploading pings: this leaves the actual upload implementation
 * to the user-provided delegate.
 */
class BaseUploader(d: PingUploader) : PingUploader by d {
    private val logger = Logger("glean/BaseUploader")

    /**
     * Log the contents of a ping to the console, if configured to do so in
     * [Configuration.logPings].
     *
     * @param path the URL path to append to the server address
     * @param data the serialized text data to send
     * @param config the Glean configuration object
     */
    private fun logPing(path: String, data: String, config: Configuration) {
        if (config.logPings) {
            // Parse and reserialize the JSON so it has indentation and is human-readable.
            try {
                val json = JSONObject(data)
                val indented = json.toString(2)

                logger.debug("Glean ping to URL: $path\n$indented")
            } catch (e: JSONException) {
                logger.debug("Exception parsing ping as JSON: $e") // $COVERAGE-IGNORE$
            }
        }
    }

    /**
     * TEST-ONLY. Allows to set specific dates for testing.
     */
    @VisibleForTesting(otherwise = VisibleForTesting.PRIVATE)
    internal fun getCalendarInstance(): Calendar { return Calendar.getInstance() }

    /**
     * Generate a date string to be used in the Date header.
     */
    private fun createDateHeaderValue(): String {
        val calendar = getCalendarInstance()
        val dateFormat = SimpleDateFormat("EEE, dd MMM yyyy HH:mm:ss z", Locale.US)
        dateFormat.timeZone = TimeZone.getTimeZone("GMT")
        return dateFormat.format(calendar.time)
    }

    /**
     * Generate a list of headers to send with the request.
     *
     * @param config the Glean configuration object
     * @return a [HeadersList] containing String to String [Pair] with the first
     *         entry being the header name and the second its value.
     */
    private fun getHeadersToSend(config: Configuration): HeadersList {
        val headers = mutableListOf(
            Pair("Content-Type", "application/json; charset=utf-8"),
            Pair("User-Agent", config.userAgent),
            Pair("Date", createDateHeaderValue()),
            // Add headers for supporting the legacy pipeline.
            Pair("X-Client-Type", "Glean"),
            Pair("X-Client-Version", BuildConfig.LIBRARY_VERSION)
        )

        // If there is a pingTag set, then this header needs to be added in order to flag pings
        // for "debug view" use.
        config.pingTag?.let {
            headers.add(Pair("X-Debug-ID", it))
        }

        return headers
    }

    /**
     * This function triggers the actual upload: logs the ping and calls the implementation
     * specific upload function.
     *
     * @param path the URL path to append to the server address
     * @param data the serialized text data to send
     * @param config the Glean configuration object
     *
     * @return true if the ping was correctly dealt with (sent successfully
     *         or faced an unrecoverable error), false if there was a recoverable
     *         error callers can deal with.
     */
    internal fun doUpload(path: String, data: String, config: Configuration): Boolean {
        logPing(path, data, config)

        return upload(
            url = config.serverEndpoint + path,
            data = data,
            headers = getHeadersToSend(config)
        )
    }
}
