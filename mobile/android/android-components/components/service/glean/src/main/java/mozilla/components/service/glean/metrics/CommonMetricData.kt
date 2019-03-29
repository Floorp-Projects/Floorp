/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
* file, You can obtain one at http://mozilla.org/MPL/2.0/. */

package mozilla.components.service.glean.metrics

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

    companion object {
        internal const val DEFAULT_STORAGE_NAME = "default"
    }

    fun shouldRecord(logger: Logger): Boolean {
        // Don't record metrics if we aren't initialized
        if (!Glean.isInitialized()) {
            logger.error("Glean must be initialized before metrics are recorded")
            return false
        }

        // Silently drop if metrics are turned off globally or locally
        if (!Glean.getUploadEnabled() || disabled) {
            return false
        }

        return true
    }

    /**
     * Defines the names of the storages the metric defaults to when
     * "default" is used as the destination storage.
     * Note that every metric type will need to override this.
     */
    val defaultStorageDestinations: List<String>

    /**
     * Get the list of storage names the metric will record to. This
     * automatically expands [DEFAULT_STORAGE_NAME] to the list of default
     * storages for the metric.
     */
    fun getStorageNames(): List<String> {
        if (DEFAULT_STORAGE_NAME !in sendInPings) {
            return sendInPings
        }

        val filteredNames = sendInPings.filter { it != DEFAULT_STORAGE_NAME }
        return filteredNames + defaultStorageDestinations
    }
}
