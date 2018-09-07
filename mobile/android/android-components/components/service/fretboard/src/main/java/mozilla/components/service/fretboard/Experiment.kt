/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

package mozilla.components.service.fretboard

/**
 * Represents an A/B test experiment,
 * independent of the underlying
 * storage mechanism
 */
data class Experiment(
    /**
     * Unique identifier of the experiment
     */
    @Deprecated("This is an internal ID used by Kinto. Public access will be removed soon.")
    val id: String,
    /**
     * Human-readable name of the experiment
     */
    val name: String,
    /**
     * Detailed description of the experiment
     */
    val description: String? = null,
    /**
     * Filters for enabling the experiment
     */
    val match: Matcher? = null,
    /**
     * Experiment buckets
     */
    val bucket: Bucket? = null,
    /**
     * Last modified date, as a UNIX timestamp
     */
    val lastModified: Long? = null,
    /**
     * Experiment associated metadata
     */
    val payload: ExperimentPayload? = null,
    /**
     * Last time the experiment schema was modified
     * (as a UNIX timestamp)
     */
    val schema: Long? = null
) {
    data class Matcher(
        /**
         * Language of the device, as a regex
         */
        val language: String? = null,
        /**
         * id (package name) of the expected application, as a regex
         */
        val appId: String? = null,
        /**
         * Regions where the experiment should be enabled
         */
        val regions: List<String>? = null,
        /**
         * Required app version, as a regex
         */
        val version: String? = null,
        /**
         * Required device manufacturer, as a regex
         */
        val manufacturer: String? = null,
        /**
         * Required device model, as a regex
         */
        val device: String? = null,
        /**
         * Required country, as a three-letter abbreviation
         */
        val country: String? = null,
        /**
         * Required app release channel (alpha, beta, ...), as a regex
         */
        val releaseChannel: String? = null
    )

    data class Bucket(
        /**
         * Maximum bucket (exclusive), values from 0 to 100
         */
        val max: Int? = null,
        /**
         * Minimum bucket (inclusive), values from 0 to 100
         */
        val min: Int? = null
    )

    /**
     * Compares experiments by their id
     *
     * @return true if the two experiments have the same id, false otherwise
     */
    override fun equals(other: Any?): Boolean {
        if (this === other) {
            return true
        }
        if (other == null || other !is Experiment) {
            return false
        }
        return other.id == id
    }

    override fun hashCode(): Int {
        return id.hashCode()
    }
}
