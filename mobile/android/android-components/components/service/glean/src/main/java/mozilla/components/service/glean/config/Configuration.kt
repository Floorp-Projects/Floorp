/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

package mozilla.components.service.glean.config

import mozilla.components.service.glean.BuildConfig

/**
 * The Configuration class describes how to configure the Glean.
 *
 * @property applicationId a unique identifier for the application (e.g. "reference-browser")
 * @property serverEndpoint the server pings are sent to
 * @property userAgent the user agent used when sending pings
 * @property connectionTimeout the timeout, in milliseconds, to use when connecting to
 *           the [serverEndpoint]
 * @property readTimeout the timeout, in milliseconds, to use when connecting to
 *           the [serverEndpoint]
 */
data class Configuration(
    val applicationId: String,
    val serverEndpoint: String = "https://incoming.telemetry.mozilla.org",
    val userAgent: String = "Glean/${BuildConfig.LIBRARY_VERSION} (Android)",
    val connectionTimeout: Int = 10000,
    val readTimeout: Int = 30000
)
