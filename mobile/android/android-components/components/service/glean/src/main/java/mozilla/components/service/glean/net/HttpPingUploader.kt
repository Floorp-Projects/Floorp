/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

package mozilla.components.service.glean.net

import android.support.annotation.VisibleForTesting
import mozilla.components.service.glean.BuildConfig
import mozilla.components.service.glean.config.Configuration
import mozilla.components.support.base.log.logger.Logger
import java.io.IOException
import java.net.CookieHandler
import java.net.CookieManager
import java.net.HttpURLConnection
import java.net.MalformedURLException
import java.net.URL
import org.json.JSONException
import org.json.JSONObject

/**
 * A simple ping Uploader, which implements a "send once" policy, never
 * storing or attempting to send the ping again.
 */
internal class HttpPingUploader(configuration: Configuration) : PingUploader {
    private val config = configuration
    private val logger = Logger("glean/HttpPingUploader")

    /**
     * Log the contents of a ping to the console, if configured to do so in
     * config.logPings.
     *
     * @param path the URL path to append to the server address
     * @param data the serialized text data to send
     */
    private fun logPing(path: String, data: String) {
        if (config.logPings) {
            // Parse and reserialize the JSON so it has indentation and is human-readable.
            var indented = try {
                var json = JSONObject(data)
                json.toString(2)
            } catch (e: JSONException) {
                logger.debug("Exception parsing ping as JSON: $e") // $COVERAGE-IGNORE$
                null // $COVERAGE-IGNORE$
            }
            indented?.let {
                logger.debug("Glean ping to URL: ${path}\n$it")
            }
        }
    }

    /**
     * Synchronously upload a ping to Mozilla servers.
     * Note that the `X-Client-Type`: `Glean` and `X-Client-Version`: <SDK version>
     * headers are added to the HTTP request in addition to the UserAgent. This allows
     * us to easily handle pings coming from glean on the legacy Mozilla pipeline.
     *
     * @param path the URL path to append to the server address
     * @param data the serialized text data to send
     *
     * @return true if the ping was correctly dealt with (sent successfully
     *         or faced an unrecoverable error), false if there was a recoverable
     *         error callers can deal with.
     */
    @Suppress("ReturnCount", "MagicNumber")
    override fun upload(path: String, data: String): Boolean {
        logPing(path, data)

        var connection: HttpURLConnection? = null
        try {
            connection = openConnection(config.serverEndpoint, path)
            connection.requestMethod = "POST"
            connection.connectTimeout = config.connectionTimeout
            connection.readTimeout = config.readTimeout
            connection.doOutput = true

            connection.setRequestProperty("Content-Type", "application/json; charset=utf-8")
            connection.setRequestProperty("User-Agent", config.userAgent)
            connection.setRequestProperty("Date", createDateHeaderValue())

            // Add headers for supporting the legacy pipeline.
            connection.setRequestProperty("X-Client-Type", "Glean")
            connection.setRequestProperty("X-Client-Version", BuildConfig.LIBRARY_VERSION)

            // Make sure we are not sending cookies. Unfortunately, HttpURLConnection doesn't
            // offer a better API to do that, so we nuke all cookies going to our telemetry
            // endpoint.
            removeCookies(config.serverEndpoint)

            // Finally upload.
            val responseCode = doUpload(connection, data)

            logger.debug("Ping upload: $responseCode")

            when (responseCode) {
                in HttpURLConnection.HTTP_OK..(HttpURLConnection.HTTP_OK + 99) -> {
                    // Known success errors (2xx):
                    // 200 - OK. Request accepted into the pipeline.

                    // We treat all success codes as successful upload even though we only expect 200.
                    logger.debug("Ping successfully sent ($responseCode)")
                    return true
                }
                in HttpURLConnection.HTTP_BAD_REQUEST..(HttpURLConnection.HTTP_BAD_REQUEST + 99) -> {
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
                    logger.error("Server returned client error code: $responseCode")
                    return true
                }
                else -> {
                    // Known other errors:
                    // 500 - internal error

                    // For all other errors we log a warning an try again at a later time.
                    logger.warn("Server returned response code: $responseCode")
                    return false
                }
            }
        } catch (e: MalformedURLException) {
            // There's nothing we can do to recover from this here. So let's just log an error and
            // notify the service that this job has been completed - even though we didn't upload
            // anything to the server.
            logger.error("Could not upload telemetry due to malformed URL", e)
            return true
        } catch (e: IOException) {
            logger.warn("IOException while uploading ping", e)
            return false
        } finally {
            connection?.disconnect()
        }
    }

    /**
     * Remove all the cookies related to the server endpoint, so
     * that nothing other than ping data travels to the endpoint.
     *
     * @param serverEndpoint the server address
     */
    internal fun removeCookies(serverEndpoint: String) {
        (CookieHandler.getDefault() as? CookieManager)?.let { cookieManager ->
            val submissionUrl = try {
                URL(serverEndpoint)
            } catch (e: MalformedURLException) {
                null
            }

            submissionUrl?.let {
                val uri = it.toURI()
                for (cookie in cookieManager.cookieStore.get(uri)) {
                    cookieManager.cookieStore.remove(uri, cookie)
                }
            }
        }
    }

    @Throws(IOException::class)
    fun doUpload(connection: HttpURLConnection, data: String): Int {
        connection.outputStream.bufferedWriter().use {
            it.write(data)
            it.flush()
        }

        return connection.responseCode
    }

    @VisibleForTesting @Throws(IOException::class)
    fun openConnection(endpoint: String, path: String): HttpURLConnection {
        return URL(endpoint + path).openConnection() as HttpURLConnection
    }
}
