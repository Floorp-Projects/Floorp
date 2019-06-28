/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

package mozilla.components.service.experiments

/**
 * Represents an A/B test experiment,
 * independent of the underlying
 * storage mechanism
 */
internal data class Experiment(
    /**
     * Unique identifier of the experiment. Used internally by Kinto. Not exposed to library consumers.
     */
    internal val id: String,
    /**
     * Detailed description of the experiment.
     */
    val description: String,
    /**
     * Last modified date, as a UNIX timestamp.
     */
    val lastModified: Long?,
    /**
     * Last time the experiment schema was modified
     * (as a UNIX timestamp).
     */
    val schemaModified: Long?,
    /**
     * Experiment buckets.
     */
    val buckets: Buckets,
    /**
     * Experiment branches.
     */
    val branches: List<Branch>,
    /**
     * Filters for enabling the experiment.
     */
    val match: Matcher
) {
    /**
     * Object containing a bucket range to match users.
     * Every user is in one of 1000 buckets (0-999).
     */
    data class Buckets(
        /**
         * The minimum bucket to match.
         */
        val start: Int,
        /**
         * The number of buckets to match from start. If (start + count >= 999), evaluation will
         * wrap around.
         */
        val count: Int
    )

    /**
     * Object containing the parameters for ratios for randomized enrollment into different
     * branches.
     */
    data class Branch(
        /**
         * The name of that branch. "control" is reserved for the control branch.
         */
        val name: String,
        /**
         * The weight to distribute enrolled clients among the branches.
         */
        val ratio: Int
    )

    data class Matcher(
        /**
         * name (package name) of the expected application, as a regex.
         */
        val appId: String?,
        /**
         * App version, as a regex
         */
        val appDisplayVersion: String?,
        /**
         * App minimum version, expected dotted numeric version E.g. 1.0.2, or 67.0.1
         */
        val appMinVersion: String?,
        /**
         * App maximum version, expected dotted numeric version E.g. 1.0.2, or 67.0.1
         */
        val appMaxVersion: String?,
        /**
         * Locale language, as a regex.
         */
        val localeLanguage: String?,
        /**
         * Locale country, as a three-letter abbreviation.
         */
        val localeCountry: String?,
        /**
         * Device manufacturer, as a regex.
         */
        val deviceManufacturer: String?,
        /**
         * Device model, as a regex.
         */
        val deviceModel: String?,
        /**
         * Regions where the experiment should be enabled.
         */
        val regions: List<String>?,
        /**
         * Debug tags for matching QA devices.
         */
        val debugTags: List<String>?
    )

    /**
     * Compares experiments by their id.
     *
     * @return true if the two experiments have the same id, false otherwise.
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
