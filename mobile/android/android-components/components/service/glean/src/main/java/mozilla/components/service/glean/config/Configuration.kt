/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

package mozilla.components.service.glean.config

import mozilla.telemetry.glean.net.PingUploader
import mozilla.telemetry.glean.config.Configuration as GleanCoreConfiguration

/**
 * The Configuration class describes how to configure the Glean.
 *
 * @property httpClient The HTTP client implementation to use for uploading pings.
 *                      If you don't provide your own networking stack with an HTTP client to use,
 *                      you can fall back to a simple implementation on top of `java.net` using
 *                      `ConceptFetchHttpUploader(lazy { HttpURLConnectionClient() as Client })`
 * @property serverEndpoint (optional) the server pings are sent to. Please note that this is
 *           is only meant to be changed for tests.
 * @property channel (optional )the release channel the application is on, if known. This will be
 *           sent along with all the pings, in the `client_info` section.
 * @property maxEvents (optional) the number of events to store before the events ping is sent
 */
data class Configuration @JvmOverloads constructor(
    val httpClient: PingUploader,
    val serverEndpoint: String = DEFAULT_TELEMETRY_ENDPOINT,
    val channel: String? = null,
    val maxEvents: Int? = null,
) {
    // The following is required to support calling our API from Java.
    companion object {
        const val DEFAULT_TELEMETRY_ENDPOINT = GleanCoreConfiguration.DEFAULT_TELEMETRY_ENDPOINT
    }

    /**
     * Convert the Android Components configuration object to the Glean SDK
     * configuration object.
     *
     * @return a [mozilla.telemetry.glean.config.Configuration] instance.
     */
    fun toWrappedConfiguration(): GleanCoreConfiguration {
        return GleanCoreConfiguration(serverEndpoint, channel, maxEvents, httpClient)
    }
}
