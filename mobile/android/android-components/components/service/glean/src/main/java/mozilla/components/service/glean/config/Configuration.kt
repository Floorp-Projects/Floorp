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
 * @property serverEndpoint the server pings are sent to. Please note that this is
 *           is only meant to be changed for tests.
 * @property userAgent the user agent used when sending pings, only to be used internally.
 * @property connectionTimeout the timeout, in milliseconds, to use when connecting to
 *           the [serverEndpoint]
 * @property readTimeout the timeout, in milliseconds, to use when connecting to
 *           the [serverEndpoint]
 * @property maxEvents the number of events to store before the events ping is sent
 * @property logPings whether to log ping contents to the console. This is only meant to be used
 *           internally by the `GleanDebugActivity`.
 * @property httpClient The HTTP client implementation to use for uploading pings.
 * @property pingTag String tag to be applied to headers when uploading pings for debug view.
 *           This is only meant to be used internally by the `GleanDebugActivity`.
 * @property channel the release channel the application is on, if known. This will be
 *           sent along with all the pings, in the `client_info` section. */
data class Configuration internal constructor(
    val serverEndpoint: String,
    val userAgent: String = DEFAULT_USER_AGENT,
    val connectionTimeout: Long = DEFAULT_CONNECTION_TIMEOUT,
    val readTimeout: Long = DEFAULT_READ_TIMEOUT,
    val maxEvents: Int = DEFAULT_MAX_EVENTS,
    val logPings: Boolean = DEFAULT_LOG_PINGS,
    // NOTE: since only simple object or strings can be made `const val`s, if the
    // default values for the lines below are ever changed, they are required
    // to change in the public constructor below.
    val httpClient: Lazy<Client> = lazy { HttpURLConnectionClient() },
    val pingTag: String? = null,
    val channel: String? = null
) {
    // This is the only public constructor this class should have. It should only
    // expose things we want to allow external applications to change. Every test
    // only or internal configuration option should be added to the above primary internal
    // constructor and only initialized with a proper default when calling the primary
    // constructor from the secondary, public one, below.
    constructor(
        serverEndpoint: String = DEFAULT_TELEMETRY_ENDPOINT,
        connectionTimeout: Long = DEFAULT_CONNECTION_TIMEOUT,
        readTimeout: Long = DEFAULT_READ_TIMEOUT,
        maxEvents: Int = DEFAULT_MAX_EVENTS,
        httpClient: Lazy<Client> = lazy { HttpURLConnectionClient() },
        channel: String? = null
    ) : this (
        serverEndpoint = serverEndpoint,
        userAgent = DEFAULT_USER_AGENT,
        connectionTimeout = connectionTimeout,
        readTimeout = readTimeout,
        maxEvents = maxEvents,
        logPings = DEFAULT_LOG_PINGS,
        httpClient = httpClient,
        pingTag = null,
        channel = channel
    )

    companion object {
        const val DEFAULT_TELEMETRY_ENDPOINT = "https://incoming.telemetry.mozilla.org"
        const val DEFAULT_USER_AGENT = "Glean/${BuildConfig.LIBRARY_VERSION} (Android)"
        const val DEFAULT_CONNECTION_TIMEOUT = 10000L
        const val DEFAULT_READ_TIMEOUT = 30000L
        const val DEFAULT_MAX_EVENTS = 500
        const val DEFAULT_LOG_PINGS = false
    }
}
