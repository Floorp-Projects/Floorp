/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
* file, You can obtain one at http://mozilla.org/MPL/2.0/. */

package mozilla.components.service.glean

import mozilla.components.support.base.log.logger.Logger

enum class Lifetime {
    Ping,        // Metric is reset with each sent ping
    Application, // Metric is reset on application restart
    User         // Metric is reset with each user profile
}

/**
 * This defines the common set of data shared across all the different
 * metric types.
 */
interface CommonMetricData {
    val disabled: Boolean
    val category: String
    val lifetime: Lifetime
    val name: String
    val sendInPings: List<String>

    public fun shouldRecord(logger: Logger): Boolean {
        // Silently drop recording for disabled events.
        if (disabled) {
            return false
        }

        // TODO implement "user" metric lifetime. See bug 1499756.
        // Metrics can be recorded with application or user lifetime. For now,
        // we only support "application": metrics live as long as the application lives.
        if (lifetime != Lifetime.Application) {
            logger.error("The metric lifetime must be explicitly set.")
            return false
        }

        return true
    }
}
