/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

package mozilla.components.service.glean.private

import mozilla.components.service.glean.Glean
import mozilla.components.support.base.log.logger.Logger

/**
 * Enumeration of different metric lifetimes.
 */
enum class Lifetime {
    /**
     * The metric is reset with each sent ping
     */
    Ping,
    /**
     * The metric is reset on application restart
     */
    Application,
    /**
     * The metric is reset with each user profile
     */
    User
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

    val identifier: String get() = if (category.isEmpty()) { name } else { "$category.$name" }

    fun shouldRecord(logger: Logger): Boolean {
        // Silently drop if metrics are turned off globally or locally
        if (!Glean.getUploadEnabled() || disabled) {
            return false
        }

        return true
    }
}
