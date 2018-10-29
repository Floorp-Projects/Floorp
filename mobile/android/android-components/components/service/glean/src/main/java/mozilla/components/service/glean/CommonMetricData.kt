/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
* file, You can obtain one at http://mozilla.org/MPL/2.0/. */

package mozilla.components.service.glean

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

    companion object {
        internal const val DEFAULT_STORAGE_NAME = "default"
    }

    fun shouldRecord(logger: Logger): Boolean {
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

    /**
     * Defines the names of the storages the metric defaults to when
     * "default" is used as the destination storage.
     * Note that every metric type will need to implement this.
     */
    fun defaultStorageDestinations(): List<String>

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
        return filteredNames + defaultStorageDestinations()
    }
}
