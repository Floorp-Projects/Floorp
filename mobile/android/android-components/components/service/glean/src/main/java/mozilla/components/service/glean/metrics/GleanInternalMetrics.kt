/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

package mozilla.components.service.glean.metrics

import mozilla.components.service.glean.Lifetime
import mozilla.components.service.glean.UuidMetricType

/**
 * This class holds the set of core metrics handled by the Glean library.
 * This metrics are not meant to be exposed to the outside: their values
 * are only relevant to Glean, updated and fetched when needed.
 *
 * Ideally, this should be generated from an internal metrics.yaml file.
 * Bug 1507743 is for dealing with that.
 */
class GleanInternalMetrics {
    val clientId: UuidMetricType by lazy {
        UuidMetricType(
            name = "client_id",
            category = "",
            sendInPings = listOf("glean_ping_info"),
            lifetime = Lifetime.User,
            disabled = false
        )
    }
}
