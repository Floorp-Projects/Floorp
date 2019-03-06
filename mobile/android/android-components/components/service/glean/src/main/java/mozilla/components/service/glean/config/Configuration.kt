/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

package mozilla.components.service.glean.config

import mozilla.components.concept.fetch.Client
import mozilla.components.lib.fetch.httpurlconnection.HttpURLConnectionClient
import mozilla.components.service.glean.BuildConfig

/**
 * The Configuration class describes how to configure the Glean.
 *
 * @property serverEndpoint the server pings are sent to
 * @property userAgent the user agent used when sending pings
 * @property connectionTimeout the timeout, in milliseconds, to use when connecting to
 *           the [serverEndpoint]
 * @property readTimeout the timeout, in milliseconds, to use when connecting to
 *           the [serverEndpoint]
 * @property maxEvents the number of events to store before the events ping is sent
 * @property logPings whether to log ping contents to the console.
 * @property httpClient The HTTP client implementation to use for uploading pings.
 */
data class Configuration(
    val serverEndpoint: String = "https://incoming.telemetry.mozilla.org",
    val userAgent: String = "Glean/${BuildConfig.LIBRARY_VERSION} (Android)",
    val connectionTimeout: Long = 10000,
    val readTimeout: Long = 30000,
    val maxEvents: Int = 500,
    val logPings: Boolean = false,
    val httpClient: Client = HttpURLConnectionClient()
)
